#include "services/gsp_gpu_service.h"
#include "runtime/ctr_svc_bridge.h"
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace lego::ctr;
namespace {
constexpr auto RW=MemoryPermission::Read|MemoryPermission::Write;
constexpr std::uint32_t kShared=0x10000000,kSource=0x14000000;
int failures=0;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}}while(0)
struct Fixture {
    Kernel kernel;GuestMemory memory;IpcRouter ipc;SvcBridge bridge{kernel,&ipc};
    std::shared_ptr<GspGpuService> module=std::make_shared<GspGpuService>();
    Handle session{},event{},spare{};std::shared_ptr<EventObject> object;a32::GuestState cpu{};unsigned slot{};
    Fixture(bool second=false,bool registered=true):slot(second?1:0) {
        CHECK(memory.EnsureTlsMappings(kernel));CHECK(ipc.RegisterService("gsp::Gpu",module)==0);
        if(second)CHECK(ipc.ConnectToService(kernel,"gsp::Gpu",&spare)==0);
        CHECK(ipc.ConnectToService(kernel,"gsp::Gpu",&session)==0);
        CHECK(kernel.CreateEvent(&event,0)==0);object=std::dynamic_pointer_cast<EventObject>(kernel.handles().Get(event));
        if(registered){Put({0x00130042,1,0,event});CHECK(Call().kind==a32::ExitKind::Fallthrough);}
        Put({0x00160042,0,0,kCurrentProcessPseudoHandle});CHECK(Call().kind==a32::ExitKind::Fallthrough);
        CHECK(memory.MapSharedServicePage(kShared,module->shared_memory(),RW));CHECK(memory.Map(kSource,0x1000,RW));
    }
    std::uint32_t cb() const {return kernel.current_thread()->tls_address+kIpcCommandBufferOffset;}
    void Put(const IpcCommandBuffer& q={0x000C0000},GuestMemory* target=nullptr) {
        auto& m=target?*target:memory;for(unsigned i=0;i<q.size();++i)CHECK(m.Write32(cb()+4*i,q[i]));
        for(unsigned i=0;i<16;++i)cpu.r[i]=0xABC00000+i;cpu.r[0]=session;cpu.r[15]=0x25947C;
        cpu.cpsr=0x60000010;cpu.fpscr=0x23000010;
    }
    a32::ExecutionResult Call(GuestMemory* target=nullptr) {return bridge.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcSendSyncRequest},cpu,target?target:&memory);}
    void Word(std::uint32_t offset,std::uint32_t v){CHECK(memory.Write32(kShared+offset,v));}
    std::uint32_t Word(std::uint32_t offset){std::uint32_t v=0;CHECK(memory.Read32(kShared+offset,&v));return v;}
    std::uint32_t queue()const{return 0x800+slot*0x200;}
    void List(std::initializer_list<std::uint32_t> words) {unsigned i=0;for(auto v:words)CHECK(memory.Write32(kSource+4*i++,v));Packet(0,kSource,static_cast<std::uint32_t>(words.size()*4));Word(queue(),0x100);}
    void Packet(unsigned index,std::uint32_t addr=kSource,std::uint32_t size=16,std::uint32_t kind=1) {
        const auto p=queue()+0x20+index*0x20;Word(p,kind);
        Word(p+4,addr);Word(p+8,size);for(unsigned i=3;i<8;++i)Word(p+4*i,0);
    }
    std::vector<std::uint8_t> Page()const{auto p=module->shared_memory()->bytes();return {p.begin(),p.end()};}
    std::vector<std::uint32_t> Registers()const{std::vector<std::uint32_t> v;for(unsigned i=0;i<kPicaGpuWords;++i)v.push_back(*module->register_word(0x400000+i*4));return v;}
    void Stop(GuestMemory* m=nullptr){Put({0xC0000},m);auto page=Page();auto regs=Registers();auto state=module->pica_uploads();auto before=cpu;auto signaled=object->signaled();
        CHECK(Call(m).kind==a32::ExitKind::Svc&&ipc.unsupported_request());CHECK(cpu.r==before.r&&Page()==page&&Registers()==regs&&module->pica_uploads()==state);CHECK(object->signaled()==signaled&&kernel.now_ns()==0);}
};
void ExecuteAndSignal(){
    Fixture f;f.List({0xABCDEF01,0x000F0080,0x12345678,0x000F0010});
    f.Put();auto before=f.cpu;auto page=f.Page();const auto handles=f.kernel.handles().OpenHandleCount();
    std::uint64_t value=0,source_token=0,header_token=0;std::uint32_t fault=0;
    CHECK(f.memory.LoadExclusive(kSource,4,&value,&source_token,&fault));CHECK(f.memory.LoadExclusive(kShared+f.queue(),4,&value,&header_token,&fault));
    CHECK(f.Call().kind==a32::ExitKind::Fallthrough&&f.cpu.r[0]==0&&f.cpu.r[15]==0x259480);
    for(unsigned i=1;i<15;++i)CHECK(f.cpu.r[i]==before.r[i]);CHECK(f.cpu.cpsr==before.cpsr&&f.cpu.fpscr==before.fpscr);
    CHECK(f.Word(f.queue())==1&&*f.module->register_word(0x401200)==0xABCDEF01);
    CHECK(*f.module->register_word(0x4018E0)==2&&*f.module->register_word(0x4018E8)==0x04000000&&*f.module->register_word(0x4018F0)==0);
    page[0x800]=1;page[0x801]=0;page[1]=1;page[12]=5;CHECK(f.Page()==page);
    CHECK(f.object->signaled()&&f.kernel.now_ns()==0&&f.kernel.handles().OpenHandleCount()==handles);
    CHECK(f.module->last_pica_result().writes==2&&f.module->last_pica_result().irqs==1);
    CHECK(f.memory.StoreExclusive(kSource,4,0xABCDEF01,source_token,&fault)==a32::ExclusiveStoreResult::Success);
    CHECK(f.memory.StoreExclusive(kShared+f.queue(),4,1,header_token,&fault)==a32::ExclusiveStoreResult::ReservationLost);
    auto done=f.Page();f.Put();CHECK(f.Call().kind==a32::ExitKind::Fallthrough&&f.Page()==done); // Empty is idempotent.
    CHECK(f.kernel.WaitSynchronization1(f.event,0).result==0&&!f.object->signaled());
    CHECK(f.kernel.WaitSynchronization1(f.event,0).result==kResultTimeout);
}
void OwnershipAndRelay(){
    Fixture f(true);f.List({7,0xF0080,0x12345678,0xF0010});
    CHECK(f.kernel.CloseHandle(f.event)==0); // Strong service ownership, no fake event handle.
    f.session=f.spare; // Nonowner calls; the owner slot 1 remains selected.
    f.Put();CHECK(f.Call().kind==a32::ExitKind::Fallthrough);
    CHECK(f.object->signaled()&&f.Page()[0x41]==1&&f.Page()[0x4C]==5&&f.Word(f.queue())==1);
    Fixture ring;ring.Word(0,51);ring.List({0x12345678,0xF0010});ring.Put();CHECK(ring.Call().kind==a32::ExitKind::Fallthrough);
    CHECK(ring.Page()[63]==5&&ring.Page()[1]==1&&ring.Page()[0]==51);
    ring.Word(ring.queue(),0x100);ring.Put();CHECK(ring.Call().kind==a32::ExitKind::Fallthrough);CHECK(ring.Page()[12]==5&&ring.Page()[1]==2);
    // A full handle table still needs no new handles to signal or submit.
    Fixture full;full.List({1,0xF0080,0x12345678,0xF0010});
    for(;;){Handle h=0;auto r=full.kernel.DuplicateHandle(&h,full.event);if(r){CHECK(r==kResultOutOfHandles);break;}}
    full.Put();CHECK(full.Call().kind==a32::ExitKind::Fallthrough&&full.object->signaled());
}
void FailureAtomicity(){
    Fixture f;
    for(auto words:std::vector<std::vector<std::uint32_t>>{{7,0xF0080,0,0x000F022E},{7,0xF0080,0,0x0FFF0080}}){unsigned i=0;for(auto v:words)CHECK(f.memory.Write32(kSource+4*i++,v));f.Packet(0);f.Word(f.queue(),0x100);f.Stop();}
    f.List({0x12345678,0xF0010});
    for(auto fields:std::vector<std::array<std::uint32_t,2>>{{0,0xFFFFFFFF},{1,0x14000001},{2,7},{2,kPicaMaxListBytes+8},{1,0x13FFFFF8},{1,0x1C000000},{3,1},{7,1}}){
        f.Packet(0,kSource,8);f.Word(f.queue()+0x20+fields[0]*4,fields[1]);f.Stop();}
    // Entire batch atomicity: valid CacheFlush/Submit prefixes are not consumed.
    f.List({9,0xF0080,0x12345678,0xF0010});f.Packet(0,kSource,16,5);f.Packet(1);f.Packet(2,kSource,16,3);f.Word(f.queue(),0x300);f.Stop();
    // Exact source must be validated, not silently rounded through /8 registers.
    f.Packet(0,0x1BFFFFF8,16);f.Word(f.queue(),0x100);f.Stop();
    // Relay corruption/fullness prevents shader/register/queue commit and IRQ.
    for(auto header:{0x00003400U,0x00003500U,0x00000134U,0x00010000U}){f.List({0x12345678,0xF0010});f.Word(0,header);f.Stop();}
    Fixture unregistered(false,false);unregistered.List({0x12345678,0xF0010});unregistered.Stop();
    // No IRQ is synthesized when the list does not request one.
    Fixture passive;passive.List({0x777,0xF0080});passive.Put();CHECK(passive.Call().kind==a32::ExitKind::Fallthrough&&!passive.object->signaled()&&passive.Page()[1]==0);
}
void AliasesAndOutputGuards(){
    Fixture f;f.List({0x12345678,0xF0010});
    CHECK(f.memory.MapSharedServicePage(0x14010000,f.module->shared_memory(),RW));
    f.Packet(0,0x14010000,8);f.Stop();
    auto page=std::make_shared<ServiceSharedMemoryObject>();GuestMemory alias;
    CHECK(alias.MapSharedServicePage(f.cb()&~0xFFFU,page,RW));CHECK(alias.MapSharedServicePage(0x14020000,page,RW));
    f.Packet(0,0x14020000+(f.cb()&0xFFFU),8);f.Stop(&alias);
    // Protected reply preflight happens before PICA/queue/event mutation.
    GuestMemory ro;CHECK(ro.Map(f.cb(),0x100,MemoryPermission::Read));
    std::array<std::uint8_t,4> request{0,0,12,0};CHECK(ro.LoadBytes(f.cb(),request));
    f.Packet(0,kSource,8);f.Put();auto before=f.Page();CHECK(f.Call(&ro).kind==a32::ExitKind::Fallthrough&&f.cpu.r[0]==kResultInvalidPointer);
    CHECK(f.Page()==before&&!f.object->signaled());
}
void ProceduralUploadCommitAndBatchRollback(){
    Fixture f;f.List({0x2ff,0x000f00af,0x12345678,0x000000b0,0x87654321,0x000f00b7,0x12345678,0x000f0010});
    f.Put();CHECK(f.Call().kind==a32::ExitKind::Fallthrough);
    const auto& u=f.module->pica_uploads().procedural;
    CHECK(u.color_map.words[127]==0x12345678 && u.color_map.words[0]==0x87654321);
    CHECK(u.color_map.written.count()==2 && u.noise.written.none());
    CHECK(f.module->last_pica_result().procedural_words==2 && f.Word(f.queue())==1 && f.object->signaled());
    CHECK(*f.module->register_word(0x4012bc)==0x201);
    CHECK(f.kernel.WaitSynchronization1(f.event,0).result==0);
    // An unsupported later packet must not commit a valid lookup prefix or its IRQ.
    f.List({0x400,0x000f00af,0xdecafbad,0x000f00b0,0x12345678,0x000f0010});
    f.Packet(1,kSource,24,0x7f);f.Word(f.queue(),0x200);f.Stop();
    CHECK(f.module->pica_uploads().procedural.color.written.none());
    CHECK(f.module->pica_uploads().procedural.color_map.words[127]==0x12345678);
}
void SignalRetainedObjectWakesActualWaiter(){
    for(auto reset:{0U,1U,2U}){Kernel k;Handle h=0;CHECK(k.CreateEvent(&h,reset)==0);auto e=std::dynamic_pointer_cast<EventObject>(k.handles().Get(h));
        CHECK(k.WaitSynchronization1(h,-1).blocked);CHECK(k.CloseHandle(h)==0);k.SignalEventObject(*e);
        CHECK(k.current_thread()->status==ThreadStatus::Ready&&k.current_thread()->pending_wake&&k.current_thread()->wait_result==0);
        CHECK(k.now_ns()==0);CHECK(e->signaled()==(reset==1));}
}
} // namespace
int main(){ExecuteAndSignal();OwnershipAndRelay();FailureAtomicity();AliasesAndOutputGuards();ProceduralUploadCommitAndBatchRollback();SignalRetainedObjectWakesActualWaiter();
 if(failures)return EXIT_FAILURE;std::cout<<"PASS: actual staged submit, P3D relay/event, ownership, wait, aliases and atomic failure\n";return EXIT_SUCCESS;}
