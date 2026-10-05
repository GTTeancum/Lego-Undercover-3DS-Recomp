#include "services/apt_service.h"
#include "runtime/ctr_svc_bridge.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
using namespace lego::ctr;
int failures = 0;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL " << __LINE__ << ": " #x "\n"; ++failures; } } while (0)

struct Fixture {
    Kernel kernel;
    GuestMemory memory;
    IpcRouter router;
    SvcBridge bridge{kernel, &router};
    std::shared_ptr<AptService> apt{std::make_shared<AptService>()};
    Handle session{};
    a32::GuestState cpu{};
    std::shared_ptr<EventObject> notification, parameter;
    Handle notification_handle{}, parameter_handle{};
    Fixture() {
        if (!memory.EnsureTlsMappings(kernel) ||
            router.RegisterService("APT:U", apt) != kResultSuccess ||
            router.ConnectToService(kernel, "APT:U", &session) != kResultSuccess)
            throw std::runtime_error("fixture setup failed");
        cpu = kernel.CurrentGuestState();
    }
    std::uint32_t Address() const { return kernel.current_thread()->tls_address + kIpcCommandBufferOffset; }
    IpcCommandBuffer Read(GuestMemory& source) const {
        IpcCommandBuffer result{};
        for (std::size_t i = 0; i < result.size(); ++i)
            CHECK(source.Read32(Address() + i * 4, &result[i]));
        return result;
    }
    void Put(const IpcCommandBuffer& request) {
        for (std::size_t i = 0; i < request.size(); ++i)
            CHECK(memory.Write32(Address() + i * 4, request[i]));
        cpu.r[0] = session;
        cpu.r[15] = 0x25947c;
    }
    a32::ExecutionResult Call(GuestMemory* target = nullptr) {
        return bridge.Handle({a32::ExitKind::Svc, cpu.r[15], a32::FallbackReason::None,
                              kSvcSendSyncRequest}, cpu, target ? target : &memory);
    }
    void Initialize() {
        Put({0x00020080, 0x300, 0});
        CHECK(Call().kind == a32::ExitKind::Fallthrough);
        const auto response = Read(memory);
        notification_handle = response[3]; parameter_handle = response[4];
        notification = std::dynamic_pointer_cast<EventObject>(kernel.handles().Get(notification_handle));
        parameter = std::dynamic_pointer_cast<EventObject>(kernel.handles().Get(parameter_handle));
        if (!notification || !parameter) throw std::runtime_error("missing APT event");
    }
    void CheckUnchangedNotify() {
        const auto queued = apt->pending_parameter();
        const auto current = kernel.current_thread();
        const auto status = current->status;
        const bool pending_wake = current->pending_wake;
        const bool ns = notification->signaled(), ps = parameter->signaled();
        const auto handles = kernel.handles().OpenHandleCount();
        const auto threads = kernel.threads().size();
        const auto time = kernel.now_ns();
        // Non-result registers and floating-point/exclusive state are sentinels.
        cpu.r[6] = 0xCAFEBABE; cpu.vfp[4] = 0x12345678;
        cpu.cpsr = 0xA0000010; cpu.fpscr = 0x03C00010;
        cpu.exclusive_address = 0x2000; cpu.exclusive_token = 12345;
        cpu.exclusive_size = 4; cpu.exclusive_valid = true;
        Put({0x00430040, 0x300});
        const auto before = cpu;
        const auto exit = Call();
        auto expected = before.r; expected[0] = 0; expected[15] += 4;
        CHECK(exit.kind == a32::ExitKind::Fallthrough && exit.pc == 0x259480);
        CHECK(cpu.r == expected && cpu.vfp == before.vfp);
        CHECK(cpu.cpsr == before.cpsr && cpu.fpscr == before.fpscr);
        CHECK(cpu.thread_pointer == before.thread_pointer);
        CHECK(cpu.exclusive_address == before.exclusive_address && cpu.exclusive_token == before.exclusive_token);
        CHECK(cpu.exclusive_size == before.exclusive_size && cpu.exclusive_valid == before.exclusive_valid);
        const IpcCommandBuffer response{0x00430040, 0};
        CHECK(Read(memory) == response);
        CHECK(router.last_session_name() == "APT:U" && router.last_request()[0] == 0x00430040);
        CHECK(!router.unsupported_request());
        CHECK(apt->initialized() && apt->registered());
        CHECK(apt->pending_parameter().has_value() == queued.has_value());
        if (queued) {
            const auto& after = *apt->pending_parameter();
            CHECK(after.sender_id == queued->sender_id && after.destination_id == queued->destination_id && after.signal == queued->signal);
        }
        CHECK(notification->signaled() == ns && parameter->signaled() == ps);
        CHECK(kernel.handles().OpenHandleCount() == handles && kernel.threads().size() == threads);
        CHECK(kernel.current_thread() == current && current->status == status && current->pending_wake == pending_wake);
        CHECK(kernel.now_ns() == time);
    }
};

void RepeatedAcknowledgmentDoesNotProduceOrConsumeEvents() {
    Fixture f;
    f.Initialize();
    CHECK(!f.notification->signaled() && f.parameter->signaled());
    f.CheckUnchangedNotify(); f.CheckUnchangedNotify();
    CHECK(f.kernel.WaitSynchronization1(f.parameter_handle, 0).result == kResultSuccess);
    f.CheckUnchangedNotify();
    CHECK(f.kernel.WaitSynchronization1(f.parameter_handle, 0).result == kResultTimeout);
    CHECK(f.kernel.CloseHandle(f.session) == kResultSuccess);
    CHECK(f.router.ConnectToService(f.kernel, "APT:U", &f.session) == kResultSuccess);
    f.CheckUnchangedNotify();
    CHECK(f.memory.Write32(f.Address() + 0x100, 2));
    CHECK(f.memory.Write32(f.Address() + 0x104, 0));
    f.Put({0x000D0080, 0x300, 0});
    CHECK(f.Call().kind == a32::ExitKind::Fallthrough);
    CHECK(!f.apt->pending_parameter());
    f.CheckUnchangedNotify(); f.CheckUnchangedNotify();
    f.Put({0x000E0080, 0x300, 0});
    CHECK(f.Call().kind == a32::ExitKind::Fallthrough);
    CHECK(f.Read(f.memory)[1] == kResultAptNoData);
}

void NotifyDoesNotWakeAnUnrelatedWaiter() {
    Fixture f;
    f.Initialize();
    Handle child_handle = 0;
    CHECK(f.kernel.CreateThread(&child_handle, 0x200000, 0, 0x08000000, 20, 0) == kResultSuccess);
    const auto child = std::dynamic_pointer_cast<ThreadObject>(f.kernel.handles().Get(child_handle));
    CHECK(f.kernel.Reschedule(f.cpu) && f.kernel.current_thread() == child);
    CHECK(f.kernel.WaitSynchronization1(f.notification_handle, -1).blocked);
    CHECK(f.kernel.Reschedule(f.cpu) && f.kernel.current_thread()->thread_id == 1);
    f.CheckUnchangedNotify();
    CHECK(child->status == ThreadStatus::WaitSynchAny && !child->pending_wake);
    CHECK(!f.notification->signaled());
}

void UnsupportedShapesAndPrematureNotifyStayStopped() {
    Fixture f;
    f.Put({0x00430040, 0x300});
    const auto before = f.cpu;
    CHECK(f.Call().kind == a32::ExitKind::Svc && f.cpu.r == before.r);
    CHECK(!f.apt->initialized() && !f.apt->registered());
    f.Initialize();
    const auto handles = f.kernel.handles().OpenHandleCount();
    for (const auto request : {IpcCommandBuffer{0x00430000}, IpcCommandBuffer{0x00430080, 0x300},
          IpcCommandBuffer{0x00430041, 0x300}, IpcCommandBuffer{0x00430040, 0},
          IpcCommandBuffer{0x00430040, 0x101}, IpcCommandBuffer{0x00430040, 0xFFFFFFFF},
          IpcCommandBuffer{0x004B00C2, 7, 4, 1}}) {
        f.Put(request);
        const auto cpu = f.cpu;
        CHECK(f.Call().kind == a32::ExitKind::Svc);
        CHECK(f.cpu.r == cpu.r && f.Read(f.memory) == request);
        CHECK(f.router.unsupported_request());
        CHECK(f.apt->registered() && f.apt->pending_parameter());
        CHECK(f.parameter->signaled() && !f.notification->signaled());
        CHECK(f.kernel.handles().OpenHandleCount() == handles && f.kernel.now_ns() == 0);
    }
}

void ResponsePreflightPreventsPartialMutation() {
    Fixture f;
    f.Initialize();
    const IpcCommandBuffer request{0x00430040, 0x300};
    for (const bool partial : {false, true}) {
        GuestMemory guarded;
        if (partial) {
            CHECK(guarded.Map(f.Address(), 0xFC, MemoryPermission::Read | MemoryPermission::Write));
            CHECK(guarded.Map(f.Address() + 0xFC, 4, MemoryPermission::Read));
            CHECK(guarded.LoadBytes(f.Address(), {reinterpret_cast<const std::uint8_t*>(request.data()), 0xFC}));
        } else {
            CHECK(guarded.Map(f.Address(), sizeof(request), MemoryPermission::Read));
            CHECK(guarded.LoadBytes(f.Address(), {reinterpret_cast<const std::uint8_t*>(request.data()), sizeof(request)}));
        }
        f.cpu.r[0] = f.session; f.cpu.r[15] = 0x25947c;
        const auto handles = f.kernel.handles().OpenHandleCount();
        CHECK(f.Call(&guarded).kind == a32::ExitKind::Fallthrough);
        CHECK(f.cpu.r[0] == kResultInvalidPointer && f.Read(guarded) == request);
        CHECK(f.apt->registered() && f.apt->pending_parameter());
        CHECK(f.parameter->signaled() && !f.notification->signaled());
        CHECK(f.kernel.handles().OpenHandleCount() == handles && f.kernel.now_ns() == 0);
    }
}
} // namespace
int main() {
    RepeatedAcknowledgmentDoesNotProduceOrConsumeEvents();
    NotifyDoesNotWakeAnUnrelatedWaiter();
    UnsupportedShapesAndPrematureNotifyStayStopped();
    ResponsePreflightPreventsPartialMutation();
    if (failures) return EXIT_FAILURE;
    std::cout << "PASS: bounded NotifyToWait HLE acknowledgment; no artificial message, event, or thread changes\n";
    return EXIT_SUCCESS;
}
