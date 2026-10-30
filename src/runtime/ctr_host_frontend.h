#pragma once
#include <cstdint>

namespace lego::ctr {
class GspGpuService;

// Host-selected controls. Ordinary pad buttons occupy bits 0..11. Circle-pad
// coordinates are calibrated HLE units (-154..154), and touch uses original
// 320x240 LCD pixel coordinates. No raw sensor/calibration values are invented.
struct HostInputState {
    std::uint32_t buttons{};
    std::uint16_t touch_x{}, touch_y{};
    bool touching{};
    std::int16_t circle_x{}, circle_y{};
};

enum class HostFrontendResult : std::uint8_t { Continue, Quit, Error };

// Optional, single-host-thread interface. Implementations must remain alive
// while the attached runner executes, must not advance guest time, and must
// return errors without throwing.
// A ready frame is a latched display update (or the initial presentation).
// Other calls pump host events only; input sampling belongs to guest deadlines.
class HostFrontend {
public:
    virtual ~HostFrontend() = default;
    virtual HostFrontendResult Pump(const GspGpuService& gpu, bool frame_ready,
                                    HostInputState& input) noexcept = 0;
    // Acknowledge a successfully published guest sample. Press edges retained
    // by the frontend are consumed here, never by presentation-only calls.
    virtual void InputSampled() noexcept {}
    virtual const char* error() const noexcept = 0;
};
} // namespace lego::ctr
