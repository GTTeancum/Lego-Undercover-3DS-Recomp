#pragma once
#include "services/pica_startup.h"
#include <array>
#include <bitset>
#include <cstdint>
#include <vector>

namespace lego::ctr {
class GuestMemory;
class GpuVramBank;

// Explicit host work bounds for a staged fetch, not claims of PICA capacity.
inline constexpr std::uint32_t kPicaVertexFetchMaxVertices = 4096;
inline constexpr std::uint32_t kPicaVertexFetchMaxBytes = 1024 * 1024;

struct PicaFetchedVertex {
    std::uint32_t source_index{};
    // IEEE32 bit patterns, including signed zero/NaN payloads from float arrays.
    // Unwritten entries have no asserted value, despite zeroed host storage.
    std::array<std::array<std::uint32_t,4>,16> attributes{},inputs{};
    std::bitset<16> attributes_written{},inputs_written{};
    bool operator==(const PicaFetchedVertex&) const = default;
};

struct PicaVertexReadSpan {
    // vertex_index is the source index after applying vertex_offset, matching
    // PicaFetchedVertex::source_index rather than the submitted ordinal.
    std::uint32_t physical_address{},bytes{},vertex_index{},attribute_index{};
    bool operator==(const PicaVertexReadSpan&) const = default;
};

struct PicaVertexFetchPlan {
    std::uint32_t attribute_count{},input_count{};
    std::vector<PicaFetchedVertex> vertices;
    // Exact source reads, retained for diagnostics and future transaction checks.
    // Their presence does not make this a complete draw or queue execution plan.
    std::vector<PicaVertexReadSpan> reads;
    bool operator==(const PicaVertexFetchPlan&) const = default;
};

// Native, non-indexed (0x22e) attribute fetch and VS input-register mapping only.
// This does not execute shaders, assemble/rasterize triangles, signal completion,
// or permit a draw command through StagePicaStartupList. GS/input-to-uniform modes
// and vertex retention remain explicit stops. Defaults must have been uploaded;
// register fields must have known byte writes, not just allocated shadow values.
// Direct GSP MMIO writes currently do not establish new byte-knowledge flags;
// configurations supplied only through that path conservatively remain unknown.
//
// Array reads are restricted to readable private linear FCRAM (physical
// 0x20000000..0x27ffffff -> VA 0x14000000..0x1bffffff), or the supplied device VRAM
// bank. Shared/device CPU callbacks and unknown physical mappings are excluded.
// All source state is read-only; failure leaves the caller's plan unchanged.
bool StagePicaVertexFetch(std::uint32_t draw_register,
                          const PicaGpuRegisters& registers,
                          const PicaUploadState& uploads,
                          GuestMemory& memory,const GpuVramBank* vram,
                          PicaVertexFetchPlan& plan,const char*& error);
} // namespace lego::ctr
