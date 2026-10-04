#include "runtime/ctr_kernel.h"
#include "runtime/ctr_svc_bridge.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <unordered_map>

using lego::ctr::GenericObject;
using lego::ctr::Handle;
using lego::ctr::KernelObject;
using lego::ctr::MutexObject;
using lego::ctr::SemaphoreObject;
using lego::ctr::ThreadObject;
using lego::ctr::ThreadStatus;

namespace {

int failures = 0;

#define CHECK(expr)                                                                    \
    do {                                                                               \
        if (!(expr)) {                                                                 \
            std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << ": " #expr "\n"; \
            ++failures;                                                                \
        }                                                                              \
    } while (0)

class TestMemory final : public oot3d::recomp::a32::MemoryBus {
public:
    bool Read32(std::uint32_t address, std::uint32_t* value) override {
        const auto it = words.find(address);
        if (it == words.end())
            return false;
        *value = it->second;
        return true;
    }

    bool Write32(std::uint32_t address, std::uint32_t value) override {
        words[address] = value;
        return true;
    }

    std::unordered_map<std::uint32_t, std::uint32_t> words;
};

oot3d::recomp::a32::ExecutionResult Svc(std::uint32_t id,
                                        std::uint32_t pc = 0x00100000U) {
    return {
        oot3d::recomp::a32::ExitKind::Svc,
        pc,
        oot3d::recomp::a32::FallbackReason::None,
        id,
    };
}

void TestPseudoHandlesAndDuplicate() {
    lego::ctr::Kernel kernel(7, 11);
    auto thread = kernel.handles().Get(lego::ctr::kCurrentThreadPseudoHandle);
    auto process = kernel.handles().Get(lego::ctr::kCurrentProcessPseudoHandle);
    CHECK(thread == kernel.current_thread());
    CHECK(process == kernel.current_process());

    Handle duplicate = 0;
    CHECK(kernel.DuplicateHandle(&duplicate, lego::ctr::kCurrentThreadPseudoHandle) ==
          lego::ctr::kResultSuccess);
    CHECK(duplicate == 1U);
    CHECK(kernel.handles().IsValid(duplicate));
    CHECK(kernel.handles().Get(duplicate) == thread);
    CHECK(kernel.CloseHandle(duplicate) == lego::ctr::kResultSuccess);
    CHECK(!kernel.handles().IsValid(duplicate));
}

void TestGenerationProtectsStaleHandles() {
    lego::ctr::Kernel kernel;
    Handle first = 0;
    CHECK(kernel.handles().Create(
              &first, std::make_shared<GenericObject>(KernelObject::Type::Event)) ==
          lego::ctr::kResultSuccess);
    CHECK(kernel.CloseHandle(first) == lego::ctr::kResultSuccess);

    Handle second = 0;
    CHECK(kernel.handles().Create(
              &second, std::make_shared<GenericObject>(KernelObject::Type::Event)) ==
          lego::ctr::kResultSuccess);
    CHECK(first != second);
    CHECK(!kernel.handles().IsValid(first));
    CHECK(kernel.handles().IsValid(second));
    CHECK((first >> 15U) == (second >> 15U));
}

void TestEventResetModes() {
    lego::ctr::Kernel kernel;
    Handle one_shot = 0;
    Handle sticky = 0;
    Handle pulse = 0;
    CHECK(kernel.CreateEvent(&one_shot, 0) == lego::ctr::kResultSuccess);
    CHECK(kernel.CreateEvent(&sticky, 1) == lego::ctr::kResultSuccess);
    CHECK(kernel.CreateEvent(&pulse, 2) == lego::ctr::kResultSuccess);

    CHECK(kernel.SignalEvent(one_shot) == lego::ctr::kResultSuccess);
    CHECK(kernel.WaitSynchronization1(one_shot, 0).result == lego::ctr::kResultSuccess);
    CHECK(kernel.WaitSynchronization1(one_shot, 0).result == lego::ctr::kResultTimeout);

    CHECK(kernel.SignalEvent(sticky) == lego::ctr::kResultSuccess);
    CHECK(kernel.WaitSynchronization1(sticky, 0).result == lego::ctr::kResultSuccess);
    CHECK(kernel.WaitSynchronization1(sticky, 0).result == lego::ctr::kResultSuccess);
    CHECK(kernel.ClearEvent(sticky) == lego::ctr::kResultSuccess);
    CHECK(kernel.WaitSynchronization1(sticky, 0).result == lego::ctr::kResultTimeout);

    CHECK(kernel.SignalEvent(pulse) == lego::ctr::kResultSuccess);
    CHECK(kernel.WaitSynchronization1(pulse, 0).result == lego::ctr::kResultTimeout);
}

void TestBlockedWaitSignalAndTimeout() {
    {
        lego::ctr::Kernel kernel;
        Handle event = 0;
        CHECK(kernel.CreateEvent(&event, 0) == lego::ctr::kResultSuccess);
        const auto out = kernel.WaitSynchronization1(event, 100);
        CHECK(out.result == lego::ctr::kResultTimeout);
        CHECK(out.blocked);
        CHECK(kernel.current_thread()->status == ThreadStatus::WaitSynchAny);
        CHECK(kernel.SignalEvent(event) == lego::ctr::kResultSuccess);
        CHECK(kernel.current_thread()->status == ThreadStatus::Ready);

        lego::ctr::Result result = 0;
        std::int32_t index = -7;
        bool valid = true;
        CHECK(kernel.ConsumeCurrentThreadWake(&result, &index, &valid));
        CHECK(result == lego::ctr::kResultSuccess);
        CHECK(!valid);
    }

    {
        lego::ctr::Kernel kernel;
        Handle event = 0;
        CHECK(kernel.CreateEvent(&event, 0) == lego::ctr::kResultSuccess);
        const auto out = kernel.WaitSynchronization1(event, 50);
        CHECK(out.blocked);
        kernel.AdvanceTime(49);
        CHECK(kernel.current_thread()->status == ThreadStatus::WaitSynchAny);
        kernel.AdvanceTime(1);
        CHECK(kernel.current_thread()->status == ThreadStatus::Ready);

        lego::ctr::Result result = 0;
        std::int32_t index = 99;
        bool valid = true;
        CHECK(kernel.ConsumeCurrentThreadWake(&result, &index, &valid));
        CHECK(result == lego::ctr::kResultTimeout);
        CHECK(!valid);
        CHECK(index == -1);
    }
}

void TestWaitAnyAndAll() {
    {
        lego::ctr::Kernel kernel;
        Handle e0 = 0;
        Handle e1 = 0;
        kernel.CreateEvent(&e0, 1);
        kernel.CreateEvent(&e1, 1);
        kernel.SignalEvent(e1);
        Handle handles[] = {e0, e1};

        const auto out = kernel.WaitSynchronizationN(handles, false, 0);
        CHECK(out.result == lego::ctr::kResultSuccess);
        CHECK(out.index_valid);
        CHECK(out.index == 1);
    }

    {
        lego::ctr::Kernel kernel;
        Handle e0 = 0;
        Handle e1 = 0;
        kernel.CreateEvent(&e0, 1);
        kernel.CreateEvent(&e1, 1);
        kernel.SignalEvent(e0);
        Handle handles[] = {e0, e1};

        const auto out = kernel.WaitSynchronizationN(handles, true, 1000);
        CHECK(out.blocked);
        CHECK(kernel.current_thread()->status == ThreadStatus::WaitSynchAll);
        kernel.SignalEvent(e1);
        CHECK(kernel.current_thread()->status == ThreadStatus::Ready);

        lego::ctr::Result result = 0;
        std::int32_t index = 123;
        bool valid = true;
        CHECK(kernel.ConsumeCurrentThreadWake(&result, &index, &valid));
        CHECK(result == lego::ctr::kResultSuccess);
        CHECK(!valid);
    }
}

void TestMutexAndSemaphore() {
    lego::ctr::Kernel kernel;
    Handle mutex_handle = 0;
    CHECK(kernel.CreateMutex(&mutex_handle, true) == lego::ctr::kResultSuccess);
    auto mutex = std::dynamic_pointer_cast<MutexObject>(kernel.handles().Get(mutex_handle));
    CHECK(mutex != nullptr);
    CHECK(mutex->lock_count() == 1);

    mutex->Acquire(*kernel.current_thread());
    CHECK(mutex->lock_count() == 2);
    CHECK(kernel.ReleaseMutex(mutex_handle) == lego::ctr::kResultSuccess);
    CHECK(mutex->lock_count() == 1);
    CHECK(kernel.ReleaseMutex(mutex_handle) == lego::ctr::kResultSuccess);
    CHECK(mutex->lock_count() == 0);

    Handle child_handle = 0;
    CHECK(kernel.CreateThread(&child_handle, 0x00200000, 0, 0x08000000, 32, 0) ==
          lego::ctr::kResultSuccess);
    auto child = std::dynamic_pointer_cast<ThreadObject>(kernel.handles().Get(child_handle));
    CHECK(child != nullptr);

    mutex->Acquire(*kernel.current_thread());
    CHECK(mutex->ShouldWait(*child));
    CHECK(mutex->Release(*child) == lego::ctr::kResultWrongLockingThread);
    CHECK(kernel.ReleaseMutex(mutex_handle) == lego::ctr::kResultSuccess);

    Handle semaphore_handle = 0;
    CHECK(kernel.CreateSemaphore(&semaphore_handle, 0, 2) == lego::ctr::kResultSuccess);
    CHECK(kernel.WaitSynchronization1(semaphore_handle, 0).result ==
          lego::ctr::kResultTimeout);

    std::int32_t previous = -1;
    CHECK(kernel.ReleaseSemaphore(&previous, semaphore_handle, 1) ==
          lego::ctr::kResultSuccess);
    CHECK(previous == 0);
    CHECK(kernel.WaitSynchronization1(semaphore_handle, 0).result ==
          lego::ctr::kResultSuccess);

    auto semaphore =
        std::dynamic_pointer_cast<SemaphoreObject>(kernel.handles().Get(semaphore_handle));
    CHECK(semaphore->available_count() == 0);
    CHECK(kernel.ReleaseSemaphore(&previous, semaphore_handle, 3) ==
          lego::ctr::kResultOutOfRangeKernel);
}

void TestThreadCreateAndSleep() {
    lego::ctr::Kernel kernel;
    Handle thread_handle = 0;
    CHECK(kernel.CreateThread(&thread_handle, 0x00123400, 0x55, 0x07FFF000, 20, -1) ==
          lego::ctr::kResultSuccess);

    auto thread = std::dynamic_pointer_cast<ThreadObject>(kernel.handles().Get(thread_handle));
    CHECK(thread != nullptr);
    CHECK(thread->status == ThreadStatus::Ready);
    CHECK(thread->entry_point == 0x00123400U);
    CHECK(thread->argument == 0x55U);
    CHECK(thread->priority == 20U);
    CHECK(thread->processor_id == 0);
    CHECK(kernel.HighestPriorityReadyThread() == thread);

    CHECK(kernel.SleepCurrentThread(100));
    CHECK(kernel.current_thread()->status == ThreadStatus::WaitSleep);
    kernel.AdvanceTime(100);
    CHECK(kernel.current_thread()->status == ThreadStatus::Ready);
}

void TestSvcAbisAndWake() {
    lego::ctr::Kernel kernel;
    lego::ctr::SvcBridge bridge(kernel);
    oot3d::recomp::a32::GuestState state{};

    state.r[1] = 0;
    auto result = bridge.Handle(Svc(lego::ctr::kSvcCreateEvent), state);
    CHECK(result.kind == oot3d::recomp::a32::ExitKind::Fallthrough);
    CHECK(state.r[0] == lego::ctr::kResultSuccess);
    const Handle event = state.r[1];

    state.r[0] = event;
    state.r[2] = 100;
    state.r[3] = 0;
    result =
        bridge.Handle(Svc(lego::ctr::kSvcWaitSynchronization1, 0x00110000), state);
    CHECK(result.kind == oot3d::recomp::a32::ExitKind::Wait);
    CHECK(state.r[0] == lego::ctr::kResultTimeout);
    CHECK(state.r[15] == 0x00110004U);
    CHECK(kernel.SignalEvent(event) == lego::ctr::kResultSuccess);
    CHECK(bridge.ApplyPendingWake(state));
    CHECK(state.r[0] == lego::ctr::kResultSuccess);

    state.r[1] = 0;
    state.r[2] = 2;
    bridge.Handle(Svc(lego::ctr::kSvcCreateSemaphore), state);
    CHECK(state.r[0] == lego::ctr::kResultSuccess);
    const Handle semaphore = state.r[1];

    state.r[1] = semaphore;
    state.r[2] = 1;
    bridge.Handle(Svc(lego::ctr::kSvcReleaseSemaphore), state);
    CHECK(state.r[0] == lego::ctr::kResultSuccess);
    CHECK(state.r[1] == 0U);

    state.r[0] = 30;
    state.r[1] = 0x00200000;
    state.r[2] = 0xABC;
    state.r[3] = 0x07FFE000;
    state.r[4] = static_cast<std::uint32_t>(-1);
    bridge.Handle(Svc(lego::ctr::kSvcCreateThread), state);
    CHECK(state.r[0] == lego::ctr::kResultSuccess);
    auto child = std::dynamic_pointer_cast<ThreadObject>(kernel.handles().Get(state.r[1]));
    CHECK(child != nullptr);
    CHECK(child->priority == 30U);
    CHECK(child->entry_point == 0x00200000U);
}

void TestWaitNSvcMemoryAbi() {
    lego::ctr::Kernel kernel;
    lego::ctr::SvcBridge bridge(kernel);
    TestMemory memory;

    Handle e0 = 0;
    Handle e1 = 0;
    kernel.CreateEvent(&e0, 1);
    kernel.CreateEvent(&e1, 1);
    kernel.SignalEvent(e1);
    memory.words[0x2000] = e0;
    memory.words[0x2004] = e1;

    oot3d::recomp::a32::GuestState state{};
    state.r[0] = 0;
    state.r[1] = 0x2000;
    state.r[2] = 2;
    state.r[3] = 0;
    state.r[4] = 0;

    const auto result =
        bridge.Handle(Svc(lego::ctr::kSvcWaitSynchronizationN), state, &memory);
    CHECK(result.kind == oot3d::recomp::a32::ExitKind::Fallthrough);
    CHECK(state.r[0] == lego::ctr::kResultSuccess);
    CHECK(state.r[1] == 1U);
}


void TestGuestContextSwitchAndTls() {
    lego::ctr::Kernel kernel;

    oot3d::recomp::a32::GuestState main_state{};
    main_state.r[0] = 0x11111111U;
    main_state.r[4] = 0x44444444U;
    main_state.r[13] = 0x08000000U;
    main_state.r[15] = 0x00100000U;
    main_state.cpsr = lego::ctr::kUserModeCpsr;
    main_state.fpscr = 0x03C00010U;
    main_state.vfp[3] = 0xDEADBEEFU;
    main_state.exclusive_address = 0x12340000U;
    main_state.exclusive_token = 0x1122334455667788ULL;
    main_state.exclusive_size = 4;
    main_state.exclusive_valid = true;
    kernel.SetCurrentGuestState(main_state);

    CHECK(kernel.current_thread()->tls_address == lego::ctr::kTlsAreaBase);
    CHECK(kernel.CurrentGuestState().thread_pointer == lego::ctr::kTlsAreaBase);

    Handle child_handle = 0;
    CHECK(kernel.CreateThread(&child_handle, 0x00200000U, 0xA5A5U,
                              0x07FFF000U, 20U, 0) ==
          lego::ctr::kResultSuccess);
    auto child =
        std::dynamic_pointer_cast<ThreadObject>(kernel.handles().Get(child_handle));
    CHECK(child != nullptr);
    CHECK(child->tls_address ==
          lego::ctr::kTlsAreaBase + lego::ctr::kTlsEntrySize);
    CHECK(child->guest_state.r[0] == 0xA5A5U);
    CHECK(child->guest_state.r[13] == 0x07FFF000U);
    CHECK(child->guest_state.r[15] == 0x00200000U);
    CHECK(child->guest_state.cpsr == lego::ctr::kUserModeCpsr);
    CHECK(child->guest_state.fpscr == lego::ctr::kThreadInitialFpscr);
    CHECK(child->guest_state.thread_pointer == child->tls_address);

    oot3d::recomp::a32::GuestState live = kernel.CurrentGuestState();
    CHECK(kernel.Reschedule(live));
    CHECK(kernel.current_thread() == child);
    CHECK(kernel.handles().Get(lego::ctr::kCurrentThreadPseudoHandle) == child);
    CHECK(live.r[0] == 0xA5A5U);
    CHECK(live.r[13] == 0x07FFF000U);
    CHECK(live.thread_pointer == child->tls_address);

    live.r[5] = 0x5555AAAAU;
    live.vfp[7] = 0xCAFEBABEU;
    live.fpscr = 0x01234567U;
    live.exclusive_address = 0x2000U;
    live.exclusive_token = 0x8877665544332211ULL;
    live.exclusive_size = 8;
    live.exclusive_valid = true;

    CHECK(kernel.SleepCurrentThread(100));
    CHECK(kernel.Reschedule(live));
    CHECK(kernel.current_thread()->thread_id == 1U);
    CHECK(live.r[0] == 0x11111111U);
    CHECK(live.r[4] == 0x44444444U);
    CHECK(live.vfp[3] == 0xDEADBEEFU);
    CHECK(live.exclusive_valid);
    CHECK(live.exclusive_token == 0x1122334455667788ULL);
    CHECK(live.thread_pointer == lego::ctr::kTlsAreaBase);

    kernel.AdvanceTime(100);
    live.r[6] = 0x66666666U;
    CHECK(kernel.Reschedule(live));
    CHECK(kernel.current_thread() == child);
    CHECK(live.r[5] == 0x5555AAAAU);
    CHECK(live.vfp[7] == 0xCAFEBABEU);
    CHECK(live.fpscr == 0x01234567U);
    CHECK(live.exclusive_valid);
    CHECK(live.exclusive_token == 0x8877665544332211ULL);
    CHECK(live.thread_pointer == child->tls_address);

    Handle thumb_handle = 0;
    CHECK(kernel.CreateThread(&thumb_handle, 0x00210001U, 0,
                              0x07FFE000U, 30U, 0) ==
          lego::ctr::kResultSuccess);
    auto thumb =
        std::dynamic_pointer_cast<ThreadObject>(kernel.handles().Get(thumb_handle));
    CHECK(thumb != nullptr);
    CHECK(thumb->guest_state.r[15] == 0x00210000U);
    CHECK((thumb->guest_state.cpsr & (1U << 5U)) != 0U);
}

void TestBlockedThreadContextResume() {
    lego::ctr::Kernel kernel;
    lego::ctr::SvcBridge bridge(kernel);

    Handle child_handle = 0;
    CHECK(kernel.CreateThread(&child_handle, 0x00200000U, 0x44U,
                              0x07FFF000U, 20U, 0) ==
          lego::ctr::kResultSuccess);
    auto child =
        std::dynamic_pointer_cast<ThreadObject>(kernel.handles().Get(child_handle));
    CHECK(child != nullptr);

    oot3d::recomp::a32::GuestState live = kernel.CurrentGuestState();
    CHECK(kernel.Reschedule(live));
    CHECK(kernel.current_thread() == child);

    Handle event = 0;
    CHECK(kernel.CreateEvent(&event, 0) == lego::ctr::kResultSuccess);

    live.r[0] = event;
    live.r[2] = 1000U;
    live.r[3] = 0U;
    const auto wait_exit =
        bridge.Handle(Svc(lego::ctr::kSvcWaitSynchronization1, 0x00201000U),
                      live);
    CHECK(wait_exit.kind == oot3d::recomp::a32::ExitKind::Wait);
    CHECK(live.r[0] == lego::ctr::kResultTimeout);
    CHECK(live.r[15] == 0x00201004U);
    CHECK(child->status == ThreadStatus::WaitSynchAny);

    CHECK(kernel.Reschedule(live));
    CHECK(kernel.current_thread()->thread_id == 1U);

    CHECK(kernel.SignalEvent(event) == lego::ctr::kResultSuccess);
    CHECK(child->status == ThreadStatus::Ready);

    CHECK(kernel.Reschedule(live));
    CHECK(kernel.current_thread() == child);
    CHECK(live.r[0] == lego::ctr::kResultSuccess);
    CHECK(live.r[15] == 0x00201004U);
    CHECK(!child->pending_wake);
    CHECK(live.thread_pointer == child->tls_address);
}

void TestDuplicateSvcStage2Shape() {
    lego::ctr::Kernel kernel(1, 1);
    lego::ctr::SvcBridge bridge(kernel);
    oot3d::recomp::a32::GuestState state{};
    state.r[1] = lego::ctr::kCurrentThreadPseudoHandle;

    const auto resumed =
        bridge.Handle(Svc(lego::ctr::kSvcDuplicateHandle, 0x0013313C), state);
    CHECK(resumed.kind == oot3d::recomp::a32::ExitKind::Fallthrough);
    CHECK(resumed.pc == 0x00133140U);
    CHECK(state.r[0] == lego::ctr::kResultSuccess);
    CHECK(state.r[1] != lego::ctr::kCurrentThreadPseudoHandle);
}

}  // namespace

int main() {
    TestPseudoHandlesAndDuplicate();
    TestGenerationProtectsStaleHandles();
    TestEventResetModes();
    TestBlockedWaitSignalAndTimeout();
    TestWaitAnyAndAll();
    TestMutexAndSemaphore();
    TestThreadCreateAndSleep();
    TestSvcAbisAndWake();
    TestWaitNSvcMemoryAbi();
    TestGuestContextSwitchAndTls();
    TestBlockedThreadContextResume();
    TestDuplicateSvcStage2Shape();

    if (failures != 0) {
        std::cerr << failures << " CTR runtime checks failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "PASS: CTR handle/thread/sync/SVC checks\n";
    return EXIT_SUCCESS;
}
