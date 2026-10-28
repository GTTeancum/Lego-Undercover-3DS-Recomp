#pragma once
#include "services/pica_vertex_fetch.h"
#include <array>
#include <cstdint>
#include <vector>

namespace lego::ctr {
inline constexpr std::uint32_t kPicaVertexShaderMaxInstructions = 1024;
inline constexpr std::uint32_t kPicaVertexShaderMaxTotalInstructions = 1024 * 1024;
inline constexpr std::uint32_t kPicaVertexShaderMaxCallDepth = 3;

struct PicaShadedVertex {
    std::uint32_t source_index{},instruction_count{};
    // IEEE32 bit patterns. Component masks use bit0=x, bit1=y, bit2=z, bit3=w.
    // Unwritten components remain unknown; zeroed host storage is not a value.
    std::array<std::array<std::uint32_t,4>,16> outputs{},attributes{};
    std::array<std::uint8_t,16> output_components{},attribute_components{};
    bool operator==(const PicaShadedVertex&) const = default;
};

struct PicaVertexShaderPlan {
    std::uint32_t output_count{},instruction_count{};
    std::vector<PicaShadedVertex> vertices;
    bool operator==(const PicaVertexShaderPlan&) const = default;
};

// Executes the bounded native VS slice MOV, MUL, DPHI, MADI, CALLU and END.
// Inputs must come from the same register/upload snapshot as the fetch plan.
// Uploaded instructions/descriptors/uniforms and every consumed source component
// must be known. Output attributes compact enabled output registers in ascending
// order; callers must still inspect component masks before consuming any value.
// Relative addressing, GS modes, unknown opcodes, deep calls and calls at return
// boundaries stop. No shader substitution or implicit END at unknown code exists.
//
// Arithmetic follows the pinned reference's float32-container approximation,
// with separately rounded multiply/add operations (no fused MAD). This is not
// hardware float24 precision. The supported arithmetic slice requires nearest
// host rounding and finite normal/zero operands and results; subnormal, underflow
// and nonfinite arithmetic stop. MOV preserves raw bits with optional sign flip.
//
// Source state is immutable and the output plan is replaced only on success.
// This neither rasterizes nor authorizes a draw/queue completion.
bool StagePicaVertexShader(const PicaGpuRegisters& registers,
                           const PicaUploadState& uploads,
                           const PicaVertexFetchPlan& fetched,
                           PicaVertexShaderPlan& plan,const char*& error);
} // namespace lego::ctr
