#include "services/y2r_user_service.h"
#include "runtime/ctr_runner.h"
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
    std::shared_ptr<Y2rUserService> endpoint = std::make_shared<Y2rUserService>();
    Handle session{};
    a32::GuestState cpu{};
    explicit Fixture(bool connect = true) {
        CHECK(memory.EnsureTlsMappings(kernel));
        CHECK(router.RegisterService("y2r:u", endpoint) == 0);
        if (connect) CHECK(router.ConnectToService(kernel, "y2r:u", &session) == 0);
    }
    std::uint32_t cb() const { return kernel.current_thread()->tls_address + kIpcCommandBufferOffset; }
    void Put(const IpcCommandBuffer& q) {
        for (unsigned i = 0; i < q.size(); ++i) CHECK(memory.Write32(cb() + 4 * i, q[i]));
        for (unsigned i = 0; i < 16; ++i) cpu.r[i] = 0xABCD0000U + i;
        cpu.r[0] = session; cpu.r[15] = 0x0025947C;
        cpu.cpsr = 0x60000010; cpu.fpscr = 0x83000018;
    }
    IpcCommandBuffer Read() {
        IpcCommandBuffer q{};
        for (unsigned i = 0; i < q.size(); ++i) CHECK(memory.Read32(cb() + 4 * i, &q[i]));
        return q;
    }
    a32::ExecutionResult Call(GuestMemory* alternate = nullptr) {
        return bridge.Handle({a32::ExitKind::Svc, cpu.r[15], a32::FallbackReason::None,
                              kSvcSendSyncRequest}, cpu, alternate ? alternate : &memory);
    }
    void Initialize() {
        IpcCommandBuffer q{};
        q.fill(0xCAFEABCD);
        q[0] = 0x002B0000;
        Put(q);
        const auto before = cpu;
        const auto count = kernel.handles().OpenHandleCount();
        const auto time = kernel.now_ns();
        const auto threads = kernel.threads().size();
        const auto event = endpoint->completion_event();
        CHECK(Call().kind == a32::ExitKind::Fallthrough);
        CHECK(cpu.r[0] == 0 && cpu.r[15] == before.r[15] + 4);
        for (unsigned i = 1; i < 15; ++i) CHECK(cpu.r[i] == before.r[i]);
        CHECK(cpu.cpsr == before.cpsr && cpu.fpscr == before.fpscr && cpu.vfp == before.vfp);
        CHECK(Read() == (IpcCommandBuffer{0x002B0040, 0}));
        CHECK(router.last_request() == q && router.last_session_name() == "y2r:u");
        CHECK(!router.unsupported_request() && router.last_host_error().empty());
        Y2rConfiguration expected{}; expected.input_line_width = 1024;
        CHECK(endpoint->configuration() == expected && endpoint->initialized());
        CHECK(endpoint->completion_event() == event && !event->signaled());
        CHECK(event->reset_type() == ResetType::OneShot);
        CHECK(kernel.handles().OpenHandleCount() == count && kernel.now_ns() == time);
        CHECK(kernel.threads().size() == threads && kernel.current_thread()->status == ThreadStatus::Running);
        CHECK(!kernel.current_thread()->pending_wake);
    }
};

void ResetRetainsReferenceFields() {
    Y2rConfiguration c;
    c.input_format = 4; c.output_format = 3; c.rotation = 2; c.block_alignment = 1;
    c.input_line_width = 88; c.input_lines = 73; c.alpha = 0x1234; c.padding = 0x56;
    c.coefficients.fill(-314);
    c.src_y = c.src_u = c.src_v = c.src_yuyv = c.dst = {0x12345678, 456, 32, 16};
    auto expected = c;
    expected.input_format = expected.output_format = expected.rotation = expected.block_alignment = 0;
    expected.input_line_width = 1024; expected.coefficients.fill(0); expected.alpha = 0;
    expected.src_y = expected.src_u = expected.src_v = expected.dst = {};
    c.DriverInitialize();
    CHECK(c == expected);
    c.DriverInitialize(); CHECK(c == expected);
    // In particular, SetInputLines(1024) is a no-assignment call in the pin.
    CHECK(c.input_lines == 73 && c.src_yuyv.address == 0x12345678 && c.padding == 0x56);
}

void InitializationAndDiscovery() {
    Fixture f;
    CHECK(!f.endpoint->initialized() && f.endpoint->configuration() == Y2rConfiguration{});
    CHECK(!f.endpoint->CanHandle({0x002B0000})); // endpoint itself is not a client
    CHECK(f.endpoint->CreateSessionHandler(nullptr) == kResultInvalidPointer);
    const auto completion = f.endpoint->completion_event();
    CHECK(completion && !completion->signaled());
    f.Initialize();
    Handle event_handle{};
    CHECK(f.kernel.handles().Create(&event_handle, completion) == 0);
    CHECK(f.kernel.SignalEvent(event_handle) == 0 && completion->signaled());
    f.Initialize(); // clears the SAME real event, does not create a completion
    CHECK(f.kernel.handles().Get(event_handle) == completion);
    CHECK(f.kernel.CloseHandle(event_handle) == 0);

    Kernel kernel; GuestMemory memory; const a32::Registry registry{};
    NativeRunner runner(registry, memory, kernel);
    CHECK(runner.ipc().HasService("y2r:u"));
    CHECK(memory.EnsureTlsMappings(kernel));
    constexpr std::uint32_t name_va = 0x08000000;
    CHECK(memory.Map(name_va, 5, MemoryPermission::Read));
    const std::array<std::uint8_t, 5> name{'s','r','v',':',0};
    CHECK(memory.LoadBytes(name_va, name));
    Handle srv{}; CHECK(runner.ipc().ConnectToPort(kernel, memory, name_va, &srv) == 0);
    const auto cb = kernel.current_thread()->tls_address + kIpcCommandBufferOffset;
    const IpcCommandBuffer query{0x00050100, 0x3A723279, 0x75, 5, 0};
    for (unsigned i = 0; i < query.size(); ++i) CHECK(memory.Write32(cb + i * 4, query[i]));
    CHECK(runner.ipc().SendSyncRequest(kernel, memory, srv) == 0);
    std::uint32_t reply{}, result{}, descriptor{}, handle{};
    CHECK(memory.Read32(cb, &reply) && reply == 0x00050042);
    CHECK(memory.Read32(cb + 4, &result) && result == 0);
    CHECK(memory.Read32(cb + 8, &descriptor) && descriptor == IpcMoveHandleDesc());
    CHECK(memory.Read32(cb + 12, &handle) && handle != 0);
    const auto object = std::dynamic_pointer_cast<ClientSessionObject>(kernel.handles().Get(handle));
    CHECK(object && object->name == "y2r:u" && std::dynamic_pointer_cast<Y2rUserService>(object->service));
}

void LifetimeAndConnectionLimit() {
    Fixture f;
    f.Initialize();
    const auto event = f.endpoint->completion_event();
    Handle other = 0xF00D;
    CHECK(f.router.ConnectToService(f.kernel, "y2r:u", &other) == kResultMaxConnectionsReached);
    CHECK(other == 0xF00D);
    Handle duplicate{}; CHECK(f.kernel.DuplicateHandle(&duplicate, f.session) == 0);
    CHECK(f.kernel.CloseHandle(f.session) == 0);
    CHECK(f.router.ConnectToService(f.kernel, "y2r:u", &other) == kResultMaxConnectionsReached);
    f.session = duplicate; f.Initialize();
    CHECK(f.kernel.CloseHandle(duplicate) == 0);
    CHECK(f.router.ConnectToService(f.kernel, "y2r:u", &f.session) == 0);
    CHECK(f.endpoint->completion_event() == event && f.endpoint->initialized());
    Fixture independent;
    CHECK(independent.endpoint->completion_event() != event && !independent.endpoint->initialized());
    independent.Initialize(); f.Initialize();
}

void RejectionAndResponsePreflight() {
    Fixture f;
    const auto event = f.endpoint->completion_event();
    event->Signal(); // deliberate test preparation; no original-game state
    const auto config = f.endpoint->configuration();
    for (const auto header : {0x002B0040U, 0x002B0001U, 0x002B0002U, 0x002B1000U,
                              0x00260000U, 0x00280000U, 0x000F0000U, 0x002C0000U, 0xFFFF0000U}) {
        const IpcCommandBuffer q{header, 0xABCD};
        f.Put(q); const auto cpu = f.cpu;
        CHECK(f.Call().kind == a32::ExitKind::Svc);
        CHECK(f.router.unsupported_request() && f.Read() == q && f.cpu.r == cpu.r);
        CHECK(f.endpoint->configuration() == config && !f.endpoint->initialized() && event->signaled());
    }
    for (unsigned mode = 0; mode < 3; ++mode) {
        GuestMemory blocked;
        const IpcCommandBuffer q{0x002B0000};
        const unsigned bytes = mode == 1 ? 4 : sizeof(q);
        const auto permission = mode == 0 ? MemoryPermission::Read : mode == 1 ?
            MemoryPermission::Read | MemoryPermission::Write : MemoryPermission::Write;
        CHECK(blocked.Map(f.cb(), bytes, permission));
        CHECK(blocked.LoadBytes(f.cb(), {reinterpret_cast<const std::uint8_t*>(q.data()), bytes}));
        f.Put(q);
        CHECK(f.Call(&blocked).kind == a32::ExitKind::Fallthrough && f.cpu.r[0] == kResultInvalidPointer);
        CHECK(f.endpoint->configuration() == config && !f.endpoint->initialized() && event->signaled());
    }
    f.Put({0x002B0000}); f.cpu.r[0] = 0;
    CHECK(f.Call().kind == a32::ExitKind::Fallthrough && f.cpu.r[0] == kResultInvalidHandle);
    CHECK(!f.endpoint->initialized() && event->signaled());
    f.Initialize();
}

void FailedHandleAllocationReleasesSession() {
    Fixture f(false);
    Handle event{}; CHECK(f.kernel.CreateEvent(&event, static_cast<unsigned>(ResetType::OneShot)) == 0);
    Handle last{}; bool exhausted = false;
    for (unsigned i = 0; i < 65536; ++i) {
        Handle copy{};
        const auto result = f.kernel.DuplicateHandle(&copy, event);
        if (result != 0) { CHECK(result == kResultOutOfHandles); exhausted = true; break; }
        last = copy;
    }
    CHECK(exhausted && last != 0);
    const auto count = f.kernel.handles().OpenHandleCount();
    CHECK(f.router.ConnectToService(f.kernel, "y2r:u", &f.session) == kResultOutOfHandles);
    CHECK(f.session == 0 && f.kernel.handles().OpenHandleCount() == count);
    CHECK(f.kernel.CloseHandle(last) == 0);
    // Failed connection must not leave the one-session lease occupied.
    CHECK(f.router.ConnectToService(f.kernel, "y2r:u", &f.session) == 0);
    CHECK(f.kernel.handles().OpenHandleCount() == count);
    f.Initialize(); // no handle allocation during reset even with a full table
}
} // namespace
int main() {
    ResetRetainsReferenceFields(); InitializationAndDiscovery(); LifetimeAndConnectionLimit();
    RejectionAndResponsePreflight(); FailedHandleAllocationReleasesSession();
    if (failures) return EXIT_FAILURE;
    std::cout << "PASS: Y2R driver state/event reset, single-session ownership and untouched conversion requests\n";
    return EXIT_SUCCESS;
}
