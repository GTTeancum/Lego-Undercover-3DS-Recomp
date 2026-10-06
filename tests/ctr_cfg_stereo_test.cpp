#include "runtime/ctr_runner.h"
#include "services/cfg_service.h"
#include "services/gsp_gpu_service.h"
#include <array>
#include <bit>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

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
    Fixture(CfgProfile mode=CfgProfile::ReferenceStereo) : cfg(std::make_shared<CfgService>(mode)) {
        CHECK(memory.EnsureTlsMappings(kernel));
        CHECK(memory.Map(0x08000000,0x1000,MemoryPermission::Read|MemoryPermission::Write));
        CHECK(router.RegisterService("cfg:u",cfg)==0);
        CHECK(router.ConnectToService(kernel,"cfg:u",&handle)==0);
    }
    std::uint32_t cb() const {return kernel.current_thread()->tls_address+kIpcCommandBufferOffset;}
    IpcCommandBuffer Request(std::uint32_t dst=target) const {return {0x00010082,32,0x50005,0x20c,dst};}
    void Put(const IpcCommandBuffer& q) {
        for(unsigned i=0;i<q.size();++i) CHECK(memory.Write32(cb()+i*4,q[i]));
        for(unsigned i=0;i<16;++i)cpu.r[i]=0x12340000+i;
        cpu.r[0]=handle;cpu.r[15]=0x25947c;cpu.cpsr=0xa0000010;cpu.fpscr=0x83000018;
    }
    IpcCommandBuffer Read() {
        IpcCommandBuffer q{};
        for(unsigned i=0;i<q.size();++i)CHECK(memory.Read32(cb()+4*i,&q[i]));
        return q;
    }
    std::vector<std::uint8_t> Bytes(std::uint32_t p,unsigned n) {
        std::vector<std::uint8_t> v(n);
        for(unsigned i=0;i<n;++i)CHECK(memory.Read8(p+i,&v[i]));
        return v;
    }
    a32::ExecutionResult Call(GuestMemory* alternate=nullptr) {
        return bridge.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcSendSyncRequest},cpu,alternate?alternate:&memory);
    }
    void Stop(IpcCommandBuffer q) {
        Put(q);const auto before=cpu, saved=kernel.CurrentGuestState();
        const auto count=kernel.handles().OpenHandleCount();const auto now=kernel.now_ns();
        const auto output=Bytes(0x08000000,0x1000);
        CHECK(Call().kind==a32::ExitKind::Svc && router.unsupported_request());
        CHECK(cpu.r==before.r && cpu.cpsr==before.cpsr && cpu.fpscr==before.fpscr);
        CHECK(Read()==q && Bytes(0x08000000,0x1000)==output);
        CHECK(kernel.now_ns()==now && kernel.handles().OpenHandleCount()==count);
        CHECK(kernel.CurrentGuestState().r==saved.r);
    }
};
void GoldenAndState() {
    // Independently express the reference's floats; verify explicit LE serialization.
    const std::array<float,8> values{62.0f,289.0f,76.80000305175781f,46.08000183105469f,
                                    10.0f,5.0f,55.58000183105469f,21.56999969482422f};
    std::vector<std::uint8_t> golden;
    for(float f:values){const auto w=std::bit_cast<std::uint32_t>(f);for(unsigned b=0;b<4;++b)golden.push_back(w>>(b*8));}
    CHECK(std::equal(golden.begin(),golden.end(),CfgService::ReferenceStereoBytes().begin()));
    Fixture f;CHECK(f.cfg->profile()==CfgProfile::ReferenceStereo);
    Handle ev{};CHECK(f.kernel.CreateEvent(&ev,0)==0);
    const auto event=std::dynamic_pointer_cast<EventObject>(f.kernel.handles().Get(ev));
    std::array<std::uint8_t,0x1000> sentinel{};sentinel.fill(0xa5);
    CHECK(f.memory.LoadBytes(0x08000000,sentinel));
    auto token=[&](std::uint32_t a){std::uint64_t v{},t{};std::uint32_t fault{};CHECK(f.memory.LoadExclusive(a,4,&v,&t,&fault));return t;};
    const auto neighbour=token(0x08000040), before_written=token(0x08000008);
    const auto count=f.kernel.handles().OpenHandleCount();const auto now=f.kernel.now_ns();
    for(unsigned i=0;i<2;++i) {
        f.Put(f.Request());const auto before=f.cpu;
        CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==0 && f.cpu.r[15]==0x259480);
        for(unsigned n=1;n<15;++n)CHECK(f.cpu.r[n]==before.r[n]);
        CHECK(f.cpu.cpsr==before.cpsr && f.cpu.fpscr==before.fpscr);
        CHECK(f.Read()==(IpcCommandBuffer{0x00010042,0,0x20c,Fixture::target}));
        CHECK(f.Bytes(Fixture::target,32)==golden);
        CHECK(f.Bytes(0x08000000,3)==std::vector<std::uint8_t>(3,0xa5));
        CHECK(f.Bytes(Fixture::target+32,0x1000-35)==std::vector<std::uint8_t>(0x1000-35,0xa5));
        CHECK(!event->signaled() && f.kernel.now_ns()==now && f.kernel.handles().OpenHandleCount()==count);
    }
    CHECK(token(0x08000008)!=before_written && token(0x08000040)==neighbour);
    Handle second{};CHECK(f.router.ConnectToService(f.kernel,"cfg:u",&second)==0);
    CHECK(std::dynamic_pointer_cast<ClientSessionObject>(f.kernel.handles().Get(second))->service==f.cfg);
    f.handle=second;f.Put(f.Request());CHECK(f.Call().kind==a32::ExitKind::Fallthrough);
    // No hidden capacity requirement: the operation allocates no guest handles.
    Handle extra{};
    while(f.kernel.handles().Create(&extra,std::make_shared<EventObject>(ResetType::OneShot))==0){}
    f.Put(f.Request());CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.Read()[1]==0);
}
void Guards() {
    Fixture strict(CfgProfile::Unconfigured);strict.Stop(strict.Request());
    bool threw=false;try{CfgService invalid(static_cast<CfgProfile>(255));}catch(const std::invalid_argument&){threw=true;}CHECK(threw);
    Fixture f;
    for(unsigned word:{0U,1U,2U,3U}) {auto q=f.Request();q[word]^=1;f.Stop(q);}
    for(auto size:{0U,1U,31U,33U,0xffffffffU}) {auto q=f.Request();q[1]=size;q[3]=(size<<4)|0xc;f.Stop(q);}
    for(auto desc:{0x208U,0x20eU,0x80002U,0U}){auto q=f.Request();q[3]=desc;f.Stop(q);}
    for(auto id:{0x000a0002U,0x00050006U,0U}){auto q=f.Request();q[2]=id;f.Stop(q);}
    for(auto a:{0x09000000U,0xfffffff0U,0x08000ff0U}) {
        auto q=f.Request(a);f.Put(q);
        CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer && f.Read()==q);
    }
    CHECK(f.memory.Map(0x09000000,32,MemoryPermission::Read));
    auto q=f.Request(0x09000000);f.Put(q);
    CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer && f.Read()==q);
    f.Stop(f.Request(f.cb()+4));CHECK(!f.router.last_host_error().empty());
    // Adjacent fragmented mappings are not silently treated as one backing span.
    CHECK(f.memory.Map(0x0a000000,16,MemoryPermission::Write));
    CHECK(f.memory.Map(0x0a000010,16,MemoryPermission::Write));
    q=f.Request(0x0a000000);f.Put(q);
    CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer && f.Read()==q);
}
void ProtectedAndShared() {
    Fixture f;
    for(auto permission:{MemoryPermission::Read,MemoryPermission::Write,MemoryPermission::Read|MemoryPermission::Write}) {
        GuestMemory m;
        const auto size=permission==(MemoryPermission::Read|MemoryPermission::Write)?16U:256U;
        CHECK(m.Map(f.cb(),size,permission));
        CHECK(m.Map(Fixture::target,32,MemoryPermission::Read|MemoryPermission::Write));
        const auto q=f.Request();
        CHECK(m.LoadBytes(f.cb(),{reinterpret_cast<const std::uint8_t*>(q.data()),size}));
        f.Put(q);CHECK(f.Call(&m).kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer);
        std::uint8_t b{};CHECK(m.Read8(Fixture::target,&b)&&b==0);
    }
    // An output mapping with only Write permission is valid.
    CHECK(f.memory.Map(0x0b000000,32,MemoryPermission::Write));f.Put(f.Request(0x0b000000));
    CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.Read()[1]==0);
    auto shared=std::make_shared<ServiceSharedMemoryObject>();
    CHECK(f.memory.MapSharedServicePage(0x10000000,shared,MemoryPermission::Read|MemoryPermission::Write));
    const auto before=std::vector<std::uint8_t>(shared->bytes().begin(),shared->bytes().end());
    f.Stop(f.Request(0x10000000));CHECK(!f.router.last_host_error().empty());
    CHECK(std::equal(before.begin(),before.end(),shared->bytes().begin()) && shared->Epoch(0)==0);
    // A second VA aliasing the reply must stop even when numeric VAs are disjoint.
    GuestMemory alias;
    auto page=std::make_shared<ServiceSharedMemoryObject>();
    const auto tls=f.kernel.current_thread()->tls_address;
    CHECK(alias.MapSharedServicePage(tls,page,MemoryPermission::Read|MemoryPermission::Write));
    CHECK(alias.MapSharedServicePage(0x11000000,page,MemoryPermission::Read|MemoryPermission::Write));
    auto q=f.Request(0x11000000+kIpcCommandBufferOffset);
    for(unsigned i=0;i<q.size();++i)CHECK(alias.Write32(f.cb()+4*i,q[i]));
    const auto snapshot=std::vector<std::uint8_t>(page->bytes().begin(),page->bytes().end());const auto epoch=page->Epoch(kIpcCommandBufferOffset);
    f.Put(q);const auto cpu=f.cpu;CHECK(f.Call(&alias).kind==a32::ExitKind::Svc && f.cpu.r==cpu.r);
    CHECK(std::equal(snapshot.begin(),snapshot.end(),page->bytes().begin()) && page->Epoch(kIpcCommandBufferOffset)==epoch);
}
}
int main(){GoldenAndState();Guards();ProtectedAndShared();if(failures)return EXIT_FAILURE;
 std::cout<<"PASS: explicit CFG stereo profile, exact bytes, guarded mapped output and unchanged platform state\n";return EXIT_SUCCESS;}
