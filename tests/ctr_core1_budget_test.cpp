#include "runtime/ctr_core1_budget.h"
#include "runtime/ctr_runner.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>

namespace {
using namespace lego::ctr;
using B = Core1Budget;
using P = B::Phase;
using C = B::ThreadClass;
int failures = 0;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL " << __LINE__ << ": " #x "\n"; ++failures; } } while (0)
constexpr auto last_cycle = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());

void LifecycleAndAllPercentages() {
    B b;
    CHECK(!b.state().ready && !b.state().active && !b.state().timer);
    const auto empty = b.state();
    CHECK(b.Update(100, 30) && b.state() == empty); // Not ready is an ignored update.
    CHECK(b.End() && b.state() == empty);
    for (std::uint32_t percentage = 0; percentage <= 100; ++percentage) {
        B one;
        CHECK(one.Start());
        const auto started = one.state();
        CHECK(one.Start() && one.state() == started);
        CHECK(one.Update(1000, percentage));
        CHECK(one.state().ready && one.state().percentage == percentage);
        if (percentage == 0) {
            CHECK(!one.state().active && !one.state().timer);
            CHECK(one.Allows(1, C::Application) && one.Allows(1, C::System));
            continue;
        }
        const float ratio = percentage / 100.f;
        const auto app = static_cast<std::uint64_t>(536223ULL * ratio);
        CHECK(B::ApplicationCycles(percentage) == app);
        const auto sys = 536223 - app;
        CHECK(one.state().active && one.state().phase == P::System);
        const auto first = *one.state().timer;
        CHECK(first.deadline == 1000 + sys);
        CHECK(!one.Allows(1, C::Application) && one.Allows(1, C::System));
        CHECK(one.Allows(0, C::Application) && one.Allows(0, C::System));
        CHECK(one.Allows(1, C::Exempt));
        CHECK(one.OnTimer(first, first.deadline));
        CHECK(one.state().phase == P::Application);
        CHECK(one.Allows(1, C::Application) && !one.Allows(1, C::System));
        CHECK(one.state().timer->deadline == 1000 + 536223);
        const auto second = *one.state().timer;
        CHECK(one.OnTimer(second, second.deadline));
        CHECK(one.state().phase == P::System);
        CHECK(one.state().timer->deadline == 1000 + 536223 + sys);
    }
    // Real observed requested percentage. Durations are cycles, not percentages
    // of elapsed host wall time and not an invented amount of executed work.
    CHECK(B::ApplicationCycles(30) == 160866);
    CHECK(b.Start() && b.Update(0, 30));
    CHECK(b.state().timer->deadline == 375357);
    CHECK(!B::ApplicationCycles(101) && !B::ApplicationCycles(0xFFFFFFFFU));
}

void UpdatesAndCancellation() {
    B b; CHECK(b.Start() && b.Update(10, 30));
    const auto old = *b.state().timer;
    // Unlike a stateless percentage store, repeated update changes phase.
    CHECK(b.Update(20, 30));
    const auto monotonic = b.state();
    CHECK(!b.Update(19, 30) && b.state() == monotonic);
    CHECK(!b.Update(0, 0) && b.state() == monotonic);
    CHECK(b.state().phase == P::Application && b.state().timer->deadline == 20 + 160866);
    auto before = b.state();
    CHECK(!b.OnTimer(old, old.deadline) && b.state() == before);
    const auto app_timer = *b.state().timer;
    CHECK(b.Update(30, 0));
    CHECK(!b.state().active && !b.state().timer && b.state().phase == P::Application);
    before = b.state();
    CHECK(!b.OnTimer(app_timer, app_timer.deadline) && b.state() == before);
    CHECK(b.Allows(1, C::Application) && b.Allows(1, C::System));
    CHECK(b.Update(40, 30) && b.state().phase == P::System);
    const auto pre_end = *b.state().timer;
    CHECK(b.End()); before = b.state();
    CHECK(!b.state().ready && !b.state().active && !b.state().timer);
    CHECK(!b.OnTimer(pre_end, pre_end.deadline) && b.state() == before);
    CHECK(b.End() && b.state() == before);
    CHECK(b.Start() && b.state().phase == P::Application && b.state().percentage == 0);
    CHECK(b.Update(40, 30));
    CHECK(b.state().timer->deadline == pre_end.deadline);
    CHECK(b.state().timer->generation != pre_end.generation);
    before = b.state();
    CHECK(!b.OnTimer(pre_end, pre_end.deadline) && b.state() == before);
}

void DeadlinesAndLateCallbacks() {
    for (const auto late : {0ULL, 1ULL, 160865ULL, 160866ULL, 160867ULL, 10000000ULL}) {
        B b; CHECK(b.Start() && b.Update(50, 30));
        const auto event = *b.state().timer;
        auto before = b.state();
        CHECK(!b.OnTimer(event, event.deadline - 1) && b.state() == before);
        CHECK(b.OnTimer(event, event.deadline + late));
        const auto duration = 160866ULL > late ? 160866ULL - late : 160866ULL;
        CHECK(b.state().phase == P::Application);
        CHECK(b.state().timer->deadline == event.deadline + late + duration);
        // One callback means one transition even when many intervals late.
        CHECK(b.state().generation == before.generation + 1);
        before = b.state();
        CHECK(!b.OnTimer(event, b.state().timer->deadline) && b.state() == before);
        auto wrong = *b.state().timer; ++wrong.deadline;
        CHECK(!b.OnTimer(wrong, wrong.deadline) && b.state() == before);
    }
    B b; CHECK(b.Start() && b.Update(0, 30));
    for (unsigned i = 0; i < 20000; ++i) {
        const auto event = *b.state().timer;
        CHECK(b.OnTimer(event, event.deadline));
    }
    CHECK(b.state().phase == P::System);
    CHECK(b.state().timer->deadline == 10000ULL * 536223 + 375357);
}

void OverflowAndInvalidInputs() {
    B b; CHECK(b.Start());
    auto before = b.state();
    for (auto p : {101U, 0x80000000U, 0xFFFFFFFFU})
        CHECK(!b.Update(0, p) && b.state() == before);
    CHECK(!b.Update(last_cycle + 1, 30) && b.state() == before);
    CHECK(!b.Update(last_cycle, 30) && b.state() == before);
    CHECK(b.Update(last_cycle - 375357, 30));
    CHECK(b.state().timer->deadline == last_cycle);
    const auto event = *b.state().timer; before = b.state();
    CHECK(!b.OnTimer(event, last_cycle) && b.state() == before); // Next APP would overflow.
    CHECK(!b.OnTimer(event, std::numeric_limits<std::uint64_t>::max()) && b.state() == before);
    CHECK(b.Update(last_cycle, 0)); // Disable needs no new future deadline.
    CHECK(!b.state().active && !b.state().timer);
}

void ProductionGuardStillStops() {
    Kernel k; GuestMemory memory; a32::Registry registry{};
    NativeRunner runner(registry, memory, k);
    CHECK(runner.InitializeMainThread());
    CHECK(k.UpdateAppCpuTimeLimit(30) == kResultSuccess);
    CHECK(k.app_cpu_core0_only());
    CHECK(!k.AppCpuThreadCreationSupported(1));
    const auto handles = k.handles().OpenHandleCount();
    const auto count = k.threads().size(), time = k.now_ns();
    auto cpu = runner.live_state();
    cpu.r[0] = 48; cpu.r[1] = 0x00104DF4; cpu.r[2] = 0x0E007FE0;
    cpu.r[3] = 0x0E007FE0; cpu.r[4] = 1; cpu.r[15] = 0x00102FCC;
    const auto before = cpu;
    SvcBridge bridge(k, &runner.ipc());
    auto stopped = bridge.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcCreateThread},cpu,&memory);
    CHECK(stopped.kind == a32::ExitKind::Svc && cpu.r == before.r);
    CHECK(k.threads().size() == count && k.handles().OpenHandleCount() == handles && k.now_ns() == time);
    // Exercise the isolated planner. No attachment to the live kernel, no wake,
    // no second-core creation and no automatic execution from this component.
    B b; CHECK(b.Start() && b.Update(0, 30));
    CHECK(b.OnTimer(*b.state().timer,b.state().timer->deadline));
    CHECK(k.app_cpu_time_current() == 30 && k.app_cpu_core0_only());
    CHECK(k.threads().size() == count && k.handles().OpenHandleCount() == handles && k.now_ns() == time);
}

void EmitVectors() {
    std::cout << "[\n";
    bool first = true;
    for (unsigned p = 1; p <= 100; ++p) {
        B b; b.Start(); b.Update(1000, p);
        const auto start = *b.state().timer;
        for (const auto late : {0ULL, 1ULL, 200000ULL, 1000000ULL}) {
            B copy = b;
            copy.OnTimer(start,start.deadline+late);
            if (!first) std::cout << ",\n";
            first = false;
            std::cout << "{\"percentage\":" << p << ",\"app_cycles\":" << *B::ApplicationCycles(p)
                      << ",\"first_deadline\":" << start.deadline << ",\"late\":" << late
                      << ",\"next_deadline\":" << copy.state().timer->deadline << "}";
        }
    }
    std::cout << "\n]\n";
}
}

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--emit-vectors") { EmitVectors(); return 0; }
    if (argc != 1) return 2;
    LifecycleAndAllPercentages(); UpdatesAndCancellation(); DeadlinesAndLateCallbacks();
    OverflowAndInvalidInputs(); ProductionGuardStillStops();
    if (failures) return EXIT_FAILURE;
    std::cout << "PASS: isolated reference core-1 budget transitions; live multicore guard retained\n";
    return EXIT_SUCCESS;
}
