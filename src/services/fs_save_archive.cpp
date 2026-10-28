#include "services/fs_save_archive.h"
#include <array>
#include <atomic>
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
#if defined(__linux__)
#include <linux/fs.h>
#include <sys/syscall.h>
#if defined(SYS_renameat2) && defined(RENAME_NOREPLACE)
#define LEGO_HAS_ATOMIC_SAVE_FORMAT 1
#endif
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

#ifdef LEGO_HAS_ATOMIC_SAVE_FORMAT
// Every temporary entry is created exclusively under retained directory fds.
// Cleanup touches only this bounded owned staging tree, never a reopened root.
class SaveFormatStage {
public:
    SaveFormatStage(int parent, std::string name) noexcept
        : parent_(parent), name_(std::move(name)) {}
    ~SaveFormatStage() { if (active_) Rollback(); CloseDirectories(); }
    SaveFormatStage(const SaveFormatStage&) = delete;
    SaveFormatStage& operator=(const SaveFormatStage&) = delete;
    void Open() {
        directories_[0]=::openat(parent_,name_.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        if (directories_[0]<0) SaveHostError(errno,"SaveData staging open");
    }
    void AddDirectory(const char* name) {
        if (count_==directories_.size()) throw std::logic_error("SaveData staging depth exceeded");
        const auto next=count_;
        if (::mkdirat(directories_[next-1],name,0700)!=0)
            SaveHostError(errno,"SaveData staging directory creation");
        names_[next]=name;
        ++count_; // Own the entry even if its following open fails.
        directories_[next]=::openat(directories_[next-1],name,
                                     O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        if (directories_[next]<0) SaveHostError(errno,"SaveData staging directory open");
    }
    [[nodiscard]] int LastDirectory() const noexcept { return directories_[count_-1]; }
    void OwnMetadata(int directory) noexcept { metadata_directory_=directory; }
    void FlushDirectories() {
        for (auto count=count_;count>0;--count)
            if (::fsync(directories_[count-1])!=0) SaveHostError(errno,"SaveData staging directory flush");
    }
    void Publish(const char* destination) {
        // rename(2), Linux RENAME_NOREPLACE: an existing destination is never
        // replaced, including an entry which appeared after the initial walk.
        // ENOSYS/unsupported filesystem errors stop; there is no copy fallback.
        if (::syscall(SYS_renameat2,parent_,name_.c_str(),parent_,destination,RENAME_NOREPLACE)!=0)
            SaveHostError(errno,"SaveData exclusive subtree publication");
        active_=false; // No further fallible storage work after publication.
    }
    bool Rollback() noexcept {
        if (!active_) return true;
        bool ok=true;
        if (metadata_directory_>=0 && ::unlinkat(metadata_directory_,"00000001.metadata",0)!=0)
            ok=false;
        for (auto count=count_;count>1;--count)
            if (::unlinkat(directories_[count-2],names_[count-1],AT_REMOVEDIR)!=0) ok=false;
        if (::unlinkat(parent_,name_.c_str(),AT_REMOVEDIR)!=0) ok=false;
        active_=false;
        return ok;
    }
private:
    void CloseDirectories() noexcept {
        for (auto& fd:directories_) if (fd>=0) { ::close(fd);fd=-1; }
    }
    int parent_;
    std::string name_;
    std::array<int,4> directories_{{-1,-1,-1,-1}};
    std::array<const char*,4> names_{};
    std::size_t count_{1};
    int metadata_directory_{-1};
    bool active_{true};
};

std::string CreateSaveStage(int parent) {
    static std::atomic<std::uint64_t> serial{1};
    for (unsigned attempt=0;attempt<64;++attempt) {
        // Construct the complete name before creating any filesystem entry.
        auto name=".lego-format-"+std::to_string(::getpid())+"-"+
                  std::to_string(serial.fetch_add(1,std::memory_order_relaxed));
        if (::mkdirat(parent,name.c_str(),0700)==0) return name;
        if (errno!=EEXIST) SaveHostError(errno,"SaveData staging allocation");
    }
    throw std::runtime_error("SaveData staging names exhausted");
}

void WriteSaveMetadata(SaveFormatStage& stage,const std::array<std::uint8_t,16>& bytes) {
    const int directory=stage.LastDirectory();
    ScopedDirectory file(::openat(directory,"00000001.metadata",
                                  O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));
    if (file.get()<0) SaveHostError(errno,"SaveData metadata creation");
    stage.OwnMetadata(directory);
    std::size_t done=0;
    while (done<bytes.size()) {
        const auto written=::write(file.get(),bytes.data()+done,bytes.size()-done);
        if (written<0 && errno==EINTR) continue;
        if (written<=0) SaveHostError(written<0?errno:EIO,"SaveData metadata write");
        done+=static_cast<std::size_t>(written);
    }
    if (::fsync(file.get())!=0) SaveHostError(errno,"SaveData metadata flush");
    const int fd=file.release();
    if (::close(fd)!=0) SaveHostError(errno,"SaveData metadata close");
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

bool SaveDataArchiveMounts::CanFormat() const noexcept {
#ifdef LEGO_HAS_ATOMIC_SAVE_FORMAT
    return configured();
#else
    return false;
#endif
}

Result SaveDataArchiveMounts::FormatMissing(std::uint64_t program_id,const SaveDataFormatInfo& info) {
    if (!CanFormat() || !program_id)
        throw std::logic_error("SaveData format requires atomic host support, configured storage and a bound program");
    if (!info.total_size || (info.total_size&511U) || info.total_size>kSaveDataFormatMaxBytes ||
        info.number_directories>kSaveDataFormatMaxEntries || info.number_files>kSaveDataFormatMaxEntries)
        throw std::logic_error("SaveData format metadata exceeds the supported host bounds");
#ifdef LEGO_HAS_ATOMIC_SAVE_FORMAT
    // These are metadata fields, not file preallocation or invented game data.
    // Azahar 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac stores ArchiveFormatInfo
    // beside the save directory; encode its fields explicitly and zero padding.
    std::array<std::uint8_t,16> metadata{};
    const std::array<std::uint32_t,3> words{info.total_size,info.number_directories,info.number_files};
    for (unsigned word=0;word<words.size();++word)
        for (unsigned byte=0;byte<4;++byte) metadata[4*word+byte]=words[word]>>(8*byte);
    metadata[12]=info.duplicate_data?1:0;
    const std::array<std::string,3> names{HexProgramPart(program_id>>32),
        HexProgramPart(static_cast<std::uint32_t>(program_id)),"data"};
    ScopedDirectory parent(::fcntl(native_root_->fd,F_DUPFD_CLOEXEC,0));
    if (parent.get()<0) SaveHostError(errno,"SaveData format root duplication");
    std::size_t missing=0;
    for (;missing<names.size();++missing) {
        const auto result=DescendSaveDirectory(parent,names[missing]);
        if (result==kResultFsNotFormatted) break;
        if (result!=kResultSuccess) return result;
    }
    if (missing==names.size())
        throw std::logic_error("SaveData first-time format does not replace an existing data directory");

    // Publish the first absent ancestor as one complete subtree. An existing
    // data directory would require publishing two sibling entries; that case
    // remains unsupported rather than exposing a partly formatted title.
    SaveFormatStage stage(parent.get(),CreateSaveStage(parent.get()));
    try {
        stage.Open();
        for (std::size_t i=missing+1;i<names.size();++i) stage.AddDirectory(names[i].c_str());
        WriteSaveMetadata(stage,metadata);
        stage.AddDirectory("00000001");
        stage.FlushDirectories();
        stage.Publish(names[missing].c_str());
    } catch (...) {
        if (!stage.Rollback())
            throw std::runtime_error("SaveData format failed and owned staging cleanup failed");
        throw;
    }
    // Process-visible atomic publication only; no power-loss durability or
    // hardware allocation/hash-bucket/duplicate-copy behavior is claimed.
    return kResultSuccess;
#else
    throw std::logic_error("atomic SaveData formatting is unavailable on this host");
#endif
}
} // namespace lego::ctr
