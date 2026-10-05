#include "services/fs_shared_archive.h"
#include <array>
#include <cerrno>
#include <stdexcept>
#include <vector>
#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#define LEGO_HAS_CONTAINED_FILE_CREATE 1
#endif
#include <limits>
#include <string>

namespace lego::ctr {
namespace fs = std::filesystem;
struct SharedArchiveMounts::NativeRoot {
    int fd{-1};
    explicit NativeRoot(int value) : fd(value) {}
    ~NativeRoot() {
#ifdef LEGO_HAS_CONTAINED_FILE_CREATE
        if (fd >= 0) ::close(fd);
#endif
    }
    NativeRoot(const NativeRoot&) = delete;
    NativeRoot& operator=(const NativeRoot&) = delete;
};
namespace {
std::string HexId(std::uint32_t value) {
    constexpr char digits[] = "0123456789ABCDEF";
    std::string out(8, '0');
    for (unsigned i = 0; i < 8; ++i) out[7-i] = digits[(value >> (4*i)) & 15];
    return out;
}
Result CheckDirectory(const fs::path& path) {
    std::error_code ec;
    const auto status = fs::symlink_status(path, ec);
    if (ec == std::errc::no_such_file_or_directory || status.type() == fs::file_type::not_found)
        return kResultFsNotFormatted;
    // No symlink following inside the selected mount. This is host containment
    // policy, not a claim about NAND's on-device directory representation.
    if (ec || !fs::is_directory(status) || fs::is_symlink(status))
        return kResultFsInvalidPath;
    return kResultSuccess;
}
} // namespace

bool SharedArchiveMounts::ConfigureRoot(const fs::path& root) {
    if (root.empty()) return false;
    std::error_code ec;
    const auto canonical = fs::canonical(root, ec);
    if (ec || CheckDirectory(canonical) != kResultSuccess) return false;
    if (configured()) return canonical == root_;
#ifdef LEGO_HAS_CONTAINED_FILE_CREATE
    // Pin the host-selected root. Subsequent opens never resolve its pathname
    // again, so replacing it with a symlink cannot redirect guest writes.
    const int fd = ::open(canonical.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return false;
    try {
        native_root_ = std::make_shared<NativeRoot>(fd);
    } catch (...) {
        ::close(fd);
        throw;
    }
#endif
    root_ = canonical;
    return true;
}

const fs::path* SharedArchiveMounts::Find(std::uint64_t handle) const noexcept {
    const auto it = mounts_.find(handle);
    return it == mounts_.end() ? nullptr : &it->second;
}

Result SharedArchiveMounts::Open(std::uint32_t low_id, std::uint64_t* out_handle) {
    if (!configured() || out_handle == nullptr) return kResultInvalidPointer;
    auto path = root_;
    auto result = CheckDirectory(path);
    if (result != kResultSuccess) return result;
    // Pinned FS overwrites the caller's shared-extdata high ID with 0x48000.
    for (const auto& part : std::array<std::string,3>{"00048000", HexId(low_id), "user"}) {
        path /= part;
        result = CheckDirectory(path);
        if (result != kResultSuccess) return result;
    }
    if (next_handle_ == std::numeric_limits<std::uint64_t>::max())
        return kResultOutOfHandles; // Host exhaustion guard, not hardware limit.
    const auto handle = next_handle_;
    mounts_.emplace(handle, std::move(path));
    ++next_handle_;
    *out_handle = handle;
    return kResultSuccess;
}

namespace {
// Decode only well-formed UTF-16. Embedded NUL, host-special characters, dot
// components and empty components are rejected, not normalized through the host.
// The stricter dot/empty/control policy is containment, not firmware parity.
bool PathComponents(std::u16string_view path, std::vector<std::string>& out) {
    if (path.size() < 2 || path.front() != u'/') return false;
    std::string component;
    for (std::size_t i = 1; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == u'/') {
            if (component.empty() || component == "." || component == ".." || component.size() > 255)
                return false;
            out.push_back(std::move(component));
            component.clear();
            continue;
        }
        std::uint32_t cp = path[i];
        if (cp >= 0xD800 && cp <= 0xDBFF) {
            if (++i >= path.size() || path[i] < 0xDC00 || path[i] > 0xDFFF) return false;
            cp = 0x10000 + ((cp - 0xD800) << 10) + (path[i] - 0xDC00);
        } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
            return false;
        }
        if (cp < 0x20 || cp == 0x7F || cp == '<' || cp == '>' || cp == '\\' ||
            cp == '|' || cp == ':' || cp == '"' || cp == '*' || cp == '?') return false;
        if (cp < 0x80) component += static_cast<char>(cp);
        else if (cp < 0x800) {
            component += static_cast<char>(0xC0 | (cp >> 6));
            component += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            component += static_cast<char>(0xE0 | (cp >> 12));
            component += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            component += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            component += static_cast<char>(0xF0 | (cp >> 18));
            component += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            component += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            component += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
    return true;
}
#ifdef LEGO_HAS_CONTAINED_FILE_CREATE
class ScopedFd {
public:
    explicit ScopedFd(int value) : fd_(value) {}
    ~ScopedFd() { if (fd_ >= 0) ::close(fd_); }
    ScopedFd(const ScopedFd&) = delete;
    ScopedFd& operator=(const ScopedFd&) = delete;
    int get() const noexcept { return fd_; }
    void reset(int value) { if (fd_ >= 0) ::close(fd_); fd_ = value; }
private:
    int fd_;
};
// Unknown host failures are not invented guest Results or success. FS catches
// these and requests an explicit host stop; a failed resize may leave the new file.
[[noreturn]] void HostFileError(const char* operation) {
    throw std::system_error(errno, std::generic_category(), operation);
}
Result Descend(ScopedFd& current, const std::string& component) {
    const int next = ::openat(current.get(), component.c_str(),
                              O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (next >= 0) { current.reset(next); return kResultSuccess; }
    const int saved = errno;
    struct stat st{};
    if (::fstatat(current.get(), component.c_str(), &st, AT_SYMLINK_NOFOLLOW) == 0) {
        if (S_ISLNK(st.st_mode)) return kResultFsInvalidPath;
        if (!S_ISDIR(st.st_mode)) return kResultFsUnexpectedFileOrDirectory;
    }
    if (saved == ENOENT) return kResultFsPathNotFound;
    if (saved == ELOOP) return kResultFsInvalidPath;
    errno = saved;
    HostFileError("shared-extdata directory open");
}
#endif
} // namespace

bool SharedArchiveMounts::CanCreateFiles() const noexcept {
#ifdef LEGO_HAS_CONTAINED_FILE_CREATE
    return native_root_ && native_root_->fd >= 0;
#else
    return false; // No unsafe std::fstream fallback on unsupported hosts.
#endif
}

Result SharedArchiveMounts::CreateFile(std::uint64_t handle, std::u16string_view path,
                                       std::uint64_t size) const {
    const auto* mount = Find(handle);
    if (!mount) return kResultFsInvalidArchiveHandle;
    if (!CanCreateFiles() || size > kMaxSharedCreateSize)
        throw std::logic_error("unsupported shared-extdata CreateFile backend/size");
    std::vector<std::string> parts;
    if (!PathComponents(path, parts)) return kResultFsInvalidPath;
#ifdef LEGO_HAS_CONTAINED_FILE_CREATE
    ScopedFd parent(::fcntl(native_root_->fd, F_DUPFD_CLOEXEC, 0));
    if (parent.get() < 0) HostFileError("shared-extdata root duplication");
    // Reopen every numeric archive component and guest parent with no-follow
    // directory-relative opens. Never trust the earlier path-only mount check.
    for (const auto& part : mount->lexically_relative(root_)) {
        const Result result = Descend(parent, part.string());
        if (result != kResultSuccess) return result;
    }
    for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
        const Result result = Descend(parent, parts[i]);
        if (result != kResultSuccess) return result;
    }
    const auto& leaf = parts.back();
    struct stat existing{};
    if (::fstatat(parent.get(), leaf.c_str(), &existing, AT_SYMLINK_NOFOLLOW) == 0)
        return S_ISLNK(existing.st_mode) ? kResultFsInvalidPath : kResultFsFileAlreadyExists;
    if (errno != ENOENT) HostFileError("shared-extdata destination check");
    if (size == 0) return kResultFsUnsupportedOpenFlags;
    ScopedFd file(::openat(parent.get(), leaf.c_str(),
                          O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600));
    if (file.get() < 0) {
        // O_EXCL protects an existing file even if it appeared after fstatat.
        if (errno == EEXIST) return kResultFsFileAlreadyExists;
        if (errno == ELOOP) return kResultFsInvalidPath;
        if (errno == ENOENT) return kResultFsPathNotFound;
        HostFileError("shared-extdata exclusive create");
    }
    // Sparse zero extent, matching reference creation semantics. No save header,
    // Play Coin balance or application content is seeded by the host.
    if (::ftruncate(file.get(), static_cast<off_t>(size)) != 0)
        HostFileError("shared-extdata resize (new file may remain)");
    return kResultSuccess;
#else
    throw std::logic_error("contained file creation is unavailable on this host");
#endif
}
} // namespace lego::ctr
