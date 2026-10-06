#pragma once
#include "services/dsp_execution_probe.h"
#include "runtime/ctr_memory.h"
#include <optional>

namespace lego::ctr {
enum class DspPipeResult { Complete, WouldBlock, Invalid, Fault };
struct DspPipeDescriptor {
    std::uint16_t address_words{},capacity{},read_pointer{},write_pointer{};
    std::uint8_t slot{},flags{};
    std::uint32_t used{};
};
// Bounded persistent execution from the REAL firmware handshake. The service owns
// its lifecycle; ARM and firmware share DATA bytes/provenance/reservation epochs.
// External AHB, audio output and debug-pipe draining still fail closed when reached.
class DspLiveDevice final {
public:
    static constexpr std::uint32_t DataAddress=0x1FF40000, SliceSteps=16384;
    static std::unique_ptr<DspLiveDevice> Prepare(std::shared_ptr<DspExecutionProbe>,
                                                 std::uint64_t now_ns,const char*& error) noexcept;
    bool Attach(GuestMemory&); // mapping allocation precedes IPC success
    std::optional<std::uint64_t> next_deadline_ns() const noexcept;
    bool RunScheduled(std::uint64_t now_ns) noexcept;
    const char* error() const noexcept{return error_;}
    bool attached() const noexcept{return attached_;}
    std::uint64_t slices() const noexcept{return slices_;}
    DspPipeResult InspectPipe(std::uint8_t slot,DspPipeDescriptor&) const noexcept;
    DspPipeResult ReadPipe(std::uint8_t pipe,std::span<std::uint8_t> output) noexcept;
    DspPipeResult WritePipe(std::uint8_t pipe,std::span<const std::uint8_t> input) noexcept;
    std::shared_ptr<DspExecutionProbe> probe() const noexcept{return probe_;}
private:
    explicit DspLiveDevice(std::shared_ptr<DspExecutionProbe> p):probe_(std::move(p)){}
    bool ValidateTable() const noexcept;
    bool WritePointer(std::uint8_t slot,std::uint16_t value) noexcept;
    bool Fail(const char* why) noexcept{error_=why;return false;}
    std::shared_ptr<DspExecutionProbe> probe_;
    std::uint32_t table_{};
    std::uint64_t next_tick_{},slices_{};
    bool attached_{};
    const char* error_{};
};
} // namespace lego::ctr
