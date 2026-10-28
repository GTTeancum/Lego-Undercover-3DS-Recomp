#include "services/fs_save_archive.h"
#include <array>
#include <cerrno>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#define LEGO_HAS_CONTAINED_SAVE_OPEN 1
#endif

namespace lego::ctr {
namespace fs = std::filesystem;
struct SaveDataArchiveMounts::NativeDirectory {
    int fd;
    explicit NativeDirectory(int value) noexcept : fd(value) {}
    ~NativeDirectory() {
#ifdef LEGO_HAS_CONTAINED_SAVE_OPEN
        if (fd >= 0) ::close(fd);
#endif
    }
    NativeDirectory(const NativeDirectory&) = delete;
    NativeDirectory& operator=(const NativeDirectory&) = delete;
};

namespace {
std::string HexProgramPart(std::uint32_t value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string out(8, '0');
    for (unsigned i = 0; i < 8; ++i) out[7-i] = digits[(value >> (4*i)) & 15];
    return out;
}
#ifdef LEGO_HAS_CONTAINED_SAVE_OPEN
class ScopedDirectory {
public:
    explicit ScopedDirectory(int value) noexcept : fd_(value) {}
    ~ScopedDirectory() { if (fd_ >= 0) ::close(fd_); }
    ScopedDirectory(const ScopedDirectory&) = delete;
    ScopedDirectory& operator=(const ScopedDirectory&) = delete;
    [[nodiscard]] int get() const noexcept { return fd_; }
    int release() noexcept { const int out=fd_; fd_=-1; return out; }
    void reset(int value) noexcept { if (fd_ >= 0) ::close(fd_); fd_=value; }
private:
    int fd_;
};
[[noreturn]] void SaveHostError(int error, const char* operation) {
    throw std::system_error(error, std::generic_category(), operation);
}
Result DescendSaveDirectory(ScopedDirectory& current, const std::string& component) {
    const int next = ::openat(current.get(), component.c_str(),
                              O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (next >= 0) { current.reset(next); return kResultSuccess; }
    const int error = errno;
    // Inspect the entry without following a symlink. A missing numeric path is
    // an unformatted title; other host failures do not masquerade as that case.
    struct stat entry{};
    if (::fstatat(current.get(), component.c_str(), &entry, AT_SYMLINK_NOFOLLOW) == 0) {
        if (S_ISLNK(entry.st_mode) || !S_ISDIR(entry.st_mode)) return kResultFsInvalidPath;
    }
    if (error == ENOENT) return kResultFsNotFormatted;
    if (error == ELOOP || error == ENOTDIR) return kResultFsInvalidPath;
    SaveHostError(error, "SaveData directory open");
}
#endif
} // namespace

bool SaveDataArchiveMounts::ConfigureRoot(const fs::path& root) {
    if (root.empty()) return false;
    std::error_code error;
    auto canonical = fs::canonical(root, error);
    if (error || !fs::is_directory(canonical, error) || error) return false;
    if (configured()) return canonical == root_;
#ifdef LEGO_HAS_CONTAINED_SAVE_OPEN
    ScopedDirectory directory(::open(canonical.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    if (directory.get() < 0) return false;
    auto owner = std::make_shared<NativeDirectory>(directory.get());
    directory.release();
    root_ = std::move(canonical);
    native_root_ = std::move(owner);
    return true;
#else
    return false; // Do not substitute pathname-only traversal on unsupported hosts.
#endif
}

const SaveDataArchiveMounts::Mount* SaveDataArchiveMounts::Find(std::uint64_t handle) const noexcept {
    const auto found = mounts_.find(handle);
    return found == mounts_.end() ? nullptr : &found->second.mount;
}

Result SaveDataArchiveMounts::Open(std::uint64_t program_id, std::uint64_t* out_handle) {
    if (!out_handle) return kResultInvalidPointer;
    *out_handle = 0;
    if (!configured() || !program_id)
        throw std::logic_error("SaveData open requires configured storage and an initialized program");
#ifdef LEGO_HAS_CONTAINED_SAVE_OPEN
    ScopedDirectory directory(::fcntl(native_root_->fd, F_DUPFD_CLOEXEC, 0));
    if (directory.get() < 0) SaveHostError(errno, "SaveData root duplication");
    auto path = root_;
    // Azahar 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac:
    // ArchiveSource_SDSaveData::Open selects this per-program path. It does not
    // require metadata and does not format missing saves. Requiring real,
    // no-follow directories is this host's containment policy.
    for (const auto& part : std::array<std::string,4>{HexProgramPart(program_id >> 32),
            HexProgramPart(static_cast<std::uint32_t>(program_id)), "data", "00000001"}) {
        const auto result = DescendSaveDirectory(directory, part);
        if (result != kResultSuccess) return result;
        path /= part;
    }
    if (next_handle_ == std::numeric_limits<std::uint64_t>::max()) return kResultOutOfHandles;
    auto owner = std::make_shared<NativeDirectory>(directory.get());
    directory.release();
    const auto handle = next_handle_;
    mounts_.emplace(handle, Entry{Mount{program_id, std::move(path)}, std::move(owner)});
    ++next_handle_;
    *out_handle = handle;
    return kResultSuccess;
#else
    throw std::logic_error("contained SaveData opening is unavailable on this host");
#endif
}
} // namespace lego::ctr
