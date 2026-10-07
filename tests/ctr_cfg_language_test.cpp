#include "runtime/ctr_runner.h"
#include "services/cfg_service.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <vector>

// Test-only deterministic failure at the next allocation, not a production hook.
namespace { bool fail_next_allocation{}; }
void* operator new(std::size_t n) {
    if (fail_next_allocation) { fail_next_allocation=false; throw std::bad_alloc(); }
    if (auto* p=std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }

namespace {
using namespace lego::ctr;
int failures{};
#define CHECK(x) do { if (!(x)) { std::cerr<<"FAIL "<<__LINE__<<": " #x "\n"; ++failures; } } while(0)
struct Fixture {
    Kernel kernel;
    GuestMemory memory;
    IpcRouter router;
    SvcBridge bridge{kernel,&router};
    std::shared_ptr<CfgService> cfg;
    Handle handle{};
    a32::GuestState cpu{};
    static constexpr std::uint32_t target=0x08000003;
    Fixture(CfgLanguage language=CfgLanguage::English,
            CfgProfile camera=CfgProfile::Unconfigured)
        : cfg(std::make_shared<CfgService>(camera,CfgSoundMode::Unconfigured,language)) {
        CHECK(memory.EnsureTlsMappings(kernel));
        CHECK(memory.Map(0x08000000,0x2000,MemoryPermission::Read|MemoryPermission::Write));
        CHECK(router.RegisterService("cfg:u",cfg)==0);
        CHECK(router.ConnectToService(kernel,"cfg:u",&handle)==0);
        std::array<std::uint8_t,0x2000> fill{};fill.fill(0xa5);
        CHECK(memory.LoadBytes(0x08000000,fill));
    }
    std::uint32_t cb() const { return kernel.current_thread()->tls_address+kIpcCommandBufferOffset; }
    IpcCommandBuffer Request(std::uint32_t dst=target) const {
        IpcCommandBuffer q{};q.fill(0xfeed1234);q[0]=0x00010082;q[1]=1;q[2]=0xa0002;q[3]=0x1c;q[4]=dst;return q;
    }
    void Put(const IpcCommandBuffer& q) {
        for(unsigned i=0;i<q.size();++i) CHECK(memory.Write32(cb()+4*i,q[i]));
        for(unsigned i=0;i<16;++i)cpu.r[i]=0x12340000+i;
        cpu.r[0]=handle;cpu.r[15]=0x25947c;cpu.cpsr=0xa0000010;cpu.fpscr=0x83000018;
    }
    IpcCommandBuffer Read() {
        IpcCommandBuffer q{};for(unsigned i=0;i<q.size();++i) CHECK(memory.Read32(cb()+4*i,&q[i]));return q;
    }
    std::vector<std::uint8_t> Bytes(std::uint32_t p=0x08000000,unsigned size=0x2000) {
        std::vector<std::uint8_t> v(size);for(unsigned i=0;i<size;++i)CHECK(memory.Read8(p+i,&v[i]));return v;
    }
    a32::ExecutionResult Call(GuestMemory* alt=nullptr) {
        return bridge.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcSendSyncRequest},cpu,alt?alt:&memory);
    }
    void Stop(IpcCommandBuffer q) {
        Put(q);auto before=Bytes();const auto saved=cpu;
        const auto count=kernel.handles().OpenHandleCount();const auto now=kernel.now_ns();
        CHECK(Call().kind==a32::ExitKind::Svc && router.unsupported_request());
        CHECK(Read()==q && Bytes()==before && cpu.r==saved.r);
        CHECK(cpu.cpsr==saved.cpsr && cpu.fpscr==saved.fpscr);
        CHECK(kernel.now_ns()==now && kernel.handles().OpenHandleCount()==count);
    }
};
void ValuesAndIsolation() {
    // Numeric values independently pinned by CFG SystemLanguage, not inferred from the host locale.
    for(unsigned v=0;v<12;++v) {
        Fixture f(static_cast<CfgLanguage>(v));
        CHECK(f.cfg->profile()==CfgProfile::Unconfigured && unsigned(f.cfg->language())==v);
        Handle event_h{};CHECK(f.kernel.CreateEvent(&event_h,0)==0);
        auto event=std::dynamic_pointer_cast<EventObject>(f.kernel.handles().Get(event_h));
        std::uint64_t old_value{},written_token{},neighbour_token{};std::uint32_t fault{};
        CHECK(f.memory.LoadExclusive(0x08000000,4,&old_value,&written_token,&fault));
        CHECK(f.memory.LoadExclusive(0x08000010,4,&old_value,&neighbour_token,&fault));
        auto expected=f.Bytes();expected[3]=v;
        for(unsigned repeat=0;repeat<2;++repeat) {
            auto q=f.Request();f.Put(q);const auto saved=f.cpu;
            const auto now=f.kernel.now_ns();const auto handles=f.kernel.handles().OpenHandleCount();
            CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==0);
            CHECK(f.cpu.r[15]==0x259480 && f.cpu.cpsr==saved.cpsr && f.cpu.fpscr==saved.fpscr);
            for(unsigned r=1;r<15;++r) CHECK(f.cpu.r[r]==saved.r[r]);
            CHECK(f.Read()==(IpcCommandBuffer{0x00010042,0,0x1c,Fixture::target}));
            CHECK(f.Bytes()==expected && !event->signaled());
            CHECK(f.kernel.now_ns()==now && f.kernel.handles().OpenHandleCount()==handles);
        }
        std::uint64_t after{};
        CHECK(f.memory.LoadExclusive(0x08000000,4,&old_value,&after,&fault) && after!=written_token);
        CHECK(f.memory.LoadExclusive(0x08000010,4,&old_value,&after,&fault) && after==neighbour_token);
        auto camera=f.Request();camera[1]=32;camera[2]=0x50005;camera[3]=0x20c;
        f.Stop(camera); // Sound selection cannot enable camera defaults.
        Handle second{};CHECK(f.router.ConnectToService(f.kernel,"cfg:u",&second)==0);
        CHECK(std::dynamic_pointer_cast<ClientSessionObject>(f.kernel.handles().Get(second))->service==f.cfg);
        f.handle=second;f.Put(f.Request());CHECK(f.Call().kind==a32::ExitKind::Fallthrough);
    }
    Fixture omitted(CfgLanguage::Unconfigured,CfgProfile::ReferenceStereo);
    omitted.Stop(omitted.Request()); // Camera selection cannot enable language.
    Fixture sound_only(CfgLanguage::Unconfigured,CfgProfile::Unconfigured);
    // Independent service: selecting sound and camera must NOT enable language.
    auto prefs=std::make_shared<CfgService>(CfgProfile::ReferenceStereo,CfgSoundMode::Stereo);
    CHECK(!prefs->CanHandle(sound_only.Request()));
    Fixture language_only;
    auto sound_request=language_only.Request();sound_request[2]=kCfgSoundBlock;
    language_only.Stop(sound_request);
    CfgService all(CfgProfile::ReferenceStereo,CfgSoundMode::Mono,CfgLanguage::Spanish);
    CHECK(all.language()==CfgLanguage::Spanish && all.sound_mode()==CfgSoundMode::Mono);
    CHECK(all.CanHandle(language_only.Request()) && all.CanHandle(sound_request));
    Fixture both(CfgLanguage::French,CfgProfile::ReferenceStereo);
    auto q=both.Request();q[1]=32;q[2]=0x50005;q[3]=0x20c;
    both.Put(q);CHECK(both.Call().kind==a32::ExitKind::Fallthrough);
    CHECK(std::equal(CfgService::ReferenceStereoBytes().begin(),CfgService::ReferenceStereoBytes().end(),both.Bytes(Fixture::target,32).begin()));
    both.Put(both.Request());CHECK(both.Call().kind==a32::ExitKind::Fallthrough && both.Bytes(Fixture::target,1)[0]==2);
    for(unsigned v=12;v<255;++v) {
        bool thrown=false;try {CfgService s(CfgProfile::Unconfigured,CfgSoundMode::Unconfigured,static_cast<CfgLanguage>(v));}
        catch(const std::invalid_argument&) {thrown=true;}CHECK(thrown);
    }
}
void ShapeAndPermissions() {
    Fixture f;
    for(unsigned w:{0U,1U,2U,3U}) {auto q=f.Request();q[w]^=1;f.Stop(q);}
    for(auto size:{0U,2U,32U,0xffffffffU}) {auto q=f.Request();q[1]=size;q[3]=(size<<4)|0xc;f.Stop(q);}
    for(auto desc:{0U,0x18U,0x1eU,0x4002U,0x1001cU}) {auto q=f.Request();q[3]=desc;f.Stop(q);}
    for(auto a:{0U,0x09000000U,0x08002000U,0xffffffffU}) {
        auto q=f.Request(a);f.Put(q);auto saved=f.Bytes();
        CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer);
        CHECK(f.Read()==q && f.Bytes()==saved);
    }
    CHECK(f.memory.Map(0x09000000,1,MemoryPermission::Read));f.Put(f.Request(0x09000000));
    CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer);
    CHECK(f.memory.Map(0x09100000,1,MemoryPermission::Write));f.Put(f.Request(0x09100000));
    CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.Read()[1]==0);
    // One-byte output may occupy the final byte of a valid private mapping.
    f.Put(f.Request(0x08001fff));CHECK(f.Call().kind==a32::ExitKind::Fallthrough);
    CHECK(f.Bytes(0x08001ffe,2)==(std::vector<std::uint8_t>{0xa5,1}));
    f.Stop(f.Request(f.cb()+4));CHECK(!f.router.last_host_error().empty());
    for(auto perm:{MemoryPermission::Read,MemoryPermission::Write,MemoryPermission::Read|MemoryPermission::Write}) {
        GuestMemory m;const unsigned size=perm==(MemoryPermission::Read|MemoryPermission::Write)?16:256;
        CHECK(m.Map(f.cb(),size,perm));CHECK(m.Map(Fixture::target,1,MemoryPermission::Read|MemoryPermission::Write));
        auto q=f.Request();CHECK(m.LoadBytes(f.cb(),{reinterpret_cast<const std::uint8_t*>(q.data()),size}));
        f.Put(q);CHECK(f.Call(&m).kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer);
        std::uint8_t out{};CHECK(m.Read8(Fixture::target,&out) && out==0);
    }
}
void AliasesAndFailure() {
    Fixture f;auto mapped=f.memory.MapUserAlias(0x0e000000,0x08000000,0x1000,3);CHECK(mapped && *mapped==0);
    f.Put(f.Request(0x0e000003));CHECK(f.Call().kind==a32::ExitKind::Fallthrough);
    CHECK(f.Bytes(Fixture::target,1)[0]==1 && f.Bytes(0x0e000003,1)[0]==1);
    auto page=std::make_shared<ServiceSharedMemoryObject>();
    CHECK(f.memory.MapSharedServicePage(0x10000000,page,MemoryPermission::Read|MemoryPermission::Write));
    f.Stop(f.Request(0x10000000));CHECK(page->bytes()[0]==0 && page->Epoch(0)==0);
    Fixture alias;alias.kernel.current_thread()->tls_address=0x08000000;
    mapped=alias.memory.MapUserAlias(0x0e000000,0x08000000,0x1000,3);CHECK(mapped && *mapped==0);
    alias.Stop(alias.Request(0x0e000080)); // Distinct addresses, identical IPC backing.
    Fixture allocation;auto q=allocation.Request();allocation.Put(q);auto saved=allocation.Bytes();
    // Call handler directly to make the failed allocation precisely the prepared
    // output epoch. Request creation already established all IPC epochs.
    fail_next_allocation=true;
    auto result=allocation.cfg->Handle(allocation.router,allocation.kernel,allocation.memory,*allocation.kernel.current_thread(),q);
    CHECK(!fail_next_allocation && result==0 && allocation.router.unsupported_request());
    CHECK(q==allocation.Request() && allocation.Bytes()==saved);
    // Successful read requires no new guest handles, even at table capacity.
    Fixture full;Handle h{};auto object=std::make_shared<EventObject>(ResetType::OneShot);
    while(full.kernel.handles().Create(&h,object)==0) {}
    full.Put(full.Request());CHECK(full.Call().kind==a32::ExitKind::Fallthrough && full.Read()[1]==0);
}
void RunnerWiring() {
    const a32::Registry registry{};GuestMemory m;Kernel k;
    NativeRunner r(registry,m,k,kDefaultRtcMsSince1900,{},PtmStepMode::Unconfigured,{},GpuVramMode::Unconfigured,
        DisplayClockMode::Disabled,CfgProfile::Unconfigured,CpuExecutionMode::Strict,{},{},CfgSoundMode::Unconfigured,CfgLanguage::Japanese);
    Handle h{};CHECK(r.ipc().ConnectToService(k,"cfg:u",&h)==0);
    auto session=std::dynamic_pointer_cast<ClientSessionObject>(k.handles().Get(h));
    auto service=std::dynamic_pointer_cast<CfgService>(session->service);
    CHECK(service && service->language()==CfgLanguage::Japanese && service->profile()==CfgProfile::Unconfigured);
}
}
int main() {
    ValuesAndIsolation();ShapeAndPermissions();AliasesAndFailure();RunnerWiring();
    if(failures) return EXIT_FAILURE;
    std::cout<<"PASS: explicit independent CFG language choices, exact mapped byte, epochs, rollback, aliases and runner wiring\n";
    return EXIT_SUCCESS;
}
