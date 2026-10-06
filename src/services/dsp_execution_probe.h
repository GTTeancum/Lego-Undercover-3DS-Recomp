#pragma once
#include "services/dsp1_image.h"
#include <array>
#include <cstdint>
#include <memory>
#include <span>

namespace lego::ctr {
enum class DspProbeReset { KnownOnly, ReferenceZeroData };
struct DspProbeOptions {
    bool enabled{};
    DspProbeReset reset{DspProbeReset::KnownOnly};
    std::uint32_t steps{100000};
};
enum class DspProbeState { Paused, ProtocolComplete, Fault };
enum class DspProbeFault { None, UnknownSram, AddressRange, Backend, ExternalMemory, Audio, MailboxLimit };
// Provenance is per byte: allocation zero is never itself a valid source.
enum class DspByteSource : std::uint8_t { Unknown, Image, ReferenceDataReset, FirmwareWrite };
struct DspProbeReply {
    std::uint8_t index{};
    std::uint16_t word{};
    std::uint64_t after_steps{};
    bool operator==(const DspProbeReply&) const = default;
};
struct DspProbeSummary {
    DspProbeState state{};
    DspProbeFault fault{};
    std::uint64_t completed_steps{}, attempted_steps{}, read_words{}, written_words{};
    std::uint32_t pc_before{}, pc_after{}, fault_address{}, known_bytes{}, reply_count{};
    bool has_fault_address{}, has_pipe_base{};
    std::uint16_t pipe_base{};
    std::array<char,240> error{};
    bool operator==(const DspProbeSummary&) const = default;
};
// Disposable, synchronous HOST-ONLY execution of verified firmware. It cannot
// access ARM memory or commit IPC, interrupts, timers, sound or a loaded flag.
// A fault can leave partial DSP instruction effects; it seals the probe against
// further execution rather than claiming instruction rollback or successful load.
class DspExecutionProbe final {
public:
    static constexpr std::uint32_t kMaxStepsPerCall = 100000;
    static std::unique_ptr<DspExecutionProbe> Create(const Dsp1Image&, DspProbeReset,
                                                    const char*& error) noexcept;
    ~DspExecutionProbe();
    DspExecutionProbe(const DspExecutionProbe&)=delete;
    DspExecutionProbe& operator=(const DspExecutionProbe&)=delete;
    DspProbeState Advance(std::uint32_t steps) noexcept;
    [[nodiscard]] const DspProbeSummary& summary() const noexcept;
    [[nodiscard]] std::span<const std::uint8_t> memory() const noexcept;
    [[nodiscard]] std::span<const std::uint8_t> provenance() const noexcept;
    [[nodiscard]] std::span<const DspProbeReply> replies() const noexcept;
private:
    struct Impl;
    explicit DspExecutionProbe(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> impl_;
};
} // namespace lego::ctr
