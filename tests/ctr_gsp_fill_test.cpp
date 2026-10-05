#include "services/gsp_gpu_service.h"
#include "runtime/ctr_svc_bridge.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace lego::ctr;
namespace {
int failures=0;
#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}}while(0)
constexpr auto RW=MemoryPermission::Read|MemoryPermission::Write;
constexpr std::uint32_t page=0x10000000,output=0x14000000,base=kGpuVramVirtualBase;
using Packet=std::array<std::uint32_t,8>;
Packet Fill(std::uint32_t value=0x78563412,std::uint32_t start=base+0x70800,std::uint32_t size=0xC8000) {
    return {0x01000102,start,value,start+size,0,0,0xC8000,0x02010201};
}
struct Fixture {
    Kernel kernel;GuestMemory memory;IpcRouter ipc;SvcBridge bridge{kernel,&ipc};
    std::shared_ptr<GspGpuService> module=std::make_shared<GspGpuService>();
    std::shared_ptr<GpuVramBank> bank;Handle session{},event{},other{};
    std::shared_ptr<EventObject> object;unsigned slot{};a32::GuestState cpu{};
    Fixture(bool configure=true,bool second=false,bool reg=true):slot(second?1:0) {
        CHECK(memory.EnsureTlsMappings(kernel));
        if(configure){bank=GpuVramBank::ReferenceZero();CHECK(module->ConfigureVram(bank));std::vector<std::uint8_t> data(kGpuVramBytes,0xA5);CHECK(bank->Write(0,data));}
        CHECK(ipc.RegisterService("gsp::Gpu",module)==0);
        if(second)CHECK(ipc.ConnectToService(kernel,"gsp::Gpu",&other)==0);
        CHECK(ipc.ConnectToService(kernel,"gsp::Gpu",&session)==0);CHECK(kernel.CreateEvent(&event,0)==0);
        object=std::dynamic_pointer_cast<EventObject>(kernel.handles().Get(event));
        if(reg){Put({0x00130042,1,0,event});CHECK(Call().kind==a32::ExitKind::Fallthrough);}
        Put({0x00160042,0,0,kCurrentProcessPseudoHandle});CHECK(Call().kind==a32::ExitKind::Fallthrough);
        CHECK(memory.MapSharedServicePage(page,module->shared_memory(),RW));CHECK(memory.Map(output,0x4000,RW));
        Set(0,Fill());Word(queue(),0x100);
    }
    std::uint32_t cb()const{return kernel.current_thread()->tls_address+kIpcCommandBufferOffset;}
    unsigned queue()const{return 0x800+slot*0x200;}
    void Put(const IpcCommandBuffer& q={0xC0000}) {for(unsigned i=0;i<q.size();++i)CHECK(memory.Write32(cb()+4*i,q[i]));for(unsigned i=0;i<16;++i)cpu.r[i]=0xFEED0000+i;cpu.r[0]=session;cpu.r[15]=0x25947C;cpu.cpsr=0x60000010;}
    a32::ExecutionResult Call(GuestMemory* m=nullptr){return bridge.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcSendSyncRequest},cpu,m?m:&memory);}
    void Word(unsigned at,std::uint32_t value){CHECK(memory.Write32(page+at,value));}
    std::uint32_t Word(unsigned at){std::uint32_t value=0;CHECK(memory.Read32(page+at,&value));return value;}
    void Set(unsigned index,const Packet& packet){for(unsigned i=0;i<8;++i)Word(queue()+0x20+index*32+i*4,packet[i]);}
    std::vector<std::uint8_t> Page()const{auto b=module->shared_memory()->bytes();return {b.begin(),b.end()};}
    std::vector<std::uint8_t> Vram()const{if(!bank)return {};auto b=bank->bytes();return {b.begin(),b.end()};}
    PicaGpuRegisters Regs()const{PicaGpuRegisters out;for(unsigned i=0;i<out.size();++i)out[i]=*module->register_word(0x400000+i*4);return out;}
    void Success(){Put();CHECK(Call().kind==a32::ExitKind::Fallthrough&&cpu.r[0]==0&&cpu.r[15]==0x259480);std::uint32_t word=0;CHECK(memory.Read32(cb(),&word)&&word==0xC0040);CHECK(memory.Read32(cb()+4,&word)&&word==0);}
    void Stop(){Put();const auto old_page=Page(),old_vram=Vram();const auto old_regs=Regs();const auto uploads=module->pica_uploads();const auto old_cpu=cpu;const auto time=kernel.now_ns();const bool signaled=object->signaled();const auto handles=kernel.handles().OpenHandleCount();
        CHECK(Call().kind==a32::ExitKind::Svc&&ipc.unsupported_request());CHECK(Page()==old_page&&Vram()==old_vram&&Regs()==old_regs&&module->pica_uploads()==uploads);CHECK(cpu.r==old_cpu.r&&cpu.cpsr==old_cpu.cpsr);CHECK(kernel.now_ns()==time&&object->signaled()==signaled&&kernel.handles().OpenHandleCount()==handles);
    }
};
void ActualBytesAndOneIrq() {
    Fixture f;const auto regs=f.Regs();auto expected=f.Page();const auto uploads=f.module->pica_uploads();const auto handles=f.kernel.handles().OpenHandleCount();
    std::uint64_t value=0,header_token=0,packet_token=0;std::uint32_t fault=0;
    CHECK(f.memory.LoadExclusive(page+f.queue(),4,&value,&header_token,&fault));CHECK(f.memory.LoadExclusive(page+f.queue()+0x20,4,&value,&packet_token,&fault));
    f.Success();expected[f.queue()]=1;expected[f.queue()+1]=0;expected[1]=1;expected[12]=0;CHECK(f.Page()==expected);
    auto bytes=f.bank->bytes();for(unsigned i=0;i<bytes.size();++i) {
        const auto want=i>=0x70800&&i<0x138800 ? static_cast<std::uint8_t>(0x78563412U>>(8*((i-0x70800)%4))) : 0xA5;
        if(bytes[i]!=want){CHECK(false);break;}
    }
    auto after=f.Regs();CHECK(after[4]==0x18070800/8&&after[5]==0x18138800/8&&after[6]==0x78563412&&after[7]==0x202);
    for(unsigned i=0;i<regs.size();++i)if(i<4||i>7)CHECK(after[i]==regs[i]);
    CHECK(f.object->signaled()&&f.kernel.now_ns()==0&&f.kernel.handles().OpenHandleCount()==handles&&f.module->pica_uploads()==uploads);
    CHECK(!f.memory.IsMapped(base));CHECK(f.memory.StoreExclusive(page+f.queue(),4,0,header_token,&fault)==a32::ExclusiveStoreResult::ReservationLost);
    CHECK(f.memory.StoreExclusive(page+f.queue()+0x20,4,Fill()[0],packet_token,&fault)==a32::ExclusiveStoreResult::Success);
    CHECK(f.kernel.WaitSynchronization1(f.event,0).result==0&&!f.object->signaled());auto unchanged=f.Page();f.Success();CHECK(f.Page()==unchanged&&!f.object->signaled());
}
void DualAndDisabled() {
    Fixture f;auto p=Fill(0x11223344,base+0x100,32);p[4]=base+0x110;p[5]=0x88776655;p[6]=base+0x130;f.Set(0,p);f.Success();
    CHECK(f.Page()[1]==1&&f.Page()[12]==0);auto data=f.bank->bytes();
    for(unsigned i=0x100;i<0x130;++i)CHECK(data[i]==static_cast<std::uint8_t>((i<0x110?0x11223344U:0x88776655U)>>(8*(i%4))));
    CHECK(f.Regs()[7]==0x202&&f.Regs()[11]==0x202);
    Fixture second; p=Fill();p[1]=0;p[3]=~0U;p[4]=base+0x200;p[5]=0xAA998877;p[6]=base+0x210;second.Set(0,p);second.Success();CHECK(second.Page()[1]==1&&second.Page()[12]==1);
    Fixture disabled(false,false,false);p=Fill();p[7]=0x200;disabled.Set(0,p);disabled.Success();CHECK(disabled.Page()[1]==0&&!disabled.object->signaled()&&disabled.Regs()[7]==0x200);
    Fixture both_off; p=Fill(1,base+0x100,8);p[4]=base+0x200;p[6]=base+0x208;p[7]=0x02000201;both_off.Set(0,p);both_off.Success();CHECK(both_off.Page()[1]==0&&both_off.Regs()[7]==0x202&&both_off.Regs()[11]==0x200);
}
void AtomicGuardsAndDependencies() {
    Fixture absent(false);absent.Stop();Fixture unregistered(true,false,false);unregistered.Stop();Fixture f;
    auto p=Fill();p[4]=base+0x200;p[6]=base+0x1F8;f.Set(0,p);f.Stop();
    f.Set(0,Fill());p={4,0,0,0,0,0,0,0};f.Set(1,p);f.Word(f.queue(),0x200);f.Stop();
    f.Word(f.queue(),0x100);for(auto header:{0x00003400U,0x00010000U,0x34U}){f.Word(0,header);f.Stop();}f.Word(0,0);
    // A display source overlapping a staged fill must not use the old VRAM snapshot.
    f.Set(0,Fill(0x11223344,base+0x100,128));f.Set(1,{3,base+0x100,output,0x80008,0x80008,0x4400,0,0});f.Word(f.queue(),0x200);f.Stop();
    // In separate calls the display transfer reads the already committed fill.
    f.Word(f.queue(),0x100);f.Success();f.Word(f.queue(),0x101);f.Success();CHECK(f.Page()[1]==2&&f.Page()[12]==0&&f.Page()[13]==4);
    std::uint8_t byte=0;for(unsigned i=0;i<128;++i){CHECK(f.memory.Read8(output+i,&byte));CHECK(byte==static_cast<std::uint8_t>(0x11223344U>>(8*(i%4))));}
    Fixture ro;ro.Put();GuestMemory protected_memory;CHECK(protected_memory.Map(ro.cb(),0x100,MemoryPermission::Read));const std::array<std::uint8_t,4> request{0,0,12,0};CHECK(protected_memory.LoadBytes(ro.cb(),request));auto before=ro.Vram();CHECK(ro.Call(&protected_memory).kind==a32::ExitKind::Fallthrough&&ro.cpu.r[0]==kResultInvalidPointer&&ro.Vram()==before);
    Fixture alias;alias.kernel.current_thread()->tls_address=page;alias.Stop();
}
void RingOwnershipWaitersAndMixed() {
    Fixture f(true,true);f.session=f.other;CHECK(f.kernel.CloseHandle(f.event)==0);auto p=Fill(0x12345678,base+0x100,8);p[0]|=0x10000;f.Set(14,p);f.Word(f.queue(),0x10E);f.Success();CHECK(f.Word(f.queue())==0x10000&&f.Page()[0x41]==1&&f.Page()[0x4C]==0&&f.object->signaled());
    Fixture wrap;for(unsigned n=0;n<15;++n)wrap.Set((14+n)%15,Fill(n+1,base+8*n,8));wrap.Word(wrap.queue(),0xF0E);wrap.Word(0,51);wrap.Success();CHECK(wrap.Word(wrap.queue())==14&&wrap.Page()[1]==15&&wrap.Page()[63]==0);
    for(unsigned i=0;i<15;++i)CHECK(wrap.bank->bytes()[8*i]==i+1);
    Fixture waiter;CHECK(waiter.kernel.WaitSynchronization1(waiter.event,-1).blocked);waiter.Success();CHECK(waiter.kernel.current_thread()->status==ThreadStatus::Ready&&waiter.kernel.current_thread()->pending_wake&&!waiter.object->signaled());CHECK(waiter.bank->bytes()[0x70800]==0x12);
    Fixture full;for(;;){Handle h;const auto r=full.kernel.DuplicateHandle(&h,full.event);if(r){CHECK(r==kResultOutOfHandles);break;}}full.Success();CHECK(full.object->signaled());
    for(bool fill_first:{false,true}){Fixture mixed;CHECK(mixed.memory.Write32(output,0x12345678)&&mixed.memory.Write32(output+4,0xF0010));const unsigned fi=fill_first?0:1; mixed.Set(fi,Fill());mixed.Set(1-fi,{1,output,8,0,0,0,0,0});mixed.Word(mixed.queue(),0x200);mixed.Success();CHECK(mixed.Page()[1]==2&&mixed.Page()[12]==(fill_first?0:5)&&mixed.Page()[13]==(fill_first?5:0));CHECK(mixed.Regs()[7]==0x202&&mixed.Regs()[0x410]==0x12345678);}
}
}
int main(){ActualBytesAndOneIrq();DualAndDisabled();AtomicGuardsAndDependencies();RingOwnershipWaitersAndMixed();if(failures)return EXIT_FAILURE;std::cout<<"PASS: real VRAM fill bytes, ordered PSC ownership, atomic queue and dependency stops\n";}
