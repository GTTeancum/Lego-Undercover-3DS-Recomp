#pragma once
#include "services/pica_texture.h"
#include "services/pica_vertex_shader.h"
#include <cstdint>
#include <vector>

namespace lego::ctr {
class GpuVramBank;

// Host staging/work limits, not PICA hardware limits.
inline constexpr std::uint32_t kPicaRasterMaxFramebufferBytes = 1024 * 1024;
inline constexpr std::uint32_t kPicaRasterMaxPixelTests = 4 * 1024 * 1024;

struct PicaRasterPatch {
    std::uint32_t offset{}; // Byte offset into the supplied device VRAM bank.
    std::vector<std::uint8_t> bytes;
    bool operator==(const PicaRasterPatch&) const = default;
};

struct PicaRasterPlan {
    std::uint32_t framebuffer_address{},width{},height{};
    std::uint32_t triangles{},pixels_tested{},pixels_covered{},pixels_written{};
    std::uint32_t triangles_outside{}; // Input primitives wholly removed by canonical clipping.
    std::uint32_t depthbuffer_address{},depth_pixels_written{};
    std::vector<PicaTextureReadSpan> texture_reads;
    std::vector<PicaRasterPatch> patches;
    bool operator==(const PicaRasterPlan&) const = default;
};

// Bounded primitive/raster stage for a fresh primitive assembler. The caller
// must prove/reset assembly state before invoking this slice; strip carry across
// draws is not represented here. Shader attributes use their existing component
// provenance. Unsupported or unknown state stops, with no output-plan mutation.
// Triangles wholly outside one homogeneous clip plane are rejected before
// perspective/screen conversion. Positive-W crossings are clipped against the
// six strict canonical planes, interpolating position, preclamped color and
// enabled UV before perspective divide. Per input primitive, at most9 polygon
// vertices and7 fan triangles are staged; triangles still counts input primitives.
// Generated intersections snap exactly to their active plane, without epsilon.
// Nonpositive W, unsupported intermediate arithmetic and framebuffer clipping
// still stop atomically. Every surviving vertex must map inside the target.
// The color slice is tiled RGBA8, optional bounded texture0, disabled fog,
// stencil and alpha tests, and optional D24S8 Z testing/writes. TEV supports
// unscaled primary/texture0/constant/previous Replace/Modulate and additive blending
// with factors0/1/source-alpha/one-minus-source-alpha (or logic Copy).
// Coverage and binary32 interpolation follow the pinned software reference.
// Clipping, depth and texture filtering are bounded emulator models, not measurements of
// hardware precision. Texture/color/depth feedback overlap remains unsupported.
//
// Reads the supplied VRAM snapshot and returns staged color-buffer bytes. Source
// VRAM, registers, uploaded state and shader outputs remain unchanged. The queue
// owner must include these patches in its complete transaction before retiring
// the draw or signaling completion. No host image or substitute clear is used.
// Bounding-box work is preflighted against both the fixed host limit and the
// supplied remaining transaction budget before any fragment loop executes.
bool StagePicaRasterizer(const PicaGpuRegisters& registers,
                         const PicaUploadState& uploads,
                         const PicaVertexShaderPlan& shader,
                         const GpuVramBank& vram,
                         PicaRasterPlan& plan,const char*& error,
                         std::uint32_t pixel_test_budget=kPicaRasterMaxPixelTests,
                         GuestMemory* texture_memory=nullptr);
} // namespace lego::ctr
