#pragma once
#include "services/pica_startup.h"
#include <array>
#include <cstdint>
#include <vector>

namespace lego::ctr {
class GuestMemory;
class GpuVramBank;

// Host staging limits, not claims about every PICA texture configuration.
inline constexpr std::uint32_t kPicaTextureMaxBytes = 4 * 1024 * 1024;
enum class PicaTextureFilter : std::uint8_t { Nearest, Linear };
enum class PicaTextureFormat : std::uint8_t { RGBA8 = 0, IA8 = 5, ETC1 = 12, ETC1A4 = 13 };

struct PicaTextureReadSpan {
    std::uint32_t physical_address{},bytes{};
    bool operator==(const PicaTextureReadSpan&) const = default;
};

struct PicaTexturePlan {
    std::uint32_t width{},height{};
    PicaTextureFilter filter{PicaTextureFilter::Nearest};
    PicaTextureFormat format{PicaTextureFormat::RGBA8};
    PicaTextureReadSpan read;
    // Owned encoded storage. read.bytes describes this span, not decoded RGBA.
    std::vector<std::uint8_t> bytes;
    bool operator==(const PicaTexturePlan&) const = default;
};

// Stages ordinary unit0, tiled RGBA8/IA8/ETC1/ETC1A4, base level0, clamp-to-edge in both axes,
// and identical nearest or linear min/mag filters. Dimensions must be powers of
// two in [8,1024]. Consumed register fields require known byte provenance.
// Global texture enables/fog, framebuffer feedback and earlier queue writes are
// the caller's responsibility. The returned complete source span supports its
// dependency checks. FCRAM uses the existing vertex-read policy: physical
// 0x20000000..0x27ffffff maps to readable cacheless-private linear guest memory;
// device VRAM reads only the supplied snapshot. No shared/device callbacks run.
// Failure, including allocation failure, leaves the output plan and all sources
// unchanged. Allocation exceptions may propagate to the owning queue transaction.
bool StagePicaTexture0(const PicaGpuRegisters& registers,
                       const PicaUploadState& uploads,
                       GuestMemory* memory,const GpuVramBank& vram,
                       PicaTexturePlan& plan,const char*& error);

// Pure, allocation-free sampling of a staged texture. Output order is R,G,B,A.
// IA8 taps expand stored alpha/intensity to R=G=B=intensity with separate alpha.
// ETC1 taps decode PICA's little-endian 4x4 blocks within 8x8 tiles. ETC1A4
// supplies independent four-bit alpha; ETC1 is opaque. Undefined differential
// endpoints reject the entire sampled block before any output-color change.
// Linear filtering uses explicitly rounded binary32 arithmetic at normalized
// u*width-0.5 / v*height-0.5, clamped taps and tiled lookup with Y inversion.
// This is a deterministic emulator approximation, not measured PICA precision.
// Unsupported numeric inputs/host rounding and malformed plans leave color
// unchanged. The source plan and guest/device state are never modified.
bool SamplePicaTexture(const PicaTexturePlan& plan,float u,float v,
                       std::array<std::uint8_t,4>& color,const char*& error) noexcept;
} // namespace lego::ctr
