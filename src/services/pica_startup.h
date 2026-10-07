#pragma once
#include <array>
#include <bitset>
#include <cstdint>
#include <span>

namespace lego::ctr {
inline constexpr std::size_t kPicaGpuWords = 0x732;
inline constexpr std::size_t kPicaInternalWords = 0x300;
inline constexpr std::uint32_t kPicaMaxListBytes = 1024 * 1024; // Host work cap, not hardware.
using PicaGpuRegisters = std::array<std::uint32_t,kPicaGpuWords>;

// Uploaded state, NOT executed shaders or a rasterizer. Program/LUT zero storage
// follows the pinned HLE. Unwritten uniforms have no asserted value: known flags
// must be consulted by any future consumer. Float values are IEEE-32 BIT PATTERNS,
// following the reference's f24 container, not a claim of 24-bit arithmetic parity.
struct PicaShaderUpload {
    std::array<std::uint32_t,4096> program{};
    std::bitset<4096> program_written{};
    // Operand descriptors uploaded by the guest. Unwritten entries are unknown
    // to future shader consumers even though host allocation storage is zero.
    std::array<std::uint32_t,4096> swizzle{};
    std::bitset<4096> swizzle_written{};
    std::array<std::array<std::uint32_t,4>,96> floats{};
    std::bitset<96> floats_written{};
    std::array<std::uint32_t,4> integers{};
    std::bitset<4> integers_written{};
    std::uint16_t booleans{};
    bool booleans_written{};
    std::array<std::uint32_t,4> packed{};
    std::uint32_t packed_count{};
    bool operator==(const PicaShaderUpload&) const = default;
};
// Raw procedural-table upload state. A future sampler must check written bits;
// allocation zeros are not proof that the guest uploaded an entry. The three
// value tables have 128 entries; color and color-difference tables have 256.
template<std::size_t N> struct PicaLookupUpload {
    std::array<std::uint32_t,N> words{};
    std::bitset<N> written{};
    bool operator==(const PicaLookupUpload&) const = default;
};
struct PicaProceduralUpload {
    PicaLookupUpload<128> noise, color_map, alpha_map;
    PicaLookupUpload<256> color, color_difference;
    bool operator==(const PicaProceduralUpload&) const = default;
};
struct PicaUploadState {
    PicaShaderUpload gs,vs;
    std::array<std::array<std::uint32_t,256>,24> lighting{};
    std::array<std::bitset<256>,24> lighting_written{};
    PicaProceduralUpload procedural{};
    std::bitset<kPicaInternalWords> registers_written{};
    std::uint32_t topology{}; // Empty primitive assembler; all draw paths stop.
    bool operator==(const PicaUploadState&) const = default;
};
struct PicaListResult {
    const char* error{};
    std::uint32_t byte_offset{},register_id{};
    std::uint32_t packets{},writes{},program_words{},swizzle_words{},uniform_vectors{},lut_words{},procedural_words{},irqs{};
    bool autostopped{};
};
struct PicaListPlan {
    PicaGpuRegisters registers{};
    PicaUploadState uploads{};
    PicaListResult result{};
};
// This stages into a disposable plan. Failure never mutates initial state. No
// memory, event, queue, timing or renderer effects are committed here.
bool StagePicaStartupList(std::span<const std::uint8_t> bytes,
                          const PicaGpuRegisters& initial,
                          const PicaUploadState& uploads,
                          PicaListPlan& plan) noexcept;
} // namespace lego::ctr
