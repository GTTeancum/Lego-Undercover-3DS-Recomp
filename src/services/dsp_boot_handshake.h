#pragma once
#include <cstdint>
#include <optional>

namespace lego::ctr {
enum class DspMailboxRead { Empty, Received, Failed };
class DspBootMailbox {
public:
    virtual ~DspBootMailbox() = default;
    // A real backend must test readiness and consume at most ONE received word.
    // Empty/Failed must not claim a word. This API never generates ready replies.
    virtual DspMailboxRead TryReceive(std::uint8_t index,std::uint16_t& word) noexcept = 0;
};
enum class DspBootPoll { NeedsExecution, ReadBudget, ProtocolComplete, BackendFailed };
// Bounded continuation of the pinned loader's receive protocol, NOT an executor.
// No timers, dummy register values, DSP-ready flags, SRAM or guest IPC are supplied.
// A backend must still run actual firmware/native HLE semantics, enforce known
// memory and provide the received words. This component is NOT connected to live
// LoadComponent until that backend exists. ProtocolComplete alone is not a DSP load.
class DspBootHandshake final {
public:
    explicit DspBootHandshake(bool startup_replies) noexcept : phase_(startup_replies?0:3) {}
    static constexpr std::uint32_t kMaxReadsPerPoll=256; // Host work bound.
    DspBootPoll Poll(DspBootMailbox&,std::uint32_t read_budget) noexcept;
    [[nodiscard]] std::optional<std::uint8_t> expected_register() const noexcept;
    [[nodiscard]] std::optional<std::uint16_t> pipe_base_words() const noexcept {return pipe_base_;}
    [[nodiscard]] std::uint64_t received_words() const noexcept {return received_;}
    [[nodiscard]] std::uint64_t discarded_words() const noexcept {return discarded_;}
    [[nodiscard]] bool failed() const noexcept {return failed_;}
private:
    unsigned phase_{};
    bool failed_{};
    std::uint64_t received_{},discarded_{};
    std::optional<std::uint16_t> pipe_base_;
};
} // namespace lego::ctr
