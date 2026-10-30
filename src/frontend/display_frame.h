#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace lego::ctr { class GspGpuService; }

namespace lego::frontend {

struct ScreenImage {
    int width{}, height{};
    // Host-independent R,G,B,A byte order; alpha is always opaque for scanout.
    std::vector<std::uint8_t> rgba;
};

struct DualScreenFrame {
    ScreenImage top;
    ScreenImage bottom;
    [[nodiscard]] bool HasNonBlackPixels() const noexcept;
};

struct ScanoutDescriptor {
    std::uint32_t size_word{}, format_word{}, stride{}, active_buffer{};
    std::array<std::uint32_t,2> addresses{};
    std::uint32_t lcd_color_fill{};
};

// The restored game currently presents rotated RGB8 scanout in device VRAM.
// This decoder accepts that measured format plus explicit LCD force-black.
// Other formats, memory selectors and LCD fills fail instead of guessing pixels.
// On failure the caller's image remains unchanged.
bool DecodeRgb8Scanout(std::span<const std::uint8_t> vram,
                      const ScanoutDescriptor& descriptor, int screen_width,
                      ScreenImage& image, std::string& error);

// Reads the actual selected buffers after GSP has latched a display period.
// Presentation never mutates guest registers, VRAM or LCD control state.
bool ReadDisplayFrame(const ctr::GspGpuService& gpu, DualScreenFrame& frame,
                      std::string& error);

} // namespace lego::frontend
