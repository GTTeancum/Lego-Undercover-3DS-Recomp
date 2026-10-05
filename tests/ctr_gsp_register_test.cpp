#include "services/gsp_gpu_service.h"
#include "runtime/ctr_svc_bridge.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
using namespace lego::ctr;
constexpr auto RW=MemoryPermission::Read|MemoryPermission::Write;
int failures=0;
#define CHECK(x) do { if(!(x)){ std::cerr<<"FAIL "<<__LINE__<<": " #x "\n"; ++failures; } } while(0)
constexpr std::uint32_t kData=0x08000000,kMasks=kData+0x100;
struct Fixture {
    Kernel kernel;GuestMemory memory;IpcRouter router;SvcBridge bridge{kernel,&router};
    std::shared_ptr<GspGpuService> endpoint=std::make_shared<GspGpuService>();
    Handle handle{},event{};a32::GuestState cpu{};
    Fixture(){
        CHECK(memory.EnsureTlsMappings(kernel));CHECK(memory.Map(kData,0x1000,RW));
        CHECK(router.RegisterService("gsp::Gpu",endpoint)==0);Connect();
        CHECK(kernel.CreateEvent(&event,static_cast<std::uint32_t>(ResetType::OneShot))==0);
        Put({0x00130042,1,0,event});CHECK(Call().kind==a32::ExitKind::Fallthrough);
        CHECK(Read()[1]==kResultGspFirstInitialization);
    }
    void Connect(){CHECK(router.ConnectToService(kernel,"gsp::Gpu",&handle)==0);}
    std::uint32_t cb()const{return kernel.current_thread()->tls_address+kIpcCommandBufferOffset;}
    IpcCommandBuffer Request(std::uint32_t offset,std::uint32_t bytes=4,bool masked=false)const{
        if(masked)return {0x00020084,offset,bytes,(bytes<<14)|2,kData,(bytes<<14)|0x402,kMasks};
        return {0x00010082,offset,bytes,(bytes<<14)|2,kData};
    }
    void Put(const IpcCommandBuffer& q,GuestMemory* m=nullptr){
        if(!m)m=&memory;for(unsigned i=0;i<q.size();++i)CHECK(m->Write32(cb()+4*i,q[i]));
        for(unsigned i=0;i<16;++i)cpu.r[i]=0xABCD0000+i;
        cpu.r[0]=handle;cpu.r[15]=0x25947C;cpu.cpsr=0x60000010;cpu.fpscr=0x23000010;
    }
    IpcCommandBuffer Read(GuestMemory* m=nullptr){
        if(!m)m=&memory;IpcCommandBuffer q{};for(unsigned i=0;i<q.size();++i)CHECK(m->Read32(cb()+4*i,&q[i]));return q;
    }
    a32::ExecutionResult Call(GuestMemory* m=nullptr){
        return bridge.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcSendSyncRequest},cpu,m?m:&memory);
    }
    std::array<std::uint32_t,0x732> Snapshot()const {
        std::array<std::uint32_t,0x732> v{};
        for(unsigned i=0;i<v.size();++i)v[i]=endpoint->register_word(0x400000+4*i).value();return v;
    }
    void Stable(std::size_t handles,std::size_t threads,std::uint64_t now){
        CHECK(kernel.handles().OpenHandleCount()==handles && kernel.threads().size()==threads && kernel.now_ns()==now);
        CHECK(kernel.current_thread()->status==ThreadStatus::Running && !kernel.current_thread()->pending_wake);
        CHECK(!std::dynamic_pointer_cast<EventObject>(kernel.handles().Get(event))->signaled());
        for(const auto b:endpoint->shared_memory()->bytes())CHECK(b==0);
    }
    void Reply(const IpcCommandBuffer& q,Result result=0){
        Put(q);const auto before=cpu;
        const auto handles=kernel.handles().OpenHandleCount(),threads=kernel.threads().size();const auto now=kernel.now_ns();
        CHECK(Call().kind==a32::ExitKind::Fallthrough && cpu.r[0]==0 && cpu.r[15]==0x259480);
        for(unsigned i=1;i<15;++i)CHECK(cpu.r[i]==before.r[i]);CHECK(cpu.cpsr==before.cpsr && cpu.fpscr==before.fpscr);
        CHECK(Read()==(IpcCommandBuffer{IpcMakeHeader(IpcCommandId(q[0]),1,0),result}));
        CHECK(router.last_request()==q && !router.unsupported_request());Stable(handles,threads,now);
    }
    void Stop(const IpcCommandBuffer& q,GuestMemory* m=nullptr){
        Put(q,m);const auto before=cpu;const auto regs=Snapshot();
        const auto handles=kernel.handles().OpenHandleCount(),threads=kernel.threads().size();const auto now=kernel.now_ns();
        CHECK(Call(m).kind==a32::ExitKind::Svc && router.unsupported_request());
        CHECK(Read(m)==q && cpu.r==before.r && cpu.cpsr==before.cpsr && cpu.fpscr==before.fpscr);
        CHECK(Snapshot()==regs);Stable(handles,threads,now);
    }
};
void ResetAndStores(){
    Fixture f;
    CHECK(f.endpoint->register_word(0x4010D0)==1);
    CHECK(f.endpoint->register_word(0x4010C0)==0xFFFFFFF0U);
    CHECK(f.endpoint->register_word(0x401080)==0x12345678U);
    CHECK(f.endpoint->register_word(0x400468)==0x181E6000U);
    CHECK(f.endpoint->register_word(0x400494)==0x18273000U);
    CHECK(f.endpoint->register_word(0x40056C)==0x184C7800U);
    CHECK(f.endpoint->register_word(0x40045C)==((400U<<16)|240U));
    CHECK(f.endpoint->register_word(0x40055C)==((320U<<16)|240U));
    CHECK(f.endpoint->register_word(0x401A24)==0xA0000001U);
    CHECK(f.endpoint->register_word(0x400C18)==0 && f.endpoint->register_word(0x40001C)==0);
    CHECK(!f.endpoint->register_word(0x401CC8) && !f.endpoint->register_word(0x400001));
    CHECK(!f.endpoint->register_word(0x3FFFFC));
    for(auto v:{0U,0xDEADBEEFU,0xFFFFFFFFU}){
        CHECK(f.memory.Write32(kData,v));f.Reply(f.Request(0x401000));CHECK(f.endpoint->register_word(0x401000)==v);
        std::uint32_t input=0;CHECK(f.memory.Read32(kData,&input)&&input==v);
    }
    for(unsigned i=0;i<32;++i)CHECK(f.memory.Write32(kData+4*i,0x12340000+i));
    f.Reply(f.Request(0x400400,0x80));
    for(unsigned i=0;i<32;++i)CHECK(f.endpoint->register_word(0x400400+4*i)==0x12340000+i);
    f.Reply(f.Request(0x401CC4));CHECK(f.endpoint->register_word(0x401CC4)==0x12340000);
    // No separate GPU rights requirement is added to the reference WriteHWRegs parser.
    CHECK(!f.endpoint->rights_held());
    Handle duplicate=0;CHECK(f.kernel.DuplicateHandle(&duplicate,f.handle)==0);CHECK(f.kernel.CloseHandle(f.handle)==0);f.handle=duplicate;
    f.Reply(f.Request(0x401000));f.Connect();CHECK(f.endpoint->register_word(0x401000)==0x12340000);
    Fixture other;CHECK(other.endpoint->register_word(0x401000)==0);
}
void MasksAndDisabledTriggers(){
    Fixture f;CHECK(f.memory.Write32(kData,0xA5A5A5A5));f.Reply(f.Request(0x401000));
    for(auto mask:{0U,0xFFFFFFFFU,0x0F00FF03U}){
        auto old=f.endpoint->register_word(0x401000).value();CHECK(f.memory.Write32(kMasks,mask));
        CHECK(f.memory.Write32(kData,0x12345678));f.Reply(f.Request(0x401000,4,true));
        CHECK(f.endpoint->register_word(0x401000)==((old&~mask)|(0x12345678&mask)));
    }
    for(unsigned i=0;i<32;++i){CHECK(f.memory.Write32(kData+4*i,0x33330000+i));CHECK(f.memory.Write32(kMasks+4*i,0x00FF00FF));}
    f.Reply(f.Request(0x400600,0x80,true));
    for(unsigned i=0;i<32;++i)CHECK(f.endpoint->register_word(0x400600+4*i)==((0x33330000+i)&0x00FF00FF));
    auto q=f.Request(0x401000,4,true);q[6]=q[4];f.Reply(q); // Shared data/mask input is well-defined.
    for(auto offset:{0x40001CU,0x40002CU,0x400C18U}){
        CHECK(f.memory.Write32(kData,0x302));f.Reply(f.Request(offset));CHECK(f.endpoint->register_word(offset)==0x302);
        CHECK(f.memory.Write32(kData,0));CHECK(f.memory.Write32(kMasks,0xFF));f.Reply(f.Request(offset,4,true));
        CHECK(f.endpoint->register_word(offset)==0x300); // No synthetic finished bit or IRQ.
        CHECK(f.memory.Write32(kData,1));f.Stop(f.Request(offset));
        CHECK(f.memory.Write32(kMasks,1));f.Stop(f.Request(offset,4,true));
    }
    for(auto offset:{0x4018F0U,0x4018F4U}){
        CHECK(f.memory.Write32(kData,0));f.Reply(f.Request(offset));
        for(auto v:{1U,2U,0x80000000U}){CHECK(f.memory.Write32(kData,v));f.Stop(f.Request(offset));}
    }
    CHECK(f.memory.Write32(kData,0xFFFFFFFF));CHECK(f.memory.Write32(kData+4,1));
    f.Stop(f.Request(0x400018,8)); // Prefix value cannot leak through an active trigger tail.
    f.Stop(f.Request(0x401CC4,8)); // Same atomic preflight at end of modeled bank.
}
void RequestsAndErrors(){
    Fixture f;auto q=f.Request(0x401000);
    for(unsigned i:{0U,3U}){auto bad=q;bad[i]^=1;f.Stop(bad);}
    q=f.Request(0x401000,4,true);
    for(unsigned i:{0U,3U,5U}){auto bad=q;bad[i]^=0x400;f.Stop(bad);}
    auto huge=f.Request(0x401000,0x40000);f.Stop(huge);
    for(auto off:{1U,0x420000U,0xFFFFFFFCU})f.Reply(f.Request(off),0xE0E02A01U);
    f.Reply(f.Request(1,129),0xE0E02A01U); // Address validation wins.
    f.Reply(f.Request(0x401000,129),0xE0E02BECU); // Size validation wins over alignment.
    f.Reply(f.Request(0x401000,3),0xE0E02BF2U);
    f.Reply(f.Request(0x401000,3,true),0xE0E02BF2U);
    f.Stop(f.Request(0x202000));f.Stop(f.Request(0x401CC8));
    f.Stop({0x00040080,0x401000,4});f.Stop({0x000C0000}); // Read and queue execution unimplemented.
    for(bool masked:{false,true}){
        auto zero=f.Request(0x401000,0,masked);zero[4]=0xFFFFFFFF;zero[6]=0xFFFFFFFF;
        const auto regs=f.Snapshot();f.Reply(zero);CHECK(f.Snapshot()==regs);
    }
    // No new handles needed even when the handle table is exhausted.
    std::vector<Handle> handles;
    for(;;){Handle h=0;auto result=f.kernel.DuplicateHandle(&h,f.event);if(result){CHECK(result==kResultOutOfHandles);break;}handles.push_back(h);}
    f.Reply(f.Request(0x401000));for(auto h:handles)CHECK(f.kernel.CloseHandle(h)==0);
}
void MemoryGuards(){
    Fixture f;
    for(bool masked:{false,true})for(auto address:{0xDEAD0000U,0xFFFFFFFCU,kData+0xFFC}){
        auto q=f.Request(0x400600,8,masked);q[masked?6:4]=address;f.Put(q);auto old=f.Snapshot();
        CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer);
        CHECK(f.Read()==q && f.Snapshot()==old);
    }
    for(bool partial:{false,true}){
        auto q=f.Request(0x401000);GuestMemory m;const auto n=partial?20U:unsigned(sizeof(q));
        CHECK(m.Map(f.cb(),n,partial?RW:MemoryPermission::Read));CHECK(m.LoadBytes(f.cb(),{reinterpret_cast<const std::uint8_t*>(q.data()),n}));
        CHECK(m.Map(kData,4,MemoryPermission::Read));const std::array<std::uint8_t,4> d{1,2,3,4};CHECK(m.LoadBytes(kData,d));
        f.cpu.r[0]=f.handle;f.cpu.r[15]=0x25947C;const auto old=f.Snapshot();
        CHECK(f.Call(&m).kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer && f.Snapshot()==old);
        std::uint32_t word=0;CHECK(m.Read32(f.cb(),&word)&&word==q[0]);
    }
    for(bool masked:{false,true}){auto q=f.Request(0x401000,4,masked);q[masked?6:4]=f.cb();f.Stop(q);}
    // Different VAs of the SAME shared backing must also fail the input/reply alias guard.
    GuestMemory aliases;auto page=std::make_shared<ServiceSharedMemoryObject>();
    CHECK(aliases.MapSharedServicePage(f.cb()-kIpcCommandBufferOffset,page,RW));CHECK(aliases.MapSharedServicePage(kData,page,RW));
    CHECK(aliases.SpansAlias(f.cb(),0x100,kData+0x80,4));CHECK(!aliases.SpansAlias(f.cb(),4,kData+0x84,4));
    CHECK(!aliases.SpansAlias(f.cb(),0,kData+0x80,4));CHECK(!aliases.SpansAlias(f.cb(),4,0xDEAD0000,4));
    auto q=f.Request(0x401000);q[4]=kData+0x80;f.Stop(q,&aliases);
    // Read-only input stays unchanged; register writes do not invalidate source exclusives.
    CHECK(f.memory.Write32(kData,0xAABBCCDD));std::uint64_t value=0,token=0;std::uint32_t fault=0;
    CHECK(f.memory.LoadExclusive(kData,4,&value,&token,&fault));f.Reply(f.Request(0x401000));
    CHECK(f.memory.StoreExclusive(kData,4,value,token,&fault)==a32::ExclusiveStoreResult::Success);
}
}
int main(){ResetAndStores();MasksAndDisabledTriggers();RequestsAndErrors();MemoryGuards();if(failures)return EXIT_FAILURE;
std::cout<<"PASS: reference-reset passive GPU MMIO, masked writes, atomic trigger stops and input/response guards\n";return EXIT_SUCCESS;}
