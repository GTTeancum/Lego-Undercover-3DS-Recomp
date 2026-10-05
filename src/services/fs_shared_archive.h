#pragma once
#include <filesystem>
#include <map>
#include "runtime/ctr_kernel.h"

namespace lego::ctr {
inline constexpr Result kResultFsNotFormatted = 0xC8A04554U;
inline constexpr Result kResultFsInvalidPath = 0xE0E046BEU;

// Host-backed shared-extdata mount table. No directory creation or file I/O.
// The host explicitly supplies a root containing 00048000/<LOW-ID>/user.
class SharedArchiveMounts final {
public:
    bool ConfigureRoot(const std::filesystem::path& root);
    [[nodiscard]] bool configured() const noexcept { return !root_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return mounts_.size(); }
    [[nodiscard]] const std::filesystem::path* Find(std::uint64_t handle) const noexcept;
    Result Open(std::uint32_t low_id, std::uint64_t* out_handle);
private:
    std::filesystem::path root_;
    std::map<std::uint64_t, std::filesystem::path> mounts_;
    std::uint64_t next_handle_{1};
};
} // namespace lego::ctr
