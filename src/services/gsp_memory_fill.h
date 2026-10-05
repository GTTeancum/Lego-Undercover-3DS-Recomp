#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include "services/gsp_display_transfer.h"
#include "services/pica_startup.h"

namespace lego::ctr {
inline constexpr std::uint32_t kMemoryFillMaxBytes = 0x00100000U; // Per-channel host work bound.
struct MemoryFillChannel {
    std::uint32_t start{}, value{}, end{};
    std::uint16_t control{};
};
struct MemoryFillRequest { std::array<MemoryFillChannel,2> channels{}; };
struct MemoryFillChannelPlan {
    bool enabled{}, triggered{};
    std::uint32_t offset{};
    std::array<std::uint32_t,4> setup{};
    std::vector<std::uint8_t> output;
    int interrupt{-1}; // PSC0/PSC1 or suppressed; never generic completion.
};
struct MemoryFillPlan {
    std::array<MemoryFillChannelPlan,2> channels{};
    PicaGpuRegisters registers{};
    std::uint32_t irqs{};
};

// Device-owned VRAM only, with explicit bank configuration. A zero start disables
// its entire channel, including residual end/value/control fields. Enabled but
// untriggered channels store setup without bytes, finish-bit changes, or IRQs.
// All validation/allocation precedes mutation. 24-bit partial pixels are rejected
// instead of reproducing the reference software loop's possible end overrun.
bool StageMemoryFill(const MemoryFillRequest&, const GpuVramBank*,
                     const PicaGpuRegisters&, MemoryFillPlan&, const char*& error);
} // namespace lego::ctr
