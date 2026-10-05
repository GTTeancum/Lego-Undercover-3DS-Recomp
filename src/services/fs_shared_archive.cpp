#include "services/fs_shared_archive.h"
#include <array>
#include <limits>
#include <string>

namespace lego::ctr {
namespace fs = std::filesystem;
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
} // namespace lego::ctr
