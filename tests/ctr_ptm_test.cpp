#include "runtime/ctr_runner.h"
#include "services/ptm_service.h"
#include <cstdlib>
#include <iostream>

namespace {
using namespace lego::ctr;
int failures = 0;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL " << __LINE__ << ": " #x "\n"; ++failures; } } while (0)
void DiscoveryAndUntouchedCommands() {
    Kernel kernel;
    GuestMemory memory;
    const a32::Registry registry{};
    NativeRunner runner(registry, memory, kernel);
    auto& router = runner.ipc();
    SvcBridge bridge(kernel, &router);
    CHECK(memory.EnsureTlsMappings(kernel));
    const auto cb = kernel.current_thread()->tls_address + kIpcCommandBufferOffset;
    auto put = [&](const IpcCommandBuffer& q) {
        for (unsigned n=0; n<q.size(); ++n) CHECK(memory.Write32(cb+4*n,q[n]));
    };
    auto read = [&]() {
        IpcCommandBuffer q{};
        for (unsigned n=0; n<q.size(); ++n) CHECK(memory.Read32(cb+4*n,&q[n]));
        return q;
    };
    CHECK(router.HasService("ptm:u"));
    Handle srv = 0;
    constexpr std::uint32_t name_address = 0x08000000;
    CHECK(memory.Map(name_address, 5, MemoryPermission::Read));
    const std::array<std::uint8_t,5> name{'s','r','v',':',0};
    CHECK(memory.LoadBytes(name_address,name));
    CHECK(router.ConnectToPort(kernel,memory,name_address,&srv) == 0);
    // Actual game service-name words (bytes beyond the declared 5 are ignored).
    put({0x00050100,0x3a6d7470,0x74700075,5,0});
    const auto before_lookup = kernel.handles().OpenHandleCount();
    const auto lookup = router.SendSyncRequest(kernel,memory,srv);
    CHECK(lookup && *lookup == 0);
    const auto reply = read();
    CHECK(reply[0] == 0x00050042 && reply[1] == 0 && reply[2] == IpcMoveHandleDesc());
    Handle ptm = reply[3];
    CHECK(ptm != 0 && kernel.handles().OpenHandleCount() == before_lookup+1);
    const auto object = std::dynamic_pointer_cast<ClientSessionObject>(kernel.handles().Get(ptm));
    CHECK(object && object->name == "ptm:u" && std::dynamic_pointer_cast<PtmService>(object->service));
    const auto handles = kernel.handles().OpenHandleCount();
    const auto time = kernel.now_ns();
    const auto threads = kernel.threads().size();
    for (const IpcCommandBuffer q : {
             IpcCommandBuffer{0x000C0000},
             IpcCommandBuffer{0x000B00C2,24,0,0,0x30C,0xDEAD0000},
             IpcCommandBuffer{0x00070000},
             IpcCommandBuffer{0xFFFF0FFF}}) {
        put(q);
        a32::GuestState cpu{};
        for (unsigned n=0; n<16; ++n) cpu.r[n] = 0xAA110000+n;
        cpu.r[0] = ptm;
        cpu.r[15] = 0x0025947C;
        cpu.cpsr = 0xA0000010;
        const auto before = cpu;
        const auto stop = bridge.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcSendSyncRequest},cpu,&memory);
        CHECK(stop.kind == a32::ExitKind::Svc && stop.pc == before.r[15]);
        CHECK(cpu.r == before.r && cpu.cpsr == before.cpsr);
        CHECK(router.unsupported_request() && router.last_session_name() == "ptm:u");
        CHECK(router.last_request() == q && read() == q && router.last_host_error().empty());
        CHECK(kernel.handles().OpenHandleCount() == handles && kernel.now_ns() == time);
        CHECK(kernel.threads().size() == threads && kernel.current_thread()->status == ThreadStatus::Running);
    }
    CHECK(kernel.CloseHandle(ptm) == 0);
    CHECK(router.ConnectToService(kernel,"ptm:u",&ptm) == 0);
    put({0x000C0000});
    CHECK(!router.SendSyncRequest(kernel,memory,ptm));
    CHECK(read()[0] == 0x000C0000 && kernel.now_ns() == time);
}
} // namespace
int main() {
    DiscoveryAndUntouchedCommands();
    if (failures) return EXIT_FAILURE;
    std::cout << "PASS: PTM discovery with untouched unsupported requests and no fabricated state\n";
    return EXIT_SUCCESS;
}
