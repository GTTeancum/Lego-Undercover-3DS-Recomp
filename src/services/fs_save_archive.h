#pragma once
#include "services/fs_shared_archive.h"

namespace lego::ctr {
// Separate title-save backend. The explicitly selected, existing container is
// pinned; a program selects <high8>/<low8>/data/00000001 using lowercase hex.
// Configuring/opening creates no directories, metadata or application files.
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
