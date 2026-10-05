#include "runtime/ctr_clock.h"
#include "runtime/ctr_memory.h"
#include "runtime/ctr_runner.h"
#include "runtime/ctr_svc_bridge.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
using namespace lego::ctr;
int failures = 0;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL line " << __LINE__ << ": " #x "\n"; ++failures; } } while (0)

std::uint32_t Word(GuestMemory& memory, std::uint32_t address) {
    std::uint32_t value = 0;
    CHECK(memory.Read32(address, &value));
    return value;
}
std::uint64_t Wide(GuestMemory& memory, std::uint32_t address) {
    std::uint64_t value = 0;
    std::uint32_t fault = 0;
    CHECK(memory.Read64(address, &value, &fault));
    return value;
}
std::array<std::uint8_t, kSharedPageSize> Page(GuestMemory& memory) {
    std::array<std::uint8_t, kSharedPageSize> result{};
    for (std::size_t i = 0; i < result.size(); ++i)
        CHECK(memory.Read8(kSharedPageBase + static_cast<std::uint32_t>(i), &result[i]));
    return result;
}
std::uint32_t ActiveRecord(GuestMemory& memory) {
    return kSharedPageBase + kSharedClockSnapshot0Offset +
           (Word(memory, kSharedPageBase) & 1U) * kSharedClockSnapshotBytes;
}
std::uint64_t SvcTick(Kernel& kernel) {
    SvcBridge bridge(kernel);
    a32::GuestState state{};
    state.r[2] = 0x11223344;
    const auto exit = bridge.Handle({a32::ExitKind::Svc, 0x100000,
                                    a32::FallbackReason::None, kSvcGetSystemTick}, state);
    CHECK(exit.kind == a32::ExitKind::Fallthrough && exit.pc == 0x100004);
    CHECK(state.r[2] == 0x11223344);
    return (std::uint64_t(state.r[1]) << 32U) | state.r[0];
}

void InitialPageAndProtection() {
    Kernel kernel;
    GuestMemory memory;
    CHECK(memory.EnsureSharedClockPage(kernel.now_ns()));
    CHECK(memory.IsReadable(kSharedPageBase, kSharedPageSize));
    CHECK(!memory.IsWritable(kSharedPageBase, kSharedPageSize));
    CHECK(!memory.IsMapped(kSharedPageBase - 1));
    CHECK(!memory.IsMapped(kSharedPageBase + kSharedPageSize));
    CHECK(Word(memory, kSharedPageBase) == 1);
    const auto record = ActiveRecord(memory);
    CHECK(record == kSharedPageBase + 0x40);
    CHECK(Wide(memory, record) == kDefaultRtcMsSince1900);
    CHECK(Wide(memory, record + 8) == SvcTick(kernel));
    CHECK(Wide(memory, record + 16) == kArm11TicksPerSecond);
    CHECK(Wide(memory, record + 24) == 0);
    CHECK(Wide(memory, kSharedPageBase + 0x20) == 0); // Inactive at first publication.
    const auto before = Page(memory);
    CHECK(memory.EnsureSharedClockPage(0));
    CHECK(Page(memory) == before);
    CHECK(!memory.Write8(record, 0xaa));
    CHECK(!memory.Write16(record, 0xaabb));
    CHECK(!memory.Write32(record, 0xaabbccdd));
    std::uint32_t fault = 0;
    CHECK(!memory.Write64(record, 123, &fault) && fault == record);
    std::uint64_t value = 0, token = 0;
    CHECK(memory.LoadExclusive(record, 8, &value, &token, &fault));
    CHECK(memory.StoreExclusive(record, 8, 99, token, &fault) ==
          a32::ExclusiveStoreResult::MemoryFault);
    std::uint32_t old = 0;
    CHECK(!memory.AtomicSwap(record, 4, 99, &old, &fault));
    CHECK(Page(memory) == before);
    CHECK(kernel.now_ns() == 0 && kernel.current_thread()->status == ThreadStatus::Running);
}

void SharedTickAndHourlyPublication() {
    Kernel kernel;
    GuestMemory memory;
    constexpr auto epoch = kDefaultRtcMsSince1900 + 86400000ULL;
    CHECK(memory.EnsureSharedClockPage(0, epoch));
    const auto first = Page(memory);
    kernel.AdvanceTime(1500000000ULL);
    CHECK(memory.EnsureSharedClockPage(kernel.now_ns(), epoch));
    CHECK(Page(memory) == first); // Interpolation from the snapshot, not a new publication.
    CHECK(SvcTick(kernel) == 402167784ULL);
    auto active = ActiveRecord(memory);
    const auto elapsed_ticks = SvcTick(kernel) - Wide(memory, active + 8);
    CHECK(Wide(memory, active) + elapsed_ticks * 1000 / Wide(memory, active + 16) ==
          epoch + 1500);

    kernel.AdvanceTime(kSharedClockRefreshNs - kernel.now_ns());
    CHECK(memory.EnsureSharedClockPage(kernel.now_ns(), epoch));
    CHECK(Word(memory, kSharedPageBase) == 2);
    active = ActiveRecord(memory);
    CHECK(active == kSharedPageBase + 0x20);
    CHECK(Wide(memory, active) == epoch + 3600000);
    CHECK(Wide(memory, active + 8) == SvcTick(kernel));
    CHECK(Wide(memory, kSharedPageBase + 0x40) == epoch);

    kernel.AdvanceTime(kSharedClockRefreshNs + 123000000);
    CHECK(memory.EnsureSharedClockPage(kernel.now_ns(), epoch));
    CHECK(Word(memory, kSharedPageBase) == 3);
    active = ActiveRecord(memory);
    CHECK(active == kSharedPageBase + 0x40);
    CHECK(Wide(memory, active) == epoch + 7200123);
    CHECK(Wide(memory, active + 8) == SvcTick(kernel));
    CHECK(Wide(memory, kSharedPageBase + 0x20) == epoch + 3600000);

    // Refresh only observes the supplied time. Large missed periods coalesce.
    kernel.AdvanceTime(10 * kSharedClockRefreshNs);
    CHECK(memory.EnsureSharedClockPage(kernel.now_ns(), epoch));
    CHECK(Word(memory, kSharedPageBase) == 4);
    CHECK(Wide(memory, ActiveRecord(memory)) == epoch + kernel.now_ns()/1000000ULL);
    const auto before = Page(memory);
    CHECK(!memory.EnsureSharedClockPage(kernel.now_ns() - 1, epoch));
    CHECK(!memory.EnsureSharedClockPage(kernel.now_ns(), epoch + 1));
    CHECK(Page(memory) == before);
    CHECK(kernel.current_thread()->status == ThreadStatus::Running);
}

void BoundsAndOwnership() {
    CHECK(SystemTicksFromNanoseconds(0) == 0);
    CHECK(SystemTicksFromNanoseconds(1) == 0);
    CHECK(SystemTicksFromNanoseconds(999999999) == kArm11TicksPerSecond - 1);
    CHECK(SystemTicksFromNanoseconds(1000000000) == kArm11TicksPerSecond);
    CHECK(SystemTicksFromNanoseconds(20000000000ULL) == 20*kArm11TicksPerSecond);
    constexpr auto max_ns = std::numeric_limits<std::uint64_t>::max();
    CHECK(SystemTicksFromNanoseconds(max_ns) == 4945790790759268688ULL);
    GuestMemory invalid;
    CHECK(!invalid.EnsureSharedClockPage(0, kDefaultRtcMsSince1900 - 1));
    CHECK(!invalid.EnsureSharedClockPage(0, kMaxRtcEpochMs + 1));
    CHECK(!invalid.IsMapped(kSharedPageBase));
    CHECK(invalid.EnsureSharedClockPage(max_ns, kMaxRtcEpochMs));
    CHECK(Wide(invalid, ActiveRecord(invalid)) == std::numeric_limits<std::uint64_t>::max());

    for (auto permission : {MemoryPermission::Read,
                            MemoryPermission::Read | MemoryPermission::Write}) {
        GuestMemory conflict;
        CHECK(conflict.Map(kSharedPageBase, kSharedPageSize, permission));
        const std::array<std::uint8_t,1> marker{0xa5};
        CHECK(conflict.LoadBytes(kSharedPageBase, marker));
        const auto before = Page(conflict);
        CHECK(!conflict.EnsureSharedClockPage(0));
        CHECK(Page(conflict) == before);
    }
    GuestMemory partial;
    CHECK(partial.Map(kSharedPageBase + 0xffc, 4, MemoryPermission::Read));
    CHECK(!partial.EnsureSharedClockPage(0));
    CHECK(!partial.IsMapped(kSharedPageBase));
}

a32::PackedOp Op(std::uint32_t raw, a32::Opcode code,
                 a32::Condition condition = a32::Condition::Al) {
    return {raw, a32::EncodeMetadata(code, condition)};
}

void MisclassifiedBarrierContinuesItsBlock() {
    using namespace a32;
    GuestMemory memory;
    GuestState state{};
    for (unsigned rt = 0; rt < 15; ++rt) {
        state = {};
        state.r[rt] = 0x12345678;
        state.cpsr = 0x90000010;
        state.fpscr = 0x03c00010;
        state.vfp[4] = 0xcafebabe;
        state.thread_pointer = kTlsAreaBase;
        const auto before = state;
        const std::array<PackedOp,2> ops{
            Op(0xee070fba | (rt << 12U), Opcode::CoreAlu),
            Op(0xef00007f, Opcode::Svc)};
        const Block block{0x100000, ops.data(), 2};
        const auto result = ExecuteBlock(block, state, memory, nullptr, nullptr);
        CHECK(result.kind == ExitKind::Svc && result.pc == 0x100004 && result.detail == 0x7f);
        auto expected = before.r; expected[15] = 0x100004;
        CHECK(state.r == expected && state.cpsr == before.cpsr);
        CHECK(state.fpscr == before.fpscr && state.vfp == before.vfp);
        CHECK(state.thread_pointer == before.thread_pointer);
    }
    // Valid conditional DMB is skipped when EQ is false; no special PC path.
    const std::array<PackedOp,2> conditional{
        Op(0x0e074fba, Opcode::CoreAlu, Condition::Eq), Op(0xef00007f, Opcode::Svc)};
    state = {};
    auto result = ExecuteBlock({0x100000, conditional.data(), 2}, state, memory, nullptr, nullptr);
    CHECK(result.kind == ExitKind::Svc && result.pc == 0x100004);
    for (const auto raw : {0xee074f9aU, 0xee07ffbaU, 0xfe074fbaU, 0xee074fbbU}) {
        const auto op = Op(raw, Opcode::CoreAlu);
        state = {};
        result = ExecuteBlock({0x100000, &op, 1}, state, memory, nullptr, nullptr);
        CHECK(result.kind == ExitKind::Fallback && result.pc == 0x100000);
        CHECK(result.detail == raw);
    }
}

void RunnerReadsClockWithoutAdvancingTime() {
    using namespace a32;
    static const std::array<PackedOp,3> ops{
        Op(0xe5902000, Opcode::Ldr32), // authored ldr r2,[r0]
        Op(0xee074fba, Opcode::CoreAlu), // archived wrong-category barrier shape
        Op(0xef00007f, Opcode::Svc)};
    static const Block block{0x100000, ops.data(), 3};
    static const BlockShard shard{0x100000, 0x100ffc, &block, 1};
    static const Registry registry{&shard, 1, nullptr, 0};
    Kernel kernel;
    GuestMemory memory;
    NativeRunner runner(registry, memory, kernel);
    CHECK(runner.InitializeMainThread());
    runner.live_state().r[0] = kSharedPageBase;
    auto result = runner.Run(10, 10);
    CHECK(result.reason == RunnerStopReason::UnsupportedSvc);
    CHECK(result.exit.pc == 0x100008 && runner.live_state().r[2] == 1);
    CHECK(kernel.now_ns() == 0);
    kernel.AdvanceTime(kSharedClockRefreshNs);
    runner.live_state().r[15] = 0x100000;
    runner.live_state().r[0] = kSharedPageBase;
    result = runner.Run(10, 10);
    CHECK(result.reason == RunnerStopReason::UnsupportedSvc);
    CHECK(runner.live_state().r[2] == 2);
    CHECK(kernel.now_ns() == kSharedClockRefreshNs);
    CHECK(Wide(memory, ActiveRecord(memory) + 8) == SvcTick(kernel));
}
}  // namespace

int main() {
    InitialPageAndProtection(); SharedTickAndHourlyPublication(); BoundsAndOwnership();
    MisclassifiedBarrierContinuesItsBlock(); RunnerReadsClockWithoutAdvancingTime();
    if (failures) return EXIT_FAILURE;
    std::cout << "PASS: read-only clock snapshots, tick consistency, DMB routing, and strict stops\n";
    return EXIT_SUCCESS;
}
