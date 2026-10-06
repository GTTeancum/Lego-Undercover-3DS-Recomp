#pragma once
#include "services/dsp1_image.h"

namespace lego::ctr {
inline constexpr std::uint32_t kDspSpecialConfigBlock = 0x00070000;
inline constexpr std::uint32_t kDspSpecialConfigBytes = 0x214;

enum class DspSpecialProfile { Unconfigured, EmptySystemConfig, SuppliedBlock };
enum class DspConfigRead { Unconfigured, Missing, Available };

// A narrow HOST system-configuration source, not recovered NAND or guest CFG state.
// EmptySystemConfig explicitly models a new profile lacking block 0x70000. Only
// that modeled missing read selects the documented zero fallback. A host file I/O
// error NEVER turns into Missing. SuppliedBlock contains exactly the caller's bytes.
class DspSpecialConfig final {
public:
    static DspSpecialConfig EmptySystemConfig() noexcept;
    static std::optional<DspSpecialConfig> FromBlock(std::span<const std::uint8_t>) noexcept;
    [[nodiscard]] DspSpecialProfile profile() const noexcept { return profile_; }
    [[nodiscard]] DspConfigRead Read(std::span<const std::uint8_t>& output) const noexcept;
private:
    DspSpecialProfile profile_{DspSpecialProfile::Unconfigured};
    std::array<std::uint8_t,kDspSpecialConfigBytes> block_{};
};

enum class DspSpecialSource { None, MissingConfigZeroFallback, SuppliedConfigBlock };
struct DspSpecialReceipt {
    DspSpecialSource source{};
    DspConfigRead read_status{DspConfigRead::Unconfigured};
    std::uint32_t block_id{}, target_bytes{}, bytes{};
    Dsp1MemoryType memory_type{};
    std::array<char,64> sha256{}; // hex, NOT NUL-terminated
    bool operator==(const DspSpecialReceipt&) const = default;
};
enum class DspSpecialError { None, Unresolved, UnsupportedShape, Range, Layout, AlreadyKnown, Allocation };
const char* DspSpecialErrorName(DspSpecialError) noexcept;
// Changes only the validated special range's bytes/known flags and the receipt.
// On failure BOTH image and receipt are unchanged. Ordinary segments and all gaps
// are preserved. This is still host staging, NOT a running or loaded DSP device.
DspSpecialError StageDspSpecial(const DspSpecialConfig&, Dsp1Image&,
                               DspSpecialReceipt&) noexcept;
} // namespace lego::ctr
