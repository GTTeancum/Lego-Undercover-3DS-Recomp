#include "runtime/ctr_runner.h"
#include "services/ptm_service.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
using namespace lego::ctr;
int failures = 0;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL " << __LINE__ << ": " #x "\n"; ++failures; } } while (0)
constexpr auto rw = MemoryPermission::Read | MemoryPermission::Write;
struct Fixture {
    Kernel kernel;
    GuestMemory memory;
    IpcRouter router;
    SvcBridge bridge{kernel, &router};
    Handle session{};
    a32::GuestState cpu{};
    static constexpr std::uint32_t output = 0x08000001;
    std::uint32_t cb() const { return kernel.current_thread()->tls_address + kIpcCommandBufferOffset; }
    explicit Fixture(PtmStepMode mode = PtmStepMode::EmptyHistory) {
        CHECK(memory.EnsureTlsMappings(kernel));
        CHECK(memory.Map(output-1, 0x1004, rw));
        CHECK(router.RegisterService("ptm:u", std::make_shared<PtmService>(mode)) == 0);
        CHECK(router.ConnectToService(kernel,"ptm:u",&session) == 0);
    }
    void FillOutput() {
        for (std::uint32_t n=0;n<0x1004;++n) CHECK(memory.Write8(output-1+n,0xA5));
    }
    IpcCommandBuffer History(std::uint32_t hours = 24, std::uint64_t start = 0,
                             std::uint32_t address = output) {
        return {0x000B00C2,hours,static_cast<std::uint32_t>(start),static_cast<std::uint32_t>(start>>32),
                ((hours*2)<<4)|0xC,address};
    }
    void Put(const IpcCommandBuffer& q) {
        for (unsigned n=0;n<q.size();++n) CHECK(memory.Write32(cb()+4*n,q[n]));
        for (unsigned n=0;n<16;++n) cpu.r[n]=0xA5000000+n;
        cpu.r[0]=session; cpu.r[15]=0x0025947C; cpu.cpsr=0x60000010;
    }
    IpcCommandBuffer Read() {
        IpcCommandBuffer q{};
        for (unsigned n=0;n<q.size();++n) CHECK(memory.Read32(cb()+4*n,&q[n]));
        return q;
    }
    std::uint8_t Byte(std::uint32_t p) {
        std::uint8_t b=0; CHECK(memory.Read8(p,&b)); return b;
    }
    a32::ExecutionResult Call(GuestMemory* alternate = nullptr) {
        return bridge.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcSendSyncRequest},
                             cpu,alternate ? alternate : &memory);
    }
    void Reply(const IpcCommandBuffer& q, const IpcCommandBuffer& expected) {
        Put(q); const auto before=cpu;
        const auto handles=kernel.handles().OpenHandleCount();
        const auto threads=kernel.threads().size(); const auto time=kernel.now_ns();
        CHECK(Call().kind == a32::ExitKind::Fallthrough && cpu.r[0] == 0);
        CHECK(cpu.r[15] == before.r[15]+4 && cpu.cpsr == before.cpsr);
        for (unsigned n=1;n<15;++n) CHECK(cpu.r[n]==before.r[n]);
        CHECK(Read()==expected && router.last_request()==q);
        CHECK(kernel.handles().OpenHandleCount()==handles && kernel.threads().size()==threads);
        CHECK(kernel.now_ns()==time && kernel.current_thread()->status==ThreadStatus::Running);
        CHECK(!router.unsupported_request() && router.last_host_error().empty());
    }
    void Stop(const IpcCommandBuffer& q) {
        Put(q); const auto before=cpu; const auto handles=kernel.handles().OpenHandleCount();
        CHECK(Call().kind==a32::ExitKind::Svc && router.unsupported_request());
        CHECK(cpu.r==before.r && cpu.cpsr==before.cpsr && Read()==q);
        CHECK(handles==kernel.handles().OpenHandleCount());
    }
};
void TotalAndSessionState() {
    Fixture f;
    IpcCommandBuffer q{}; q.fill(0xDEADBEEF); q[0]=0x000C0000;
    // Unused TLS words are not input parameters. An exact header is required.
    for (int n=0;n<3;++n) f.Reply(q,{0x000C0080,0,0});
    CHECK(f.kernel.CloseHandle(f.session)==0);
    CHECK(f.router.ConnectToService(f.kernel,"ptm:u",&f.session)==0);
    f.Reply(q,{0x000C0080,0,0});
    Fixture unconfigured(PtmStepMode::Unconfigured);
    unconfigured.FillOutput();
    unconfigured.Stop({0x000C0000});
    unconfigured.Stop(unconfigured.History());
    CHECK(unconfigured.Byte(Fixture::output)==0xA5);
    Fixture invalid(static_cast<PtmStepMode>(99)); invalid.Stop({0x000C0000});
}
void HistoryLayoutAndBounds() {
    Fixture f;
    for (const auto start : std::array<std::uint64_t,3>{0,0x123456789ABCDEF0ULL,0xFFFFFFFFFFFFFFFFULL}) {
        for (const std::uint32_t hours : {0U,1U,24U,kMaxPtmHistoryHours}) {
            f.FillOutput();
            auto q=f.History(hours,start);
            f.Reply(q,{0x000B0042,0,q[4],q[5]});
            CHECK(f.Byte(Fixture::output-1)==0xA5);
            for (unsigned n=0;n<hours*2;++n) CHECK(f.Byte(Fixture::output+n)==0);
            CHECK(f.Byte(Fixture::output+hours*2)==0xA5);
        }
    }
    // Zero-length output performs no dereference, even at a nonsensical address.
    f.Reply(f.History(0,0,0xFFFFFFFF),{0x000B0042,0,0xC,0xFFFFFFFF});
    // Write-only guest memory is sufficient; no input data are read from it.
    constexpr std::uint32_t wo=0x09000000;
    CHECK(f.memory.Map(wo,48,MemoryPermission::Write));
    f.Reply(f.History(24,0,wo),{0x000B0042,0,0x30C,wo});
    // Closing a PTM session doesn't invent new steps or erase independent state.
    CHECK(f.kernel.CloseHandle(f.session)==0);
    CHECK(f.router.ConnectToService(f.kernel,"ptm:u",&f.session)==0);
    f.Reply(f.History(),{0x000B0042,0,0x30C,Fixture::output});
}
void RequestGuards() {
    Fixture f; f.FillOutput();
    for (const auto header : {0x000C0040U,0x000C0002U,0x000B0082U,0x000B00C1U,
                              0x00070000U,0x00090000U,0xFFFF0000U}) {
        auto q=f.History(); q[0]=header; f.Stop(q);
    }
    for (const auto descriptor : {0x30AU,0x30EU,0x30DU,0x302U,0x18CU,0x31CU,0x2FCU,0U}) {
        auto q=f.History(); q[4]=descriptor; f.Stop(q);
    }
    for (const auto hours : {kMaxPtmHistoryHours+1,0x80000000U,0xFFFFFFFFU})
        f.Stop(f.History(hours));
    CHECK(f.Byte(Fixture::output)==0xA5);
    PtmService raw;
    auto q=f.History(); const auto before=q;
    CHECK(raw.Handle(f.router,f.kernel,f.memory,*f.kernel.current_thread(),q)==kResultNotFound);
    CHECK(q==before);
}
void PointerAndResponseGuards() {
    Fixture f; f.FillOutput();
    CHECK(f.memory.Map(0x0A000000,48,MemoryPermission::Read));
    CHECK(f.memory.Map(0x0B000000,47,rw));
    CHECK(f.memory.Map(0x0C000000,24,rw));
    CHECK(f.memory.Map(0x0C000018,24,MemoryPermission::Read));
    for (const auto address : {0xDEAD0000U,0xFFFFFFF0U,0x0A000000U,0x0B000000U,0x0C000000U}) {
        auto q=f.History(24,0,address); f.Put(q);
        CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer);
        CHECK(f.Read()==q && f.Byte(Fixture::output)==0xA5);
    }
    // The valid prefix of a short output must not be partially modified.
    for (unsigned n=0;n<47;++n) CHECK(f.memory.Write8(0x0B000000+n,0x5A));
    auto short_request=f.History(24,0,0x0B000000); f.Put(short_request);
    CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer);
    for (unsigned n=0;n<47;++n) CHECK(f.Byte(0x0B000000+n)==0x5A);
    for (bool partial : {false,true}) {
        const auto q=f.History(); f.Put(q);
        GuestMemory guarded;
        const unsigned length=partial ? 24 : sizeof(IpcCommandBuffer);
        CHECK(guarded.Map(f.cb(),length,partial ? rw : MemoryPermission::Read));
        CHECK(guarded.LoadBytes(f.cb(),{reinterpret_cast<const std::uint8_t*>(q.data()),length}));
        CHECK(guarded.Map(Fixture::output,48,rw));
        for (unsigned n=0;n<48;++n) CHECK(guarded.Write8(Fixture::output+n,0xA5));
        CHECK(f.Call(&guarded).kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer);
        for (unsigned n=0;n<48;++n) { std::uint8_t b=0; CHECK(guarded.Read8(Fixture::output+n,&b)&&b==0xA5); }
    }
    // Overlapping TLS response/output is an explicit unsupported host alias.
    for (const auto address : {f.cb(),f.cb()+4,f.cb()+static_cast<std::uint32_t>(sizeof(IpcCommandBuffer))-1}) {
        f.Stop(f.History(24,0,static_cast<std::uint32_t>(address)));
        CHECK(!f.router.last_host_error().empty());
    }
    // A subsequent good call clears the prior host diagnostic.
    f.Reply({0x000C0000},{0x000C0080,0,0});
}
void ExclusiveReservationAccounting() {
    Fixture f; f.FillOutput();
    std::uint64_t value=0, token=0; std::uint32_t fault=0;
    constexpr auto target=Fixture::output+3; // aligned address inside output
    CHECK(f.memory.LoadExclusive(target,4,&value,&token,&fault));
    f.Reply(f.History(),{0x000B0042,0,0x30C,Fixture::output});
    CHECK(f.memory.StoreExclusive(target,4,0xFFFFFFFF,token,&fault)==a32::ExclusiveStoreResult::ReservationLost);
    CHECK(f.Byte(target)==0);
    CHECK(f.memory.LoadExclusive(target,4,&value,&token,&fault));
    f.Reply(f.History(0),{0x000B0042,0,0xC,Fixture::output});
    CHECK(f.memory.StoreExclusive(target,4,0x12345678,token,&fault)==a32::ExclusiveStoreResult::Success);
    CHECK(f.memory.LoadExclusive(target,4,&value,&token,&fault));
    auto malformed=f.History(); malformed[4]=0x30A; f.Stop(malformed);
    CHECK(f.memory.StoreExclusive(target,4,0x87654321,token,&fault)==a32::ExclusiveStoreResult::Success);
}
void RunnerModeWiring() {
    Kernel k; GuestMemory m; const a32::Registry registry{};
    NativeRunner runner(registry,m,k,kDefaultRtcMsSince1900,{},PtmStepMode::EmptyHistory);
    CHECK(m.EnsureTlsMappings(k));
    Handle h=0; CHECK(runner.ipc().ConnectToService(k,"ptm:u",&h)==0);
    const auto cb=k.current_thread()->tls_address+kIpcCommandBufferOffset;
    CHECK(m.Write32(cb,0x000C0000));
    const auto status=runner.ipc().SendSyncRequest(k,m,h);
    CHECK(status && *status==0);
    std::uint32_t header=0; CHECK(m.Read32(cb,&header)&&header==0x000C0080);
}
} // namespace
int main() {
    TotalAndSessionState(); HistoryLayoutAndBounds(); RequestGuards();
    PointerAndResponseGuards(); ExclusiveReservationAccounting(); RunnerModeWiring();
    if (failures) return EXIT_FAILURE;
    std::cout << "PASS: explicit empty PTM profile, exact replies, mapped-output guards and unchanged scheduling\n";
    return EXIT_SUCCESS;
}
