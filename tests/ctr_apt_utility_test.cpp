#include "services/apt_service.h"
#include "runtime/ctr_svc_bridge.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
using namespace lego::ctr;
int failures = 0;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL " << __LINE__ << ": " #x "\n"; ++failures; } } while (0)
constexpr auto RW = MemoryPermission::Read | MemoryPermission::Write;
constexpr std::uint32_t kSource = 0x08000040, kDestination = 0x08000081;

struct Fixture {
    Kernel kernel;
    GuestMemory memory;
    IpcRouter router;
    SvcBridge bridge{kernel, &router};
    std::shared_ptr<AptService> apt{std::make_shared<AptService>()};
    Handle session{}, notification_handle{}, parameter_handle{};
    std::shared_ptr<EventObject> notification, parameter;
    a32::GuestState cpu{};
    Fixture() {
        if (!memory.EnsureTlsMappings(kernel) || !memory.Map(0x08000000, 0x1000, RW) ||
            router.RegisterService("APT:U", apt) != kResultSuccess ||
            router.ConnectToService(kernel, "APT:U", &session) != kResultSuccess)
            throw std::runtime_error("fixture setup failed");
        cpu = kernel.CurrentGuestState();
        CHECK(memory.Write32(kSource, 0x10));
        CHECK(memory.Write32(kDestination - 1, 0xA5A5A5A5));
        Descriptor(0x4002, kDestination);
    }
    std::uint32_t Address() const { return kernel.current_thread()->tls_address + 0x80; }
    std::uint32_t Table() const { return kernel.current_thread()->tls_address + 0x180; }
    void Descriptor(std::uint32_t descriptor, std::uint32_t address) {
        CHECK(memory.Write32(Table(), descriptor));
        CHECK(memory.Write32(Table() + 4, address));
    }
    static IpcCommandBuffer Request() { return {0x004B00C2, 7, 4, 1, 0x10402, kSource}; }
    IpcCommandBuffer Read(GuestMemory& m) const {
        IpcCommandBuffer command{};
        for (std::size_t i = 0; i < command.size(); ++i)
            CHECK(m.Read32(Address() + i * 4, &command[i]));
        return command;
    }
    void Put(const IpcCommandBuffer& command) {
        for (std::size_t i = 0; i < command.size(); ++i)
            CHECK(memory.Write32(Address() + i * 4, command[i]));
        cpu.r[0] = session; cpu.r[15] = 0x25947c;
    }
    a32::ExecutionResult Call(GuestMemory* m = nullptr) {
        return bridge.Handle({a32::ExitKind::Svc, cpu.r[15], a32::FallbackReason::None,
                              kSvcSendSyncRequest}, cpu, m ? m : &memory);
    }
    void Initialize() {
        Put({0x20080, 0x300, 0});
        CHECK(Call().kind == a32::ExitKind::Fallthrough);
        const auto response = Read(memory);
        notification_handle = response[3]; parameter_handle = response[4];
        notification = std::dynamic_pointer_cast<EventObject>(kernel.handles().Get(notification_handle));
        parameter = std::dynamic_pointer_cast<EventObject>(kernel.handles().Get(parameter_handle));
        if (!notification || !parameter) throw std::runtime_error("missing APT events");
    }
    std::uint8_t Byte(std::uint32_t address) {
        std::uint8_t value = 0; CHECK(memory.Read8(address, &value)); return value;
    }
    struct State {
        bool queued, notification, parameter, registered, pending_wake;
        std::size_t handles, threads;
        std::uint64_t time;
        std::shared_ptr<ThreadObject> thread;
        ThreadStatus status;
    };
    State Save() const {
        return {apt->pending_parameter().has_value(), notification && notification->signaled(),
                parameter && parameter->signaled(), apt->registered(), kernel.current_thread()->pending_wake,
                kernel.handles().OpenHandleCount(), kernel.threads().size(), kernel.now_ns(),
                kernel.current_thread(), kernel.current_thread()->status};
    }
    void Same(const State& before) const {
        const auto after = Save();
        CHECK(after.queued == before.queued && after.notification == before.notification &&
              after.parameter == before.parameter && after.registered == before.registered &&
              after.pending_wake == before.pending_wake && after.handles == before.handles &&
              after.threads == before.threads && after.time == before.time &&
              after.thread == before.thread && after.status == before.status);
    }
    void Success() {
        Descriptor(0x4002, kDestination);
        CHECK(memory.Write32(kDestination - 1, 0xA5A5A5A5));
        cpu.r[6] = 0xCAFE1234; cpu.vfp[7] = 0x12345678;
        cpu.cpsr = 0xA0000010; cpu.fpscr = 0x03C00010;
        cpu.exclusive_address = 0x2000; cpu.exclusive_size = 4;
        cpu.exclusive_token = 444; cpu.exclusive_valid = true;
        Put(Request());
        const auto before = cpu;
        const auto service = Save();
        const auto exit = Call();
        auto expected = before.r; expected[0] = 0; expected[15] += 4;
        CHECK(exit.kind == a32::ExitKind::Fallthrough && exit.pc == 0x259480);
        CHECK(cpu.r == expected && cpu.vfp == before.vfp);
        CHECK(cpu.cpsr == before.cpsr && cpu.fpscr == before.fpscr && cpu.thread_pointer == before.thread_pointer);
        CHECK(cpu.exclusive_address == before.exclusive_address && cpu.exclusive_token == before.exclusive_token &&
              cpu.exclusive_size == before.exclusive_size && cpu.exclusive_valid == before.exclusive_valid);
        const IpcCommandBuffer response{0x004B0082, 0, 0, 0x4002, kDestination};
        CHECK(Read(memory) == response);
        CHECK(Byte(kDestination - 1) == 0xA5 && Byte(kDestination) == 0 && Byte(kDestination + 1) == 0xA5);
        CHECK(router.last_request() == Request() && router.last_session_name() == "APT:U");
        CHECK(!router.unsupported_request());
        Same(service);
    }
    void PointerFailure(const IpcCommandBuffer& request) {
        Put(request);
        const auto service = Save();
        const auto output = Byte(kDestination);
        CHECK(Call().kind == a32::ExitKind::Fallthrough);
        CHECK(cpu.r[0] == kResultInvalidPointer && Read(memory) == request);
        CHECK(Byte(kDestination) == output);
        Same(service);
    }
};

void ExactResponseAndStatePreservation() {
    Fixture f; f.Initialize();
    f.Success(); f.Success();
    CHECK(f.kernel.WaitSynchronization1(f.parameter_handle, 0).result == kResultSuccess);
    f.Success(); // No replacement parameter signal.
    CHECK(f.kernel.WaitSynchronization1(f.parameter_handle, 0).result == kResultTimeout);
    f.Descriptor(2, 0);
    f.Put({0x000D0080, 0x300, 0});
    CHECK(f.Call().kind == a32::ExitKind::Fallthrough && !f.apt->pending_parameter());
    f.Success(); // Does not requeue a consumed launch parameter.
    CHECK(f.kernel.CloseHandle(f.session) == kResultSuccess);
    CHECK(f.router.ConnectToService(f.kernel, "APT:U", &f.session) == kResultSuccess);
    f.Success();
    CHECK(!f.apt->pending_parameter() && !f.parameter->signaled() && !f.notification->signaled());
}

void UnknownShapesRemainStops() {
    Fixture f;
    f.Put(Fixture::Request());
    const auto early = f.cpu;
    CHECK(f.Call().kind == a32::ExitKind::Svc && f.cpu.r == early.r);
    CHECK(!f.apt->initialized());
    f.Initialize();
    const auto unchanged = f.Save();
    for (const auto request : {
        IpcCommandBuffer{0x004B00C2, 6, 4, 1, 0x10402, kSource},
        IpcCommandBuffer{0x004B00C2, 4, 1, 1, 0x4402, kSource},
        IpcCommandBuffer{0x004B00C2, 0xFFFFFFFF, 4, 1, 0x10402, kSource},
        IpcCommandBuffer{0x004B00C0, 7, 4, 1, 0x10402, kSource},
        IpcCommandBuffer{0x004B0082, 7, 4, 1, 0x10402, kSource},
        IpcCommandBuffer{0x004B00C2, 7, 3, 1, 0xC402, kSource},
        IpcCommandBuffer{0x004B00C2, 7, 4, 0, 0x10402, kSource},
        IpcCommandBuffer{0x004B00C2, 7, 4, 2, 0x10402, kSource},
        IpcCommandBuffer{0x004B00C2, 7, 4, 1, 0x10002, kSource},
        IpcCommandBuffer{0x004B00C2, 7, 4, 1, 0x1040A, kSource},
        IpcCommandBuffer{0x004B00C2, 7, 4, 1, 0x14402, kSource}}) {
        f.Put(request);
        const auto cpu = f.cpu;
        CHECK(f.Call().kind == a32::ExitKind::Svc);
        CHECK(f.cpu.r == cpu.r && f.Read(f.memory) == request);
        CHECK(f.router.unsupported_request() && f.Byte(kDestination) == 0xA5);
        f.Same(unchanged);
    }
}

void BufferValidationBeforeWrites() {
    Fixture f; f.Initialize();
    for (auto descriptor : {0U, 2U, 0x4402U, 0x400AU}) {
        f.Descriptor(descriptor, kDestination);
        f.PointerFailure(Fixture::Request());
    }
    f.Descriptor(0x4002, kDestination);
    auto request = Fixture::Request(); request[5] = 0x07000000;
    f.PointerFailure(request);
    CHECK(f.memory.Map(0xFFFFFFFC, 4, RW));
    CHECK(f.memory.Write32(0xFFFFFFFC, 0x10));
    request[5] = 0xFFFFFFFD; // Would wrap if 4-byte extent were unchecked.
    f.PointerFailure(request);
    CHECK(f.memory.Map(0x09000000, 1, MemoryPermission::Read));
    for (const auto destination : {0x07000000U, 0x09000000U, f.Address(), f.Address() + 0xFF,
                                   f.Table(), f.Table() + 7}) {
        f.Descriptor(0x4002, destination);
        f.PointerFailure(Fixture::Request());
    }
    // Larger receive capacity must not cause padding beyond requested one byte.
    f.Descriptor(0x4000002, kDestination);
    f.Put(Fixture::Request());
    CHECK(f.Call().kind == a32::ExitKind::Fallthrough && f.cpu.r[0] == 0);
    CHECK(f.Byte(kDestination) == 0 && f.Byte(kDestination + 1) == 0xA5);

    // Top-of-address-space input and one-byte output are valid when mapped.
    request[5] = 0xFFFFFFFC;
    f.Descriptor(0x4002, 0xFFFFFFFF);
    f.Put(request);
    CHECK(f.Call().kind == a32::ExitKind::Fallthrough && f.cpu.r[0] == 0);
    CHECK(f.Byte(0xFFFFFFFF) == 0);
}

void MissingDescriptorAndProtectedResponse() {
    Fixture f; f.Initialize();
    const auto request = Fixture::Request();
    for (const int mode : {0, 1, 2}) {
        GuestMemory m;
        if (mode == 0) {
            CHECK(m.Map(f.Address(), sizeof(request), RW)); // No TLS receive descriptor mapped.
        } else if (mode == 1) {
            CHECK(m.Map(f.Address(), sizeof(request), MemoryPermission::Read));
        } else {
            CHECK(m.Map(f.Address(), 0xFC, RW));
            CHECK(m.Map(f.Address() + 0xFC, 4, MemoryPermission::Read));
        }
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(request.data());
        if (mode == 2) {
            CHECK(m.LoadBytes(f.Address(), {bytes, 0xFC}));
            CHECK(m.LoadBytes(f.Address() + 0xFC, {bytes + 0xFC, 4}));
        } else CHECK(m.LoadBytes(f.Address(), {bytes, sizeof(request)}));
        CHECK(m.Map(0x08000000, 0x1000, RW));
        CHECK(m.Write32(kSource, 0x10)); CHECK(m.Write8(kDestination, 0xA5));
        const auto before = f.Save();
        f.cpu.r[0] = f.session; f.cpu.r[15] = 0x25947c;
        CHECK(f.Call(&m).kind == a32::ExitKind::Fallthrough && f.cpu.r[0] == kResultInvalidPointer);
        CHECK(f.Read(m) == request);
        std::uint8_t value = 0; CHECK(m.Read8(kDestination, &value) && value == 0xA5);
        f.Same(before);
    }
}

void GuestWriteInvalidatesReservationAndSnapshotsInput() {
    Fixture f; f.Initialize();
    // Reference HLE ignores mask content; it never mutates the input buffer.
    for (const auto mask : {0U, 0x10U, 0xFFFFFFFFU}) {
        CHECK(f.memory.Write32(kSource, mask));
        f.Success();
        std::uint32_t value = 0; CHECK(f.memory.Read32(kSource, &value) && value == mask);
    }
    f.Descriptor(0x4002, kDestination);
    f.Put(Fixture::Request());
    std::uint64_t value = 0, token = 0; std::uint32_t fault = 0;
    CHECK(f.memory.LoadExclusive(kDestination, 1, &value, &token, &fault));
    CHECK(f.Call().kind == a32::ExitKind::Fallthrough);
    CHECK(f.memory.StoreExclusive(kDestination, 1, 0xAB, token, &fault) ==
          a32::ExclusiveStoreResult::ReservationLost);
    CHECK(f.Byte(kDestination) == 0);

    CHECK(f.memory.Write32(kSource, 0xAABBCC10));
    f.Descriptor(0x4002, kSource);
    f.Put(Fixture::Request());
    CHECK(f.Call().kind == a32::ExitKind::Fallthrough && f.cpu.r[0] == 0);
    std::uint32_t input = 0; CHECK(f.memory.Read32(kSource, &input) && input == 0xAABBCC00);
}
} // namespace
int main() {
    ExactResponseAndStatePreservation(); UnknownShapesRemainStops();
    BufferValidationBeforeWrites(); MissingDescriptorAndProtectedResponse();
    GuestWriteInvalidatesReservationAndSnapshotsInput();
    if (failures) return EXIT_FAILURE;
    std::cout << "PASS: bounded UnlockTransition utility, static buffers, unchanged APT/kernel state\n";
    return EXIT_SUCCESS;
}
