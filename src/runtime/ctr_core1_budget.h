#pragma once

#include <cstdint>
#include <optional>

namespace lego::ctr {

// Isolated timing/state component for the pinned reference's CpuLimiterMulti.
// NOT connected to Kernel/NativeRunner yet: the live core-0-only guard remains.
// All timestamps are absolute ARM11 cycles, not wall time or guessed CPU charges.
// This does not execute a thread, signal an event, or implement multicore scheduling.
class Core1Budget final {
public:
    enum class Phase : std::uint8_t { Application, System };
    enum class ThreadClass : std::uint8_t { Application, System, Exempt };
    struct Timer {
        std::uint64_t deadline{}, generation{};
        bool operator==(const Timer&) const = default;
    };
    struct State {
        bool ready{}, active{};
        Phase phase{Phase::Application};
        std::uint32_t percentage{};
        std::uint64_t generation{}, last_cycle{};
        std::optional<Timer> timer;
        bool operator==(const State&) const = default;
    };

    // nsToCycles(2'000'000) from pinned thread.h/core_timing.h. Conversion
    // truncates to whole cycles before percentage arithmetic (not 2 ms exactly).
    static constexpr std::uint64_t kIntervalCycles = 536223;
    [[nodiscard]] const State& state() const noexcept { return state_; }

    // Generation tokens cancel stale callbacks, including same-deadline rearming.
    // False means unsupported input/overflow/stale callback/time reversal; no state is changed.
    // Allows is a phase filter only. It does not grant a core affinity or execution right.
    bool Start() noexcept;
    bool End() noexcept;
    bool Update(std::uint64_t now_cycles, std::uint32_t percentage) noexcept;
    bool OnTimer(Timer expected, std::uint64_t now_cycles) noexcept;
    [[nodiscard]] bool Allows(std::uint32_t core, ThreadClass category) const noexcept;
    [[nodiscard]] static std::optional<std::uint64_t> ApplicationCycles(
        std::uint32_t percentage) noexcept;

private:
    static bool BumpGeneration(State& next) noexcept;
    static bool ChangePhase(State& next, std::uint64_t now, std::uint64_t late) noexcept;
    State state_{};
};

} // namespace lego::ctr
