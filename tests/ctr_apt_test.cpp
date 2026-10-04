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

void LockIdentityAndOwnership() {
    Fixture f;
    f.Put({0x00010040, 0});
    CHECK(f.Call().kind == a32::ExitKind::Fallthrough);
    CHECK(f.cpu.r[0] == 0 && f.cpu.r[15] == 0x259480);
    const auto a = f.Read();
    CHECK(a[0] == 0x000100c2 && a[1] == 0 && a[2] == 0 && a[3] == 0 && a[4] == 0);
    const auto lock = std::dynamic_pointer_cast<MutexObject>(f.kernel.handles().Get(a[5]));
    CHECK(lock != nullptr && lock->lock_count() == 0);
    CHECK(f.kernel.WaitSynchronization1(a[5], 0).result == kResultSuccess);
    CHECK(lock->holding_thread() == f.kernel.current_thread());
    CHECK(f.kernel.ReleaseMutex(a[5]) == kResultSuccess);

    CHECK(f.kernel.CloseHandle(f.session) == kResultSuccess);
    CHECK(f.router.ConnectToService(f.kernel, "APT:U", &f.session) == 0);
    f.Put({0x00010040, 0});
    f.Call();
    const auto b = f.Read();
    CHECK(a[5] != b[5]);
    CHECK(f.kernel.handles().Get(b[5]) == lock);
    CHECK(f.kernel.CloseHandle(a[5]) == 0);
    CHECK(f.kernel.handles().Get(b[5]) == lock);
    CHECK(!f.apt->initialized() && !f.apt->pending_parameter());
}

void InitializeProducesQueuedWakeup() {
    Fixture f;
    f.Put({0x00020080, 0x300, 0});
    CHECK(f.Call().kind == a32::ExitKind::Fallthrough);
    const auto reply = f.Read();
    CHECK(reply[0] == 0x00020043 && reply[1] == 0 && reply[2] == 0x04000000);
    const auto notification = std::dynamic_pointer_cast<EventObject>(f.kernel.handles().Get(reply[3]));
    const auto parameter = std::dynamic_pointer_cast<EventObject>(f.kernel.handles().Get(reply[4]));
    CHECK(notification && parameter && notification != parameter);
    CHECK(!notification->signaled() && parameter->signaled());
    CHECK(notification->reset_type() == ResetType::OneShot && parameter->reset_type() == ResetType::OneShot);
    CHECK(f.apt->initialized() && f.apt->pending_parameter().has_value());
    const auto pending = *f.apt->pending_parameter();
    CHECK(pending.sender_id == 0 && pending.destination_id == 0x300 && pending.signal == 1);
    CHECK(f.kernel.WaitSynchronization1(reply[3], 0).result == kResultTimeout);
    CHECK(f.kernel.WaitSynchronization1(reply[4], 0).result == kResultSuccess);
    CHECK(f.kernel.WaitSynchronization1(reply[4], 0).result == kResultTimeout);
    CHECK(f.apt->pending_parameter().has_value()); // Acquiring the event is not ReceiveParameter.
}

void UnknownAndMalformedRequestsRemainStops() {
    Fixture f;
    for (const auto request : {
             IpcCommandBuffer{0x000e0080, 0x300, 0x1000},
             IpcCommandBuffer{0x00010000},
             IpcCommandBuffer{0x00010040, 1},
             IpcCommandBuffer{0x00020080, 0x301, 0},
             IpcCommandBuffer{0x00020080, 0x300, 1}}) {
        f.Put(request);
        const auto before = f.cpu;
        const auto count = f.kernel.handles().OpenHandleCount();
        CHECK(f.Call().kind == a32::ExitKind::Svc);
        CHECK(f.cpu.r == before.r && f.cpu.cpsr == before.cpsr && f.cpu.fpscr == before.fpscr);
        CHECK(f.Read() == request);
        CHECK(f.router.last_request() == request && f.router.last_session_name() == "APT:U");
        CHECK(f.router.unsupported_request());
        CHECK(f.kernel.handles().OpenHandleCount() == count);
    }
    f.Put({0x00010040, 0});
    f.Call();
    CHECK(!f.router.unsupported_request());
    f.Put({0x00020080, 0x300, 0});
    f.Call();
    f.Put({0x00020080, 0x300, 0}); // Duplicate initialization not part of this slice.
    const auto count = f.kernel.handles().OpenHandleCount();
    CHECK(f.Call().kind == a32::ExitKind::Svc);
    CHECK(f.kernel.handles().OpenHandleCount() == count);
}

void HandleExhaustionRollsBackInitialization() {
    Fixture f;
    auto dummy = std::make_shared<GenericObject>(KernelObject::Type::Other);
    std::vector<Handle> handles;
    while (f.kernel.handles().OpenHandleCount() < HandleTable::kMaxCount - 1) {
        Handle handle = 0;
        CHECK(f.kernel.handles().Create(&handle, dummy) == 0);
        handles.push_back(handle);
    }
    const IpcCommandBuffer request{0x00020080, 0x300, 0};
    f.Put(request);
    CHECK(f.Call().kind == a32::ExitKind::Fallthrough);
    CHECK(f.cpu.r[0] == kResultOutOfHandles);
    CHECK(f.Read() == request);
    CHECK(f.kernel.handles().OpenHandleCount() == HandleTable::kMaxCount - 1);
    CHECK(!f.apt->initialized() && !f.apt->pending_parameter());
    CHECK(f.kernel.CloseHandle(handles.back()) == 0);
    f.Put(request);
    f.Call();
    CHECK(f.cpu.r[0] == 0 && f.apt->initialized());
}

void ReadOnlyCommandBufferHasNoSideEffects() {
    Fixture f;
    GuestMemory protected_memory;
    CHECK(protected_memory.Map(kTlsAreaBase, kPageSize, MemoryPermission::Read));
    const IpcCommandBuffer request{0x00020080, 0x300, 0};
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(request.data());
    CHECK(protected_memory.LoadBytes(f.Address(), {bytes, sizeof(request)}));
    const auto count = f.kernel.handles().OpenHandleCount();
    CHECK(f.router.SendSyncRequest(f.kernel, protected_memory, f.session) == kResultInvalidPointer);
    CHECK(f.kernel.handles().OpenHandleCount() == count);
    CHECK(!f.apt->initialized());
}

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

void PeekThenConsumeLaunchMessage() {
    Fixture f;
    const auto event = InitializeFixture(f);
    constexpr std::uint32_t base = 0x08000000, size = 0x1000;
    CHECK(f.memory.Map(base, size + 8, MemoryPermission::Read | MemoryPermission::Write));
    const std::vector<std::uint8_t> marker(size + 8, 0xA5);
    CHECK(f.memory.LoadBytes(base, marker));
    SetReceiveBuffer(f, 0x04000002, base + 4);
    const auto handles = f.kernel.handles().OpenHandleCount();
    std::uint64_t loaded = 0, token = 0;
    std::uint32_t fault = 0;
    CHECK(f.memory.LoadExclusive(base + 8, 4, &loaded, &token, &fault));
    for (const auto id : {0xEU, 0xEU, 0xDU}) {
        f.Put({IpcMakeHeader(id, 2, 0), 0x300, size});
        CHECK(f.Call().kind == a32::ExitKind::Fallthrough && f.cpu.r[0] == 0);
        const auto response = f.Read();
        CHECK(response[0] == IpcMakeHeader(id, 4, 4));
        CHECK(response[1] == 0 && response[2] == 0 && response[3] == 1 && response[4] == 0);
        CHECK(response[5] == (id == 0xE ? 0U : 0x10U) && response[6] == 0);
        CHECK(response[7] == 0x04000002 && response[8] == base + 4);
        CHECK(f.apt->pending_parameter().has_value() == (id != 0xD));
        CHECK(f.kernel.handles().OpenHandleCount() == handles);
        CHECK(f.kernel.WaitSynchronization1(event, 0).result == kResultTimeout);
        for (std::uint32_t i = 0; i < size + 8; ++i) {
            std::uint8_t byte = 0;
            CHECK(f.memory.Read8(base + i, &byte));
            CHECK(byte == ((i < 4 || i >= size + 4) ? 0xA5 : 0));
        }
    }
    CHECK(f.memory.StoreExclusive(base + 8, 4, 0x99999999, token, &fault) ==
          a32::ExclusiveStoreResult::ReservationLost);
    // NoData is a service error response, not a transport failure or a new
    // launch event. It must not read/write an unrelated or missing output area.
    SetReceiveBuffer(f, 0, 0xFFFFFFFF);
    for (const auto id : {0xDU, 0xEU}) {
        f.Put({IpcMakeHeader(id, 2, 0), 0x300, size});
        CHECK(f.Call().kind == a32::ExitKind::Fallthrough && f.cpu.r[0] == 0);
        const auto response = f.Read();
        CHECK(response[0] == IpcMakeHeader(id, 1, 0));
        CHECK(response[1] == 0xC8A0CFEF);
        CHECK(!f.apt->pending_parameter());
    }
}

void FailedOutputDoesNotConsumeOrPartiallyWrite() {
    Fixture f;
    InitializeFixture(f);
    constexpr std::uint32_t base = 0x09000000;
    CHECK(f.memory.Map(base, 0x1000, MemoryPermission::Read | MemoryPermission::Write));
    CHECK(f.memory.Map(base + 0x2000, 0x1000, MemoryPermission::Read));
    CHECK(f.memory.Map(0xFFFFFFF0, 16, MemoryPermission::Read | MemoryPermission::Write));
    const std::vector<std::uint8_t> marker(0x1000, 0xAB);
    CHECK(f.memory.LoadBytes(base, marker));
    CHECK(f.memory.LoadBytes(base + 0x2000, marker));
    const auto count = f.kernel.handles().OpenHandleCount();
    struct Case { std::uint32_t descriptor, destination; };
    for (const auto c : {
             Case{0x04000000, base}, // wrong descriptor type
             Case{0x04000402, base}, // wrong buffer id
             Case{0x03FFC002, base}, // capacity 4095, not 4096
             Case{0x04000002, base + 4}, // mapped prefix, unmapped final bytes
             Case{0x04000002, base + 0x2000}, // read-only
             Case{0x04000002, 0xFFFFFFF0}, // arithmetic wrap
             Case{0x04000002, 0xDEAD0000}}) {
        SetReceiveBuffer(f, c.descriptor, c.destination);
        const IpcCommandBuffer request{0x000D0080, 0x300, 0x1000};
        f.Put(request);
        CHECK(f.Call().kind == a32::ExitKind::Fallthrough);
        CHECK(f.cpu.r[0] == kResultInvalidPointer && f.Read() == request);
        CHECK(f.apt->pending_parameter().has_value());
        CHECK(f.kernel.handles().OpenHandleCount() == count);
        for (std::uint32_t i = 0; i < 0x1000; ++i) {
            std::uint8_t a = 0, b = 0;
            CHECK(f.memory.Read8(base + i, &a) && a == 0xAB);
            CHECK(f.memory.Read8(base + 0x2000 + i, &b) && b == 0xAB);
        }
    }
    // Rejected requests cannot mutate the queue. Correcting the destination
    // allows the original message to be delivered once.
    SetReceiveBuffer(f, 0x04000002, base);
    f.Put({0x000D0080, 0x300, 0x1000});
    f.Call();
    CHECK(f.cpu.r[0] == 0 && !f.apt->pending_parameter());
}

void EmptyAndOversizedParameterBuffers() {
    Fixture f;
    InitializeFixture(f);
    const auto count = f.kernel.handles().OpenHandleCount();
    for (const auto request : {
             IpcCommandBuffer{0x000E0080, 0x300, 0x1001},
             IpcCommandBuffer{0x000D0080, 0x300, 0xFFFFFFFF},
             IpcCommandBuffer{0x000E0080, 0x301, 0x1000},
             IpcCommandBuffer{0x000E0082, 0x300, 0x1000}}) {
        f.Put(request);
        const auto before = f.cpu;
        CHECK(f.Call().kind == a32::ExitKind::Svc);
        CHECK(f.cpu.r == before.r && f.Read() == request);
        CHECK(f.apt->pending_parameter().has_value());
    }
    // A zero-sized transfer validates the descriptor but dereferences no
    // destination bytes. No host vector is sized from the request.
    SetReceiveBuffer(f, 2, 0xFFFFFFFF);
    f.Put({0x000E0080, 0x300, 0});
    CHECK(f.Call().kind == a32::ExitKind::Fallthrough && f.cpu.r[0] == 0);
    CHECK(f.Read()[7] == 2 && f.Read()[8] == 0xFFFFFFFF);
    CHECK(f.apt->pending_parameter().has_value());
    CHECK(f.kernel.handles().OpenHandleCount() == count);
}

void MissingStaticTableAndProtectedResponsePreserveQueue() {
    Fixture f;
    InitializeFixture(f);
    const IpcCommandBuffer request{0x000D0080, 0x300, 0x1000};
    GuestMemory partial;
    CHECK(partial.Map(f.Address(), 0x100, MemoryPermission::Read | MemoryPermission::Write));
    CHECK(partial.LoadBytes(f.Address(), {
        reinterpret_cast<const std::uint8_t*>(request.data()), sizeof(request)}));
    CHECK(f.router.SendSyncRequest(f.kernel, partial, f.session) == kResultInvalidPointer);
    CHECK(f.apt->pending_parameter().has_value());
    GuestMemory readonly;
    CHECK(readonly.Map(kTlsAreaBase, kPageSize, MemoryPermission::Read));
    CHECK(readonly.LoadBytes(f.Address(), {
        reinterpret_cast<const std::uint8_t*>(request.data()), sizeof(request)}));
    CHECK(f.router.SendSyncRequest(f.kernel, readonly, f.session) == kResultInvalidPointer);
    CHECK(f.apt->pending_parameter().has_value());
}

} // namespace

int main() {
    LockIdentityAndOwnership(); InitializeProducesQueuedWakeup();
    UnknownAndMalformedRequestsRemainStops(); HandleExhaustionRollsBackInitialization();
    ReadOnlyCommandBufferHasNoSideEffects();
    PeekThenConsumeLaunchMessage(); FailedOutputDoesNotConsumeOrPartiallyWrite();
    EmptyAndOversizedParameterBuffers(); MissingStaticTableAndProtectedResponsePreserveQueue();
    if (failures) return EXIT_FAILURE;
    std::cout << "PASS: APT lock, launch-parameter exchange, bounded output, and strict IPC stops\n";
    return EXIT_SUCCESS;
}
