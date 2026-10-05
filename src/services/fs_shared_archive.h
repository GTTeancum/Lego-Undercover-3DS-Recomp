#pragma once
#include <filesystem>
#include <map>
#include <memory>
#include <string_view>
#include "runtime/ctr_kernel.h"

namespace lego::ctr {
inline constexpr Result kResultFsNotFormatted = 0xC8A04554U;
inline constexpr Result kResultFsInvalidPath = 0xE0E046BEU;

inline constexpr Result kResultFsInvalidArchiveHandle = 0xC8804465U;
inline constexpr Result kResultFsFileNotFound = 0xC8804470U;
inline constexpr Result kResultFsPathNotFound = 0xC8804471U;
inline constexpr Result kResultFsFileAlreadyExists = 0xC82044B4U;
inline constexpr Result kResultFsUnsupportedOpenFlags = 0xE0C046F8U;
inline constexpr Result kResultFsUnexpectedFileOrDirectory = 0xE0C04702U;
// Explicit host safety limit, NOT a discovered console capacity.
inline constexpr std::uint64_t kMaxSharedCreateSize = 16ULL * 1024 * 1024;

// Host-backed shared-extdata mount table. No implicit directory creation.
// The host explicitly supplies a root containing 00048000/<LOW-ID>/user.
class SharedArchiveMounts final {
public:
    bool ConfigureRoot(const std::filesystem::path& root);
    [[nodiscard]] bool configured() const noexcept { return !root_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return mounts_.size(); }
    [[nodiscard]] const std::filesystem::path* Find(std::uint64_t handle) const noexcept;
    Result Open(std::uint32_t low_id, std::uint64_t* out_handle);
    [[nodiscard]] bool CanCreateFiles() const noexcept;
    Result CreateFile(std::uint64_t handle, std::u16string_view path,
                      std::uint64_t size) const;
private:
    struct NativeRoot;
    std::shared_ptr<NativeRoot> native_root_;
    std::filesystem::path root_;
    std::map<std::uint64_t, std::filesystem::path> mounts_;
    std::uint64_t next_handle_{1};
};
} // namespace lego::ctr
