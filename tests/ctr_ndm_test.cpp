#include "services/ndm_service.h"
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
    std::shared_ptr<NdmService> ndm{std::make_shared<NdmService>()};
    Handle session{};
    a32::GuestState cpu{};

    Fixture() {
        if (!memory.EnsureTlsMappings(kernel) ||
            router.RegisterService("ndm:u", ndm) != kResultSuccess ||
            router.ConnectToService(kernel, "ndm:u", &session) != kResultSuccess)
            throw std::runtime_error("NDM fixture setup failed");
    }
    std::uint32_t Address() const {
        return kernel.current_thread()->tls_address + kIpcCommandBufferOffset;
    }
    void Put(const IpcCommandBuffer& request) {
        for (std::size_t i = 0; i < request.size(); ++i)
            CHECK(memory.Write32(Address() + static_cast<std::uint32_t>(i * 4), request[i]));
        cpu.r[0] = session;
        cpu.r[15] = 0x0025947C;
        cpu.cpsr = 0x60000010;
        cpu.fpscr = 0x23000010;
    }
    IpcCommandBuffer Read() {
        IpcCommandBuffer result{};
        for (std::size_t i = 0; i < result.size(); ++i)
            CHECK(memory.Read32(Address() + static_cast<std::uint32_t>(i * 4), &result[i]));
        return result;
    }
    a32::ExecutionResult Call() {
        return bridge.Handle({a32::ExitKind::Svc, cpu.r[15], a32::FallbackReason::None,
                              kSvcSendSyncRequest}, cpu, &memory);
    }
    void Success(const IpcCommandBuffer& request) {
        Put(request);
        CHECK(Call().kind == a32::ExitKind::Fallthrough);
        CHECK(cpu.r[0] == kResultSuccess && cpu.r[15] == 0x00259480);
        const auto response = Read();
        CHECK(response[0] == IpcMakeHeader(IpcCommandId(request[0]), 1, 0));
        CHECK(response[1] == kResultSuccess);
        for (std::size_t i = 2; i < response.size(); ++i) CHECK(response[i] == 0);
        CHECK(router.last_session_name() == "ndm:u" && router.last_request() == request);
        CHECK(!router.unsupported_request());
    }
};

void ObservedStartupSequence() {
    Fixture f;
    CHECK(f.ndm->default_mask() == 0x9 && f.ndm->current_mask() == 0x9);
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(f.ndm->statuses()[i] == NdmService::DaemonStatus::Idle);
        CHECK(f.ndm->suspend_counts()[i] == 0);
    }
    const auto handles = f.kernel.handles().OpenHandleCount();
    const auto time = f.kernel.now_ns();
    Handle unrelated_event = 0;
    CHECK(f.kernel.CreateEvent(&unrelated_event, 0) == kResultSuccess);

    // Exact headers/masks observed in the unchanged game, not scripted guest edits.
    f.Success({0x00140040, 0xF});
    CHECK(f.ndm->default_mask() == 0xF && f.ndm->current_mask() == 0xF);
    f.Success({0x00060040, 0x6});
    CHECK(f.ndm->default_mask() == 0xF && f.ndm->current_mask() == 0x9);
    CHECK((f.ndm->suspend_counts() == std::array<std::uint32_t, 4>{0, 1, 1, 0}));
    CHECK(f.ndm->statuses()[0] == NdmService::DaemonStatus::Idle);
    CHECK(f.ndm->statuses()[1] == NdmService::DaemonStatus::Suspended);
    CHECK(f.ndm->statuses()[2] == NdmService::DaemonStatus::Suspended);
    CHECK(f.ndm->statuses()[3] == NdmService::DaemonStatus::Idle);
    CHECK(f.kernel.now_ns() == time);
    CHECK(f.kernel.current_thread()->status == ThreadStatus::Running);
    CHECK(!f.kernel.current_thread()->pending_wake);
    CHECK(f.kernel.handles().OpenHandleCount() == handles + 1);
    CHECK(f.kernel.WaitSynchronization1(unrelated_event, 0).result == kResultTimeout);
}

void MasksNestingAndSessionLifetime() {
    Fixture f;
    f.Success({0x00140040, 0xFFFFFFFF}); // Pinned HLE masks to the low four bits.
    CHECK(f.ndm->default_mask() == 0xF);
    f.Success({0x00060040, 0xFFFFFFF6});
    f.Success({0x00060040, 0x6});
    CHECK((f.ndm->suspend_counts() == std::array<std::uint32_t, 4>{0, 2, 2, 0}));
    CHECK(f.ndm->current_mask() == 0x9);
    // Closing a client session does not discard this shared service's state.
    CHECK(f.kernel.CloseHandle(f.session) == 0);
    CHECK(f.router.ConnectToService(f.kernel, "ndm:u", &f.session) == 0);
    f.Success({0x00060040, 0x8});
    CHECK(f.ndm->current_mask() == 0x7); // Pinned rule derives from default mask.
    CHECK((f.ndm->suspend_counts() == std::array<std::uint32_t, 4>{0, 2, 2, 1}));
    CHECK(f.ndm->statuses()[1] == NdmService::DaemonStatus::Suspended);
    CHECK(f.ndm->statuses()[3] == NdmService::DaemonStatus::Suspended);
    f.Success({0x00140040, 0x1});
    CHECK(f.ndm->default_mask() == 1 && f.ndm->current_mask() == 1);
    CHECK(f.ndm->statuses()[1] == NdmService::DaemonStatus::Suspended);
    // Match upstream's limited state model without secretly clearing counters.
    f.Success({0x00140040, 0xF});
    CHECK(f.ndm->statuses()[1] == NdmService::DaemonStatus::Idle);
    CHECK(f.ndm->suspend_counts()[1] == 2);
    f.Success({0x00060040, 0});
    CHECK(f.ndm->current_mask() == 0xF && f.ndm->suspend_counts()[1] == 2);
}

void UnknownShapesLeaveGuestAndServiceIntact() {
    Fixture f;
    f.Success({0x00140040, 0xF});
    f.Success({0x00060040, 6});
    for (const auto request : {
            IpcCommandBuffer{0x00070040, 6}, // Resume not implemented in this slice.
            IpcCommandBuffer{0x00160000},    // Nor a guessed query response.
            IpcCommandBuffer{0x00140000},
            IpcCommandBuffer{0x00140080, 1, 2},
            IpcCommandBuffer{0x00140042, 1, 0x20, 0},
            IpcCommandBuffer{0x00141040, 1}, // Reserved header bits are not ignored.
            IpcCommandBuffer{0x00060000},
            IpcCommandBuffer{0x00060041, 6, 0},
            IpcCommandBuffer{0x7FFF0000}}) {
        const auto counts = f.ndm->suspend_counts();
        const auto statuses = f.ndm->statuses();
        const auto handles = f.kernel.handles().OpenHandleCount();
        f.Put(request);
        const auto before = f.cpu;
        CHECK(f.Call().kind == a32::ExitKind::Svc);
        CHECK(f.cpu.r == before.r && f.cpu.cpsr == before.cpsr && f.cpu.fpscr == before.fpscr);
        CHECK(f.Read() == request && f.router.unsupported_request());
        CHECK(f.ndm->default_mask() == 0xF && f.ndm->current_mask() == 0x9);
        CHECK(f.ndm->suspend_counts() == counts && f.ndm->statuses() == statuses);
        CHECK(f.kernel.handles().OpenHandleCount() == handles);
    }
}

void ResponsePreflightPreventsStateChanges() {
    for (const auto request : {IpcCommandBuffer{0x00140040, 0},
                               IpcCommandBuffer{0x00060040, 6}}) {
        Fixture f;
        for (const bool partial : {false, true}) {
            GuestMemory protected_memory;
            // Full readable/readonly page, or just the two command words mapped.
            CHECK(protected_memory.Map(f.Address(), partial ? 8 : sizeof(IpcCommandBuffer),
                partial ? MemoryPermission::Read | MemoryPermission::Write : MemoryPermission::Read));
            CHECK(protected_memory.LoadBytes(f.Address(), {
                reinterpret_cast<const std::uint8_t*>(request.data()), partial ? 8 : sizeof(request)}));
            const auto counts = f.ndm->suspend_counts();
            CHECK(f.router.SendSyncRequest(f.kernel, protected_memory, f.session) == kResultInvalidPointer);
            CHECK(f.ndm->default_mask() == 0x9 && f.ndm->current_mask() == 0x9);
            CHECK(f.ndm->suspend_counts() == counts);
            std::uint32_t value = 0;
            CHECK(protected_memory.Read32(f.Address(), &value) && value == request[0]);
        }
    }
}
} // namespace

int main() {
    ObservedStartupSequence();
    MasksNestingAndSessionLifetime();
    UnknownShapesLeaveGuestAndServiceIntact();
    ResponsePreflightPreventsStateChanges();
    if (failures) return EXIT_FAILURE;
    std::cout << "PASS: observed NDM mask/suspend IPC and strict boundaries\n";
    return EXIT_SUCCESS;
}
