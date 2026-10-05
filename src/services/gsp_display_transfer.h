#pragma once
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace lego::ctr {
inline constexpr std::uint32_t kGpuVramVirtualBase = 0x1F000000U;
inline constexpr std::uint32_t kGpuVramPhysicalBase = 0x18000000U;
inline constexpr std::uint32_t kGpuVramBytes = 0x00600000U;
inline constexpr std::uint32_t kDisplayTransferMaxBytes = 0x00100000U; // Host work bound.

enum class GpuVramMode { Unconfigured, ReferenceZero };

// Device-owned physical VRAM, independent of CPU virtual mappings. The explicit
// reference-zero constructor follows pinned MemorySystem::Impl::make_unique<u8[]>.
// It is NOT recovered VRAM or a measured hardware reset image. No lazy creation,
// CPU map, ROM pixel substitution or reset on transfer is performed.
class GpuVramBank final {
public:
    static std::shared_ptr<GpuVramBank> ReferenceZero();
    [[nodiscard]] std::span<const std::uint8_t> bytes() const noexcept { return bytes_; }
    bool Write(std::uint32_t offset, std::span<const std::uint8_t> data) noexcept;
private:
    GpuVramBank() : bytes_(kGpuVramBytes, 0) {}
    std::vector<std::uint8_t> bytes_;
};

struct DisplayTransferRequest {
    std::uint32_t input{}, output{}, input_size{}, output_size{}, flags{};
};
struct DisplayTransferPlan {
    DisplayTransferRequest request{};
    std::uint32_t width{}, height{}, bytes{};
    std::vector<std::uint8_t> output;
};

// Current slice: equal-size, unscaled RGBA4, Morton 8x8 tiled VRAM -> linear
// FCRAM. Format/layout flags must be exactly the observed 0x4400. Unsupported
// modes stop; they are not silently treated as a memcpy or successful blit.
// Stages all bytes without touching the VRAM source or any guest destination.
bool StageDisplayTransfer(const DisplayTransferRequest& request,
                          const GpuVramBank* vram, DisplayTransferPlan& plan,
                          const char*& error);
} // namespace lego::ctr
