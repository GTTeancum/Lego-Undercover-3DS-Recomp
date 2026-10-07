#pragma once
#include "services/dsp1_image.h"
#include "runtime/ctr_device_memory.h"
#include <array>
#include <cstdint>
#include <memory>
#include <span>

namespace lego::ctr {
enum class DspBootMode { Immediate, ReferenceSlice };
enum class DspProbeReset { KnownOnly, ReferenceZeroData };
struct DspProbeOptions {
    bool enabled{};
    DspProbeReset reset{DspProbeReset::KnownOnly};
    std::uint32_t steps{100000};
    bool live{}; // Explicit integration; probe-only remains default.
    bool capture_audio{}; // Explicit bounded host capture; no playback or silent discard.
    bool reference_transmit{}; // Opt-in bounded preset/IRQ gate + reference FIFO-empty source.
    bool reference_audio_silence{}; // Missing FIFO words tagged separately, opt-in only.
    DspBootMode boot_mode{DspBootMode::Immediate}; // Explicit pinned-loader polling cadence.
};
enum class DspProbeState { Paused, ProtocolComplete, Fault };
enum class DspProbeFault { None, UnknownSram, AddressRange, Backend, ExternalMemory, Audio, MailboxLimit };
// Provenance is per byte: allocation zero is never itself a valid source.
enum class DspByteSource : std::uint8_t { Unknown, Image, ReferenceDataReset, FirmwareWrite, HostWrite };
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
struct DspCapturedAudioFrame {
    std::array<std::int16_t,2> samples{}; // FIFO order; not a newly measured L/R map.
    std::uint8_t fifo_mask{};          // Bits 0/1: actual FIFO words, else reference underflow.
    std::uint64_t during_run_call{};     // Interpreter attempt, not hardware time.
    bool operator==(const DspCapturedAudioFrame&) const = default;
};
// Guarded DSP execution. Advance is a synchronous host-only boot probe; only
// DspLiveDevice connects its completed state to the explicit live runtime. The
// executor itself never commits IPC, ARM interrupts, timers, sound or a loaded flag.
// A fault can leave partial DSP instruction effects; it seals the probe against
// further execution rather than claiming instruction rollback or successful load.
class DspExecutionProbe final {
public:
    static constexpr std::uint32_t kMaxStepsPerCall = 100000;
    static constexpr std::uint32_t kReferenceBootSlice = 16384; // Pinned LLE RunTeakraSlice, not a title-specific delay.
    static std::unique_ptr<DspExecutionProbe> Create(const Dsp1Image&, DspProbeReset,
                                                    const char*& error, bool capture_audio=false, bool reference_silence=false, bool reference_transmit=false,
                                                    DspBootMode boot_mode=DspBootMode::Immediate) noexcept;
    static constexpr std::size_t kAudioCaptureCapacity = 4096; // Host bound, not hardware FIFO.
    [[nodiscard]] std::span<const DspCapturedAudioFrame> captured_audio() const noexcept;
    ~DspExecutionProbe();
    DspExecutionProbe(const DspExecutionProbe&)=delete;
    DspExecutionProbe& operator=(const DspExecutionProbe&)=delete;
    DspProbeState Advance(std::uint32_t steps) noexcept;
    // Used only by DspLiveDevice AFTER verified protocol and pipe-table checks.
    // Advance remains terminal at ProtocolComplete for historical probe callers.
    bool ContinueLive(std::uint32_t steps) noexcept;
    std::uint16_t TakeLiveInterrupts() noexcept;
    [[nodiscard]] std::uint64_t live_notifications() const noexcept;
    // ARM-to-DSP APBP semaphore, not DSP-to-ARM completion. Updates the SAME
    // live peripheral and its ICU input without executing firmware or advancing time.
    bool SetSemaphore(std::uint16_t bits) noexcept;
    bool CanSend(std::uint8_t index) const noexcept;
    bool Send(std::uint8_t index, std::uint16_t word) noexcept;
    std::shared_ptr<DeviceMemory> data_backing() const noexcept;
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
