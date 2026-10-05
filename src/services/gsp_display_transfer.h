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
    std::uint32_t width{}, height{}, bytes{}, input_bytes{};
    bool output_vram{};
    std::vector<std::uint8_t> output;
};

// Supported slices: 0x4400 equal-size RGBA4 tiled VRAM -> private linear heap;
// 0x01001004 horizontally halved RGBA8 tiled VRAM -> RGB8 linear device VRAM.
// The latter uses the programmed output rectangle as a left/top crop, then halves
// its width. Other format/flag combinations remain unsupported. Both declared
// input and output spans are bounded separately; overlapping device spans stop.
// Stages all bytes without touching the VRAM source or any guest destination.
bool StageDisplayTransfer(const DisplayTransferRequest& request,
                          const GpuVramBank* vram, DisplayTransferPlan& plan,
                          const char*& error);
} // namespace lego::ctr
