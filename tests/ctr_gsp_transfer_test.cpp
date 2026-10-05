#include "services/gsp_gpu_service.h"
#include "runtime/ctr_svc_bridge.h"
#include "runtime/ctr_runner.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace lego::ctr;
namespace {
int failures=0;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}}while(0)
constexpr auto RW=MemoryPermission::Read|MemoryPermission::Write;
constexpr std::uint32_t kPage=0x10000000,kOutput=0x14000000,kSrc=0x1F5FFF80,kSizeWord=0x00080008;
struct Fixture {
    Kernel kernel;GuestMemory memory;IpcRouter ipc;SvcBridge bridge{kernel,&ipc};
    std::shared_ptr<GspGpuService> module=std::make_shared<GspGpuService>();
    std::shared_ptr<GpuVramBank> bank;Handle session{},event{},other{};std::shared_ptr<EventObject> object;unsigned slot{};a32::GuestState cpu{};
    explicit Fixture(bool vram=true,bool second=false,bool registered=true):slot(second?1:0){
        CHECK(memory.EnsureTlsMappings(kernel));if(vram){bank=GpuVramBank::ReferenceZero();CHECK(module->ConfigureVram(bank));}
        CHECK(ipc.RegisterService("gsp::Gpu",module)==0);if(second)CHECK(ipc.ConnectToService(kernel,"gsp::Gpu",&other)==0);
        CHECK(ipc.ConnectToService(kernel,"gsp::Gpu",&session)==0);CHECK(kernel.CreateEvent(&event,0)==0);
        object=std::dynamic_pointer_cast<EventObject>(kernel.handles().Get(event));
        if(registered){Put({0x00130042,1,0,event});CHECK(Call().kind==a32::ExitKind::Fallthrough);}
        Put({0x00160042,0,0,kCurrentProcessPseudoHandle});CHECK(Call().kind==a32::ExitKind::Fallthrough);
        CHECK(memory.MapSharedServicePage(kPage,module->shared_memory(),RW));CHECK(memory.Map(kOutput,0x4000,RW));
        // Nonzero synthetic source prevents a bogus "zero destination" implementation from passing.
        if(bank){std::vector<std::uint8_t> source(128);for(unsigned i=0;i<128;++i)source[i]=static_cast<std::uint8_t>(3*i+1);CHECK(bank->Write(kSrc-kGpuVramVirtualBase,source));}
        std::vector<std::uint8_t> sentinel(0x4000,0xA5);CHECK(memory.LoadBytes(kOutput,sentinel));
        Packet(0);Word(queue(),0x100);
    }
    std::uint32_t cb()const{return kernel.current_thread()->tls_address+kIpcCommandBufferOffset;}
    std::uint32_t queue()const{return 0x800+slot*0x200;}
    void Put(const IpcCommandBuffer& q={0xC0000}) {for(unsigned i=0;i<q.size();++i)CHECK(memory.Write32(cb()+4*i,q[i]));for(unsigned i=0;i<16;++i)cpu.r[i]=0xBAA00000+i;cpu.r[0]=session;cpu.r[15]=0x25947C;cpu.cpsr=0x60000010;cpu.fpscr=0x23000010;}
    a32::ExecutionResult Call(GuestMemory* m=nullptr){return bridge.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcSendSyncRequest},cpu,m?m:&memory);}
    void Word(std::uint32_t p,std::uint32_t v){CHECK(memory.Write32(kPage+p,v));}
    std::uint32_t Word(std::uint32_t p){std::uint32_t v=0;CHECK(memory.Read32(kPage+p,&v));return v;}
    void Packet(unsigned index,std::uint32_t out=kOutput,std::uint32_t kind=0x01000103){auto p=queue()+0x20+index*0x20;unsigned i=0;for(auto word:{kind,kSrc,out,kSizeWord,kSizeWord,0x4400U,0U,0U})Word(p+4*i++,word);}
    std::vector<std::uint8_t> Bytes(std::uint32_t addr=kOutput,unsigned size=0x4000){std::vector<std::uint8_t> out(size);for(unsigned i=0;i<size;++i)CHECK(memory.Read8(addr+i,&out[i]));return out;}
    std::vector<std::uint8_t> Page(){auto p=module->shared_memory()->bytes();return {p.begin(),p.end()};}
    std::vector<std::uint32_t> Regs(){std::vector<std::uint32_t> out;for(unsigned i=0;i<kPicaGpuWords;++i)out.push_back(*module->register_word(0x400000+4*i));return out;}
    void Stop(){Put();auto page=Page(),dest=Bytes();auto regs=Regs();auto uploads=module->pica_uploads();auto cpu_before=cpu;auto signaled=object->signaled();auto count=kernel.handles().OpenHandleCount();
        CHECK(Call().kind==a32::ExitKind::Svc&&ipc.unsupported_request());CHECK(Page()==page&&Bytes()==dest&&Regs()==regs&&module->pica_uploads()==uploads&&cpu.r==cpu_before.r);CHECK(object->signaled()==signaled&&kernel.now_ns()==0&&kernel.handles().OpenHandleCount()==count);}
};
std::vector<std::uint8_t> Expected(const GpuVramBank& bank){
    std::vector<std::uint8_t> out(128);auto b=bank.bytes().subspan(kSrc-kGpuVramVirtualBase,128);
    for(unsigned i=0;i<64;++i){auto x=(i&1)|((i>>1)&2)|((i>>2)&4);auto y=((i>>1)&1)|((i>>2)&2)|((i>>3)&4);out[2*(y*8+x)]=b[2*i];out[2*(y*8+x)+1]=b[2*i+1];}return out;
}
void BytesThenPpfAndReservations(){
    Fixture f;auto expected=Expected(*f.bank);auto before_page=f.Page(),before_dest=f.Bytes();auto uploads=f.module->pica_uploads();f.Put();auto cpu=f.cpu;auto handles=f.kernel.handles().OpenHandleCount();
    std::uint64_t value=0,header_token=0,dest_token=0,other_token=0;std::uint32_t fault=0;
    CHECK(f.memory.LoadExclusive(kOutput,4,&value,&dest_token,&fault));CHECK(f.memory.LoadExclusive(kOutput+128,4,&value,&other_token,&fault));CHECK(f.memory.LoadExclusive(kPage+f.queue(),4,&value,&header_token,&fault));
    CHECK(f.Call().kind==a32::ExitKind::Fallthrough&&f.cpu.r[0]==0&&f.cpu.r[15]==0x259480);
    for(unsigned i=1;i<15;++i)CHECK(cpu.r[i]==f.cpu.r[i]);CHECK(cpu.cpsr==f.cpu.cpsr&&cpu.fpscr==f.cpu.fpscr);
    CHECK(f.Bytes(kOutput,128)==expected);CHECK(std::equal(before_dest.begin()+128,before_dest.end(),f.Bytes().begin()+128));
    before_page[f.queue()]=1;before_page[f.queue()+1]=0;before_page[1]=1;before_page[12]=4;CHECK(f.Page()==before_page);
    CHECK(f.module->pica_uploads()==uploads&&f.object->signaled()&&f.kernel.now_ns()==0&&f.kernel.handles().OpenHandleCount()==handles);
    CHECK(*f.module->register_word(0x400C00)==0x030BFFF0U);CHECK(*f.module->register_word(0x400C04)==0x04000000U);
    CHECK(*f.module->register_word(0x400C08)==kSizeWord&&*f.module->register_word(0x400C0C)==kSizeWord);
    CHECK(*f.module->register_word(0x400C10)==0x4400&&*f.module->register_word(0x400C18)==0);
    CHECK(f.memory.StoreExclusive(kOutput,4,0,dest_token,&fault)==a32::ExclusiveStoreResult::ReservationLost);
    CHECK(f.memory.StoreExclusive(kPage+f.queue(),4,0,header_token,&fault)==a32::ExclusiveStoreResult::ReservationLost);
    CHECK(f.memory.StoreExclusive(kOutput+128,4,0xA5A5A5A5,other_token,&fault)==a32::ExclusiveStoreResult::Success);
    CHECK(!f.memory.IsMapped(kSrc,128)); // Physical device memory did not grant an invented CPU mapping.
    CHECK(f.kernel.WaitSynchronization1(f.event,0).result==0&&!f.object->signaled());
    auto page=f.Page();f.Put();CHECK(f.Call().kind==a32::ExitKind::Fallthrough&&f.Page()==page&&!f.object->signaled());
}
void GuardsAndAtomicity(){
    Fixture absent(false);absent.Stop();Fixture unregistered(true,false,false);unregistered.Stop();
    Fixture f;
    for(auto field:std::vector<std::array<std::uint32_t,2>>{{0,4},{1,0x18000000},{1,kSrc+8},{2,kOutput+1},{3,0},{4,0x80010},{5,0x4401},{5,0x01004400}}){f.Packet(0);f.Word(f.queue()+0x20+4*field[0],field[1]);f.Stop();}
    for(auto out:{0x14004000U,0x13FFFFF8U,0x1BFFFF88U,0xFFFFFFF8U}){f.Packet(0,out);f.Stop();}
    CHECK(f.memory.Map(0x15000000,128,MemoryPermission::Read));f.Packet(0,0x15000000);f.Stop();
    CHECK(f.memory.Map(0x15001000,64,RW)&&f.memory.Map(0x15001040,64,RW));f.Packet(0,0x15001000);f.Stop();
    // Valid transfer followed by unsupported packet: output, queue, GPU and IRQ unchanged.
    f.Packet(0);f.Packet(1,kOutput,4);f.Word(f.queue(),0x200);f.Stop();
    // Valid PICA prefix must not commit if a later transfer is unsupported.
    CHECK(f.memory.Write32(kOutput+0x1000,0x12345678)&&f.memory.Write32(kOutput+0x1004,0xF0010));
    f.Word(f.queue()+0x20,1);f.Word(f.queue()+0x24,kOutput+0x1000);f.Word(f.queue()+0x28,8);
    for(unsigned i=3;i<8;++i)f.Word(f.queue()+0x20+i*4,0);f.Packet(1);f.Word(f.queue()+0x40+20,0x4401);f.Stop();
    // A following command list that depends on the staged output remains unsupported.
    f.Packet(0);f.Word(f.queue()+0x40,1);f.Word(f.queue()+0x44,kOutput);f.Word(f.queue()+0x48,8);for(unsigned i=3;i<8;++i)f.Word(f.queue()+0x40+i*4,0);f.Stop();
    for(auto relay:{0x00003400U,0x00010000U,0x00000034U}){f.Packet(0);f.Word(f.queue(),0x100);f.Word(0,relay);f.Stop();}
}
void RingStopAndLifetime(){
    Fixture f(true,true);CHECK(f.kernel.CloseHandle(f.event)==0);f.session=f.other;
    f.Word(f.queue(),0x100|14);f.Packet(14,kOutput,0x01010103);f.Put();CHECK(f.Call().kind==a32::ExitKind::Fallthrough);
    CHECK(f.Word(f.queue())==0x10000&&f.Page()[0x41]==1&&f.Page()[0x4C]==4&&f.object->signaled());
    auto before=f.Bytes();f.Packet(0);f.Word(f.queue(),0x01000100);f.Put();CHECK(f.Call().kind==a32::ExitKind::Fallthrough&&f.Word(f.queue())==0x01010100&&f.Bytes()==before);
    Fixture multi;multi.Packet(0,kOutput);multi.Packet(1,kOutput+128);multi.Word(multi.queue(),0x200);multi.Word(0,51);multi.Put();
    CHECK(multi.Call().kind==a32::ExitKind::Fallthrough&&multi.Word(multi.queue())==2);CHECK(multi.Page()[63]==4&&multi.Page()[12]==4&&multi.Page()[1]==2);CHECK(multi.Bytes(kOutput,128)==multi.Bytes(kOutput+128,128));
    Fixture full;for(;;){Handle h;auto result=full.kernel.DuplicateHandle(&h,full.event);if(result){CHECK(result==kResultOutOfHandles);break;}}
    full.Put();CHECK(full.Call().kind==a32::ExitKind::Fallthrough&&full.object->signaled());
    Fixture waiter;CHECK(waiter.kernel.WaitSynchronization1(waiter.event,-1).blocked);CHECK(waiter.kernel.CloseHandle(waiter.event)==0);waiter.Put();CHECK(waiter.Call().kind==a32::ExitKind::Fallthrough);
    CHECK(waiter.kernel.current_thread()->status==ThreadStatus::Ready&&waiter.kernel.current_thread()->pending_wake&&waiter.kernel.current_thread()->wait_result==0&&!waiter.object->signaled());CHECK(waiter.Bytes(kOutput,128)==Expected(*waiter.bank));
}
void MixedSubmitTransferBatches(){
    for (bool transfer_first:{false,true}) {
        Fixture f;const auto list=kOutput+0x2000;
        CHECK(f.memory.Write32(list,0x12345678)&&f.memory.Write32(list+4,0x000F0010));
        const unsigned submit_index=transfer_first?1:0,transfer_index=1-submit_index;
        f.Packet(transfer_index);const auto packet=f.queue()+0x20+submit_index*0x20;
        f.Word(packet,1);f.Word(packet+4,list);f.Word(packet+8,8);
        for (unsigned n=3;n<8;++n)f.Word(packet+4*n,0);
        f.Word(f.queue(),0x200);f.Put();CHECK(f.Call().kind==a32::ExitKind::Fallthrough);
        CHECK(f.Word(f.queue())==2&&f.Page()[1]==2);
        CHECK(f.Page()[12]==(transfer_first?4:5)&&f.Page()[13]==(transfer_first?5:4));
        CHECK(f.Bytes(kOutput,128)==Expected(*f.bank));
        CHECK(*f.module->register_word(0x400C10)==0x4400&&*f.module->register_word(0x4018F0)==0);
        CHECK(f.module->last_pica_result().irqs==1);
    }
    Fixture ring;for(unsigned n=0;n<15;++n)ring.Packet(n,kOutput+128*n);
    ring.Word(ring.queue(),0xF0E);ring.Put();CHECK(ring.Call().kind==a32::ExitKind::Fallthrough);
    CHECK(ring.Word(ring.queue())==14&&ring.Page()[1]==15);
    for(unsigned n=0;n<15;++n){CHECK(ring.Page()[12+n]==4);CHECK(ring.Bytes(kOutput+128*n,128)==Expected(*ring.bank));}
}
void ResponseAndSharedAliases(){
    Fixture f;CHECK(f.memory.MapSharedServicePage(0x14010000,f.module->shared_memory(),RW));f.Packet(0,0x14010000);f.Stop();
    auto other=std::make_shared<ServiceSharedMemoryObject>();CHECK(f.memory.MapSharedServicePage(0x14020000,other,RW));f.Packet(0,0x14020000);f.Stop();
    f.Packet(0);f.Put();GuestMemory ro;CHECK(ro.Map(f.cb(),0x100,MemoryPermission::Read));const std::array<std::uint8_t,4> q{0,0,12,0};CHECK(ro.LoadBytes(f.cb(),q));auto page=f.Page();
    CHECK(f.Call(&ro).kind==a32::ExitKind::Fallthrough&&f.cpu.r[0]==kResultInvalidPointer&&f.Page()==page&&!f.object->signaled());
    // Relocate the response into the same destination region: reply would corrupt pixels.
    f.kernel.current_thread()->tls_address=kOutput;f.Packet(0,kOutput+128);f.Stop();
}
void ExplicitBootPolicy(){
    Kernel k;GuestMemory m;const a32::Registry registry{};NativeRunner strict(registry,m,k);Handle h;
    CHECK(strict.ipc().ConnectToService(k,"gsp::Gpu",&h)==0);auto session=std::dynamic_pointer_cast<ClientSessionObject>(k.handles().Get(h));auto gsp=std::dynamic_pointer_cast<GspGpuService>(session->service);CHECK(gsp&&!gsp->vram_bank());
    Kernel k2;GuestMemory m2;NativeRunner reference(registry,m2,k2,kDefaultRtcMsSince1900,{},PtmStepMode::Unconfigured,{},GpuVramMode::ReferenceZero);
    CHECK(reference.ipc().ConnectToService(k2,"gsp::Gpu",&h)==0);session=std::dynamic_pointer_cast<ClientSessionObject>(k2.handles().Get(h));gsp=std::dynamic_pointer_cast<GspGpuService>(session->service);CHECK(gsp&&gsp->vram_bank()&&!m2.IsMapped(kGpuVramVirtualBase));
    CHECK(!gsp->ConfigureVram(GpuVramBank::ReferenceZero()));
    auto module=std::make_shared<GspGpuService>();CHECK(!module->ConfigureVram(nullptr));std::shared_ptr<IpcService> endpoint;CHECK(module->CreateSessionHandler(&endpoint)==0);CHECK(!module->ConfigureVram(GpuVramBank::ReferenceZero()));
}
}
int main(){BytesThenPpfAndReservations();GuardsAndAtomicity();RingStopAndLifetime();MixedSubmitTransferBatches();ResponseAndSharedAliases();ExplicitBootPolicy();if(failures)return EXIT_FAILURE;std::cout<<"PASS: real DisplayTransfer bytes before PPF, private VRAM provenance, atomic queue and real event ownership\n";return EXIT_SUCCESS;}
