#include "services/apt_service.h"
#include "runtime/ctr_svc_bridge.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

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
    Fixture() {
        if (!memory.EnsureTlsMappings(kernel) ||
            router.RegisterService("APT:U", apt) != kResultSuccess ||
            router.ConnectToService(kernel, "APT:U", &session) != kResultSuccess)
            throw std::runtime_error("fixture initialization failed");
    }
    std::uint32_t Address() const { return kernel.current_thread()->tls_address + kIpcCommandBufferOffset; }
    IpcCommandBuffer Read() {
        IpcCommandBuffer result{};
        for (std::size_t i = 0; i < result.size(); ++i)
            CHECK(memory.Read32(Address() + i * 4, &result[i]));
        return result;
    }
    void Put(IpcCommandBuffer request) {
        for (std::size_t i = 0; i < request.size(); ++i)
            CHECK(memory.Write32(Address() + i * 4, request[i]));
        cpu.r[0] = session;
        cpu.r[15] = 0x25947c;
    }
    a32::ExecutionResult Call() {
        return bridge.Handle({a32::ExitKind::Svc, cpu.r[15], a32::FallbackReason::None,
                              kSvcSendSyncRequest}, cpu, &memory);
    }
};

Handle InitializeFixture(Fixture& f) {
    f.Put({0x00020080, 0x300, 0});
    CHECK(f.Call().kind == a32::ExitKind::Fallthrough);
    const auto response = f.Read();
    // Drain the actual parameter event once, independently of message reads.
    CHECK(f.kernel.WaitSynchronization1(response[4], 0).result == kResultSuccess);
    CHECK(f.kernel.WaitSynchronization1(response[4], 0).result == kResultTimeout);
    return response[4];
}

void SetReceiveBuffer(Fixture& f, std::uint32_t descriptor, std::uint32_t address) {
    CHECK(f.memory.Write32(f.Address() + 0x100, descriptor));
    CHECK(f.memory.Write32(f.Address() + 0x104, address));
}

void EnableIsIdempotentWithoutSyntheticWakeups() {
    Fixture f;
    CHECK(!f.apt->registered());
    f.Put({0x00020080, 0x300, 0});
    CHECK(f.Call().kind == a32::ExitKind::Fallthrough);
    const auto initialized = f.Read();
    const auto notification = std::dynamic_pointer_cast<EventObject>(
        f.kernel.handles().Get(initialized[3]));
    const auto parameter = std::dynamic_pointer_cast<EventObject>(
        f.kernel.handles().Get(initialized[4]));
    CHECK(notification && parameter);
    if (!notification || !parameter) return;
    CHECK(f.apt->initialized() && f.apt->registered()); // First app auto-enable.
    const auto handle_count = f.kernel.handles().OpenHandleCount();
    const auto thread_count = f.kernel.threads().size();
    const auto current = f.kernel.current_thread();
    const auto clock = f.kernel.now_ns();

    const auto enable = [&] {
        const bool was_pending = f.apt->pending_parameter().has_value();
        const bool notification_state = notification->signaled();
        const bool parameter_state = parameter->signaled();
        f.Put({0x00030040, 0});
        const auto before = f.cpu;
        CHECK(f.Call().kind == a32::ExitKind::Fallthrough);
        auto expected = before.r;
        expected[0] = 0;
        expected[15] += 4;
        CHECK(f.cpu.r == expected && f.cpu.cpsr == before.cpsr && f.cpu.fpscr == before.fpscr);
        IpcCommandBuffer response{0x00030040, 0};
        CHECK(f.Read() == response);
        CHECK(f.apt->registered() && f.apt->initialized());
        CHECK(f.apt->pending_parameter().has_value() == was_pending);
        if (was_pending) {
            const auto& message = *f.apt->pending_parameter();
            CHECK(message.sender_id == 0 && message.destination_id == 0x300 && message.signal == 1);
        }
        CHECK(notification->signaled() == notification_state);
        CHECK(parameter->signaled() == parameter_state);
        CHECK(f.kernel.handles().OpenHandleCount() == handle_count);
        CHECK(f.kernel.threads().size() == thread_count && f.kernel.current_thread() == current);
        CHECK(current->status == ThreadStatus::Running && !current->pending_wake);
        CHECK(f.kernel.now_ns() == clock);
    };
    enable(); enable(); // The queued Wakeup/event survives repeated Enable.
    CHECK(f.kernel.WaitSynchronization1(initialized[4], 0).result == kResultSuccess);
    enable(); // Event acquired; Enable must not signal it again.
    CHECK(!parameter->signaled());
    CHECK(f.kernel.WaitSynchronization1(initialized[4], 0).result == kResultTimeout);
    CHECK(f.kernel.WaitSynchronization1(initialized[3], 0).result == kResultTimeout);

    CHECK(f.kernel.CloseHandle(f.session) == 0);
    CHECK(f.router.ConnectToService(f.kernel, "APT:U", &f.session) == 0);
    enable(); // Registration, queued message and events are service-wide.
    SetReceiveBuffer(f, 2, 0); // Empty receive requires no destination mapping.
    f.Put({0x000D0080, 0x300, 0});
    CHECK(f.Call().kind == a32::ExitKind::Fallthrough && f.cpu.r[0] == 0);
    CHECK(!f.apt->pending_parameter());
    enable(); enable(); // Never recreate a consumed launch message.
    f.Put({0x000E0080, 0x300, 0});
    CHECK(f.Call().kind == a32::ExitKind::Fallthrough);
    CHECK(f.Read()[1] == kResultAptNoData);
}

void UnsupportedEnableShapesPreserveState() {
    Fixture f;
    // Before Initialize and for other attributes, this narrow application-only
    // model stops explicitly rather than claiming full AppletManager behavior.
    f.Put({0x00030040, 0});
    const auto before = f.cpu;
    CHECK(f.Call().kind == a32::ExitKind::Svc && f.cpu.r == before.r);
    CHECK(!f.apt->registered() && !f.apt->initialized() && !f.apt->pending_parameter());
    const auto event = InitializeFixture(f);
    const auto count = f.kernel.handles().OpenHandleCount();
    for (const auto request : {
             IpcCommandBuffer{0x00030000}, IpcCommandBuffer{0x00030080, 0, 0},
             IpcCommandBuffer{0x00030041, 0}, IpcCommandBuffer{0x00030040, 1},
             IpcCommandBuffer{0x00030040, 0x20}, IpcCommandBuffer{0x00030040, 0xFFFFFFFF},
             IpcCommandBuffer{0x00430040, 0x300}}) {
        f.Put(request);
        const auto cpu = f.cpu;
        CHECK(f.Call().kind == a32::ExitKind::Svc);
        CHECK(f.cpu.r == cpu.r && f.Read() == request);
        CHECK(f.router.unsupported_request());
        CHECK(f.apt->registered() && f.apt->pending_parameter());
        CHECK(f.kernel.handles().OpenHandleCount() == count);
        CHECK(f.kernel.WaitSynchronization1(event, 0).result == kResultTimeout);
        CHECK(f.kernel.now_ns() == 0);
    }
}

void ProtectedEnableResponseHasNoSideEffects() {
    Fixture f;
    const auto event = InitializeFixture(f);
    const auto count = f.kernel.handles().OpenHandleCount();
    for (const bool partial : {false, true}) {
        GuestMemory protected_memory;
        const IpcCommandBuffer request{0x00030040, 0};
        if (partial) {
            CHECK(protected_memory.Map(f.Address(), 0xFC, MemoryPermission::Read | MemoryPermission::Write));
            CHECK(protected_memory.Map(f.Address() + 0xFC, 4, MemoryPermission::Read));
            CHECK(protected_memory.LoadBytes(f.Address(), {
                reinterpret_cast<const std::uint8_t*>(request.data()), 0xFC}));
        } else {
            CHECK(protected_memory.Map(f.Address(), sizeof(request), MemoryPermission::Read));
            CHECK(protected_memory.LoadBytes(f.Address(), {
                reinterpret_cast<const std::uint8_t*>(request.data()), sizeof(request)}));
        }
        CHECK(f.router.SendSyncRequest(f.kernel, protected_memory, f.session) == kResultInvalidPointer);
        for (std::size_t i = 0; i < request.size(); ++i) {
            std::uint32_t value = 0;
            CHECK(protected_memory.Read32(f.Address() + i * 4, &value));
            CHECK(value == request[i]);
        }
        CHECK(f.apt->registered() && f.apt->pending_parameter());
        CHECK(f.kernel.handles().OpenHandleCount() == count);
        CHECK(f.kernel.WaitSynchronization1(event, 0).result == kResultTimeout);
        CHECK(f.kernel.current_thread()->status == ThreadStatus::Running && f.kernel.now_ns() == 0);
    }
}
} // namespace
int main() {
    EnableIsIdempotentWithoutSyntheticWakeups();
    UnsupportedEnableShapesPreserveState();
    ProtectedEnableResponseHasNoSideEffects();
    if (failures) return EXIT_FAILURE;
    std::cout << "PASS: APT Enable idempotence, initialization guards, and no synthetic wakeups\n";
    return EXIT_SUCCESS;
}
