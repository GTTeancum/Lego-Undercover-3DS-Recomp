#pragma once
#include "services/fs_shared_archive.h"

namespace lego::ctr {
// Host support bounds for format metadata, not claims about hardware capacity.
inline constexpr std::uint32_t kSaveDataFormatMaxBytes = 64U * 1024U * 1024U;
inline constexpr std::uint32_t kSaveDataFormatMaxEntries = 4096;
inline constexpr std::uint32_t kSaveDataFormatMaxBuckets = 8192;
struct SaveDataFormatInfo {
    std::uint32_t total_size{}, number_directories{}, number_files{};
    bool duplicate_data{};
};

// A real opened file whose new leaf, if any, remains private until Publish.
// Allocate the client endpoint and prepare the IPC reply before publication.
// Rollback reports cleanup failures explicitly; existing files are never removed.
class SaveDataFileOpenPlan final {
public:
    SaveDataFileOpenPlan();
    ~SaveDataFileOpenPlan();
    SaveDataFileOpenPlan(const SaveDataFileOpenPlan&) = delete;
    SaveDataFileOpenPlan& operator=(const SaveDataFileOpenPlan&) = delete;
    std::unique_ptr<SharedArchiveFile> TakeFile() noexcept;
    void Publish();
    bool Rollback() noexcept;
private:
    friend class SaveDataArchiveMounts;
    struct NativeCreation;
    std::unique_ptr<SharedArchiveFile> file_;
    std::unique_ptr<NativeCreation> creation_;
};

// Separate title-save backend. The explicitly selected, existing container is
// pinned; a program selects <high8>/<low8>/data/00000001 using lowercase hex.
// ConfigureRoot/Open (archive mounting) create no directories, metadata or files.
class SaveDataArchiveMounts final {
public:
    struct Mount {
        static constexpr std::uint32_t archive_id = 4;
        std::uint64_t program_id;
        std::filesystem::path path;
    };
    bool ConfigureRoot(const std::filesystem::path& root);
    [[nodiscard]] bool configured() const noexcept { return native_root_ != nullptr; }
    [[nodiscard]] std::size_t size() const noexcept { return mounts_.size(); }
    [[nodiscard]] const Mount* Find(std::uint64_t handle) const noexcept;
    // A configured missing title returns NotFormatted and zero. An unconfigured
    // backend or unbound program is unsupported, not an invented guest result.
    Result Open(std::uint64_t program_id, std::uint64_t* out_handle);
    // Retire only this program's archive mount. Files opened through it own
    // separate descriptors and remain valid; storage and next_handle_ are kept.
    Result Close(std::uint64_t handle, std::uint64_t program_id) noexcept;
    [[nodiscard]] bool CanFormat() const noexcept;
    // Creates a complete missing high/low/data subtree using an atomic exclusive
    // rename. Existing data directories (even empty ones) require a later format
    // policy and stop; no existing save is removed, replaced or mounted here.
    // Requires Linux renameat2(RENAME_NOREPLACE); no weaker fallback is used.
    Result FormatMissing(std::uint64_t program_id, const SaveDataFormatInfo& info);
    [[nodiscard]] bool CanOpenFiles() const noexcept { return CanFormat(); }
    // Initial supported slice: mode 7 (read/write/create), no truncation. Mount
    // ownership is checked against the initialized fs:USER program. New leaves
    // publish atomically without replacing a concurrent destination.
    Result StageOpenFile(std::uint64_t handle, std::uint64_t program_id,
                         std::u16string_view path, std::uint32_t mode,
                         SaveDataFileOpenPlan& out) const;
private:
    struct NativeDirectory;
    struct Entry {
        Mount mount;
        std::shared_ptr<NativeDirectory> directory;
    };
    std::shared_ptr<NativeDirectory> native_root_;
    std::filesystem::path root_;
    std::map<std::uint64_t, Entry> mounts_;
    std::uint64_t next_handle_{kSaveDataArchiveHandleTag | 1};
};
} // namespace lego::ctr
