#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace lego::ctr {
inline constexpr std::uint32_t kDsp1HeaderBytes = 0x300;
inline constexpr std::uint32_t kDsp1BankBytes = 0x40000;
inline constexpr std::uint32_t kDsp1MaxBytes = 0x100000; // Host inspection work bound.
enum class Dsp1MemoryType : std::uint8_t { ProgramA, ProgramB, Data };
struct Dsp1Segment {
    std::uint32_t source_offset{}, target_words{}, bytes{};
    Dsp1MemoryType type{};
    std::string sha256;
};
struct Dsp1SpecialSegment {
    bool required{};
    Dsp1MemoryType type{};
    std::uint32_t target_words{}, bytes{};
};
// A HOST staging image, not a DSP device or a declaration of power-on SRAM.
// Only bytes copied from verified segments are known. Zero allocation storage in
// gaps and the required special segment MUST NOT be consumed as firmware data.
class Dsp1Bank final {
public:
    Dsp1Bank() : bytes_(kDsp1BankBytes, 0), known_(kDsp1BankBytes, 0) {}
    bool IsKnown(std::uint32_t offset, std::uint32_t size) const noexcept;
    std::optional<std::uint8_t> Read(std::uint32_t offset) const noexcept;
    std::uint32_t known_bytes() const noexcept;
    // Staging only. Refuses overlap; not a device write or reset operation.
    bool Stage(std::uint32_t offset, std::span<const std::uint8_t> bytes) noexcept;
    std::span<const std::uint8_t> storage() const noexcept { return bytes_; }
    std::span<const std::uint8_t> known_mask() const noexcept { return known_; }
private:
    std::vector<std::uint8_t> bytes_, known_;
};
struct Dsp1Image {
    std::uint32_t binary_bytes{};
    std::uint16_t memory_layout{};
    bool receive_startup_replies{};
    Dsp1SpecialSegment special{};
    std::vector<Dsp1Segment> segments;
    Dsp1Bank program, data;
    std::string component_sha256;
    // RSA verification and boot execution are NOT performed by this parser.
};
enum class Dsp1Error {
    None, Size, Magic, BinarySize, SegmentCount, Flags, Type, SourceRange,
    TargetRange, Alignment, SourceOverlap, TargetOverlap, MemoryLayout,
    SegmentHash, SpecialRange, SpecialOverlap, Allocation
};
struct Dsp1Result {
    Dsp1Error error{};
    std::uint32_t segment{};
    explicit operator bool() const noexcept { return error == Dsp1Error::None; }
};
const char* Dsp1ErrorName(Dsp1Error error) noexcept;
// Validates all records/hashes before publishing a complete host-owned image.
// Failure (including allocation failure) preserves the caller's existing image.
// ProgramA and ProgramB address the same bank. Strict overlap/layout/alignment
// rejection is host policy, not a claim of firmware error codes or precedence.
Dsp1Result StageDsp1Image(std::span<const std::uint8_t> source,
                          std::unique_ptr<Dsp1Image>& output) noexcept;
} // namespace lego::ctr
