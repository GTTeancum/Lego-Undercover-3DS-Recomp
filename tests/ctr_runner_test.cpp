#include "runtime/ctr_kernel.h"
#include "runtime/ctr_memory.h"
#include "runtime/ctr_runner.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

int failures = 0;

#define CHECK(expr)                                                                    \
    do {                                                                               \
        if (!(expr)) {                                                                 \
            std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << ": " #expr "\n"; \
            ++failures;                                                                \
        }                                                                              \
    } while (0)

using lego::ctr::GuestMemory;
using lego::ctr::Handle;
using lego::ctr::Kernel;
using lego::ctr::MemoryPermission;
using lego::ctr::NativeRunner;
using lego::ctr::RunnerStopReason;
using lego::ctr::ThreadObject;

void TestPreparedImageLayout() {
    std::vector<std::uint8_t> code(lego::ctr::kPreparedCodeBytes, 0);
    code[0] = 0x11;
    code[1] = 0x22;
    code[2] = 0x33;
    code[3] = 0x44;

    const std::size_t rodata_offset = lego::ctr::kTextAllocatedBytes;
    code[rodata_offset + 0] = 0x55;
    code[rodata_offset + 1] = 0x66;
    code[rodata_offset + 2] = 0x77;
    code[rodata_offset + 3] = 0x88;

    const std::size_t data_offset =
        lego::ctr::kTextAllocatedBytes + lego::ctr::kRodataAllocatedBytes;
    code[data_offset + 0] = 0xAA;
    code[data_offset + 1] = 0xBB;
    code[data_offset + 2] = 0xCC;
    code[data_offset + 3] = 0xDD;

    // Deliberately put non-zero bytes into the data allocation where BSS
    // begins. The loader must zero the documented BSS range afterward.
    const std::size_t bss_code_offset =
        data_offset + (lego::ctr::kBssBegin - lego::ctr::kDataBase);
    code[bss_code_offset] = 0xFE;

    GuestMemory memory;
    CHECK(memory.LoadLegoCodeImage(code));

    std::uint32_t value = 0;
    CHECK(memory.Read32(lego::ctr::kTextBase, &value));
    CHECK(value == 0x44332211U);
    CHECK(!memory.Write32(lego::ctr::kTextBase, 0x12345678U));

    CHECK(memory.Read32(lego::ctr::kRodataBase, &value));
    CHECK(value == 0x88776655U);
    CHECK(!memory.Write32(lego::ctr::kRodataBase, 0x12345678U));

    CHECK(memory.Read32(lego::ctr::kDataBase, &value));
    CHECK(value == 0xDDCCBBAAU);
    CHECK(memory.Write32(lego::ctr::kDataBase, 0xAABBCCDDU));
    CHECK(memory.Read32(lego::ctr::kDataBase, &value));
    CHECK(value == 0xAABBCCDDU);

    std::uint8_t bss = 0xFF;
    CHECK(memory.Read8(lego::ctr::kBssBegin, &bss));
    CHECK(bss == 0);

    CHECK(memory.IsMapped(lego::ctr::kMainStackTop -
                          lego::ctr::kMainStackBytes,
                          lego::ctr::kMainStackBytes));
    CHECK(memory.IsWritable(lego::ctr::kMainStackTop - 4U, 4U));
}

void TestTlsPagesAndExclusiveMemory() {
    Kernel kernel;
    GuestMemory memory;
    CHECK(memory.EnsureTlsMappings(kernel));
    CHECK(memory.IsWritable(lego::ctr::kTlsAreaBase, lego::ctr::kPageSize));

    for (int index = 0; index < 8; ++index) {
        Handle handle = 0;
        CHECK(kernel.CreateThread(&handle, 0x00200000U + index * 4U, 0,
                                  0x07FFF000U - index * 0x1000U,
                                  30U, 0) == lego::ctr::kResultSuccess);
    }
    CHECK(memory.EnsureTlsMappings(kernel));
    CHECK(memory.IsWritable(lego::ctr::kTlsAreaBase + lego::ctr::kPageSize,
                            lego::ctr::kPageSize));

    CHECK(memory.Map(0x20000000U, 0x1000U,
                     MemoryPermission::Read | MemoryPermission::Write));
    CHECK(memory.Write32(0x20000000U, 0x11223344U));

    std::uint64_t loaded = 0;
    std::uint64_t token = 0;
    std::uint32_t fault = 0;
    CHECK(memory.LoadExclusive(0x20000000U, 4U, &loaded, &token, &fault));
    CHECK(loaded == 0x11223344U);

    CHECK(memory.Write8(0x20000001U, 0x99U));
    CHECK(memory.StoreExclusive(0x20000000U, 4U, 0xAABBCCDDU, token, &fault) ==
          oot3d::recomp::a32::ExclusiveStoreResult::ReservationLost);

    CHECK(memory.LoadExclusive(0x20000000U, 4U, &loaded, &token, &fault));
    CHECK(memory.StoreExclusive(0x20000000U, 4U, 0xAABBCCDDU, token, &fault) ==
          oot3d::recomp::a32::ExclusiveStoreResult::Success);

    std::uint32_t value = 0;
    CHECK(memory.Read32(0x20000000U, &value));
    CHECK(value == 0xAABBCCDDU);
}

constexpr oot3d::recomp::a32::PackedOp SvcOp(std::uint32_t number) {
    return {
        0xEF000000U | number,
        oot3d::recomp::a32::EncodeMetadata(
            oot3d::recomp::a32::Opcode::Svc,
            oot3d::recomp::a32::Condition::Al),
    };
}

void TestDispatchSvcRescheduleLoop() {
    using namespace oot3d::recomp::a32;

    static constexpr PackedOp main_wait_ops[] = {SvcOp(lego::ctr::kSvcWaitSynchronization1)};
    static constexpr PackedOp main_exit_ops[] = {SvcOp(lego::ctr::kSvcExitThread)};
    static constexpr PackedOp child_signal_ops[] = {SvcOp(lego::ctr::kSvcSignalEvent)};
    static constexpr PackedOp child_exit_ops[] = {SvcOp(lego::ctr::kSvcExitThread)};

    static const Block main_blocks[] = {
        {0x00100000U, main_wait_ops, 1U},
        {0x00100004U, main_exit_ops, 1U},
    };
    static const Block child_blocks[] = {
        {0x00200000U, child_signal_ops, 1U},
        {0x00200004U, child_exit_ops, 1U},
    };
    static const BlockShard shards[] = {
        {0x00100000U, 0x00100FFCU, main_blocks, 2U},
        {0x00200000U, 0x00200FFCU, child_blocks, 2U},
    };
    static const Registry registry{shards, 2U, nullptr, 0U};

    Kernel kernel;
    GuestMemory memory;
    CHECK(memory.EnsureMainStack());
    CHECK(memory.EnsureTlsMappings(kernel));

    Handle event = 0;
    CHECK(kernel.CreateEvent(&event, 0) == lego::ctr::kResultSuccess);

    NativeRunner runner(registry, memory, kernel);
    CHECK(runner.InitializeMainThread(0x00100000U));

    runner.live_state().r[0] = event;
    runner.live_state().r[2] = 0xFFFFFFFFU;
    runner.live_state().r[3] = 0xFFFFFFFFU;
    kernel.SetCurrentGuestState(runner.live_state());

    Handle child_handle = 0;
    CHECK(kernel.CreateThread(&child_handle, 0x00200000U, 0,
                              0x07FFF000U, 20U, 0) ==
          lego::ctr::kResultSuccess);
    auto child = std::dynamic_pointer_cast<ThreadObject>(
        kernel.handles().Get(child_handle));
    CHECK(child != nullptr);
    child->guest_state.r[0] = event;

    CHECK(memory.EnsureTlsMappings(kernel));

    const auto result = runner.Run(100U, 32U);
    CHECK(result.reason == RunnerStopReason::ProcessExited);
    CHECK(result.dispatch_rounds >= 4U);

    CHECK(kernel.current_thread()->status == lego::ctr::ThreadStatus::Dead);
    CHECK(kernel.threads()[0]->status == lego::ctr::ThreadStatus::Dead);
    CHECK(child->status == lego::ctr::ThreadStatus::Dead);

    // Main resumed after its blocked wait with ResultSuccess before executing
    // its ExitThread block.
    CHECK(kernel.threads()[0]->guest_state.r[0] == lego::ctr::kResultSuccess);
    CHECK(kernel.threads()[0]->guest_state.r[15] == 0x00100008U);

    CHECK(memory.IsWritable(lego::ctr::kTlsAreaBase, lego::ctr::kPageSize));
}

void TestRunnerStopsOnUnsupportedSvc() {
    using namespace oot3d::recomp::a32;

    static constexpr PackedOp unsupported_ops[] = {SvcOp(0x7FU)};
    static const Block blocks[] = {
        {0x00100000U, unsupported_ops, 1U},
    };
    static const BlockShard shards[] = {
        {0x00100000U, 0x00100FFCU, blocks, 1U},
    };
    static const Registry registry{shards, 1U, nullptr, 0U};

    Kernel kernel;
    GuestMemory memory;
    NativeRunner runner(registry, memory, kernel);
    CHECK(runner.InitializeMainThread());

    const auto result = runner.Run(100U, 4U);
    CHECK(result.reason == RunnerStopReason::UnsupportedSvc);
    CHECK(result.exit.kind == ExitKind::Svc);
    CHECK(result.exit.detail == 0x7FU);
}

}  // namespace

int main() {
    TestPreparedImageLayout();
    TestTlsPagesAndExclusiveMemory();
    TestDispatchSvcRescheduleLoop();
    TestRunnerStopsOnUnsupportedSvc();

    if (failures != 0) {
        std::cerr << failures << " CTR runner/memory checks failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "PASS: CTR guest-memory/AOT runner checks\n";
    return EXIT_SUCCESS;
}
