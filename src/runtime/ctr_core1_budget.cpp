#include "runtime/ctr_core1_budget.h"

#include <limits>

namespace lego::ctr {
namespace {
// Pinned timing uses signed cycle counts. Do not wrap a future callback or invent
// a saturated deadline. These validation/transaction rules are host safety policy.
constexpr auto kLastCycle = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
}

std::optional<std::uint64_t> Core1Budget::ApplicationCycles(std::uint32_t percentage) noexcept {
    if (percentage > 100) return std::nullopt;
    static_assert(std::numeric_limits<float>::is_iec559 &&
                  std::numeric_limits<float>::digits == 24);
    // Preserve the reference's two binary32 operations and integer truncation.
    // Keep these operations explicit instead of relying on an integer reformulation.
    const float fraction = static_cast<float>(percentage) / 100.0f;
    const float cycles = static_cast<float>(kIntervalCycles) * fraction;
    return static_cast<std::uint64_t>(cycles);
}

bool Core1Budget::BumpGeneration(State& next) noexcept {
    if (next.generation == std::numeric_limits<std::uint64_t>::max()) return false;
    ++next.generation;
    return true;
}

bool Core1Budget::Start() noexcept {
    if (state_.ready) return true;
    State next = state_;
    if (!BumpGeneration(next)) return false;
    next.ready = true;
    next.active = false;
    next.phase = Phase::Application; // First nonzero update switches to System.
    next.percentage = 0; // Reference PREEMPTION_DISABLED.
    next.timer.reset();
    state_ = next;
    return true;
}

bool Core1Budget::End() noexcept {
    if (!state_.ready) return true;
    State next = state_;
    if (!BumpGeneration(next)) return false;
    next.ready = false;
    next.active = false;
    next.timer.reset();
    state_ = next;
    return true;
}

bool Core1Budget::ChangePhase(State& next, std::uint64_t now, std::uint64_t late) noexcept {
    const auto app = ApplicationCycles(next.percentage);
    if (!app || now > kLastCycle) return false;
    next.phase = next.phase == Phase::Application ? Phase::System : Phase::Application;
    auto duration = next.phase == Phase::Application ? *app : kIntervalCycles - *app;
    // This is the pinned late-callback rule, not catch-up through missed phases:
    // subtract lateness only when the new duration is strictly greater than it.
    if (duration > late) duration -= late;
    if (duration > kLastCycle - now || !BumpGeneration(next)) return false;
    next.timer = Timer{now + duration, next.generation};
    next.last_cycle = now;
    return true;
}

bool Core1Budget::Update(std::uint64_t now_cycles, std::uint32_t percentage) noexcept {
    if (percentage > 100 || now_cycles > kLastCycle || now_cycles < state_.last_cycle) return false;
    if (!state_.ready) return true;
    State next = state_;
    next.percentage = percentage;
    if (percentage == 0) {
        // Zero means disable preemption, NOT deny application execution.
        if (!BumpGeneration(next)) return false;
        next.active = false;
        next.timer.reset();
        next.last_cycle = now_cycles;
    } else {
        next.active = true;
        // The reference calls ChangeState for every accepted update, even an
        // identical percentage. Do not silently restart each update in SYS.
        if (!ChangePhase(next, now_cycles, 0)) return false;
    }
    state_ = next;
    return true;
}

bool Core1Budget::OnTimer(Timer expected, std::uint64_t now_cycles) noexcept {
    if (!state_.ready || !state_.active || state_.timer != expected ||
        now_cycles < expected.deadline || now_cycles > kLastCycle) return false;
    State next = state_;
    if (!ChangePhase(next, now_cycles, now_cycles - expected.deadline)) return false;
    state_ = next;
    return true;
}

bool Core1Budget::Allows(std::uint32_t core, ThreadClass category) const noexcept {
    if (core != 1 || !state_.ready || !state_.active || category == ThreadClass::Exempt) return true;
    return (category == ThreadClass::Application && state_.phase == Phase::Application) ||
           (category == ThreadClass::System && state_.phase == Phase::System);
}

} // namespace lego::ctr
