#include "dsp_live_fixture.h"
#include "runtime/ctr_svc_bridge.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <new>
static int fail_after=-1;
void* operator new(std::size_t n){if(fail_after==0){fail_after=-1;throw std::bad_alloc();}if(fail_after>0)--fail_after;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept{std::free(p);}void operator delete(void* p,std::size_t) noexcept{std::free(p);}
using namespace lego::ctr;
namespace {
int failures=0;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}}while(0)
// Real synthetic DSP opcodes: boot, read incoming semaphore into r0, store to DATA
// word0x100; clear those bits; store the now-empty semaphore to DATA word0x101.
std::vector<std::uint8_t> Container(bool fault=false) {
    auto b=dsp_live_fixture::Container(fault);
    if(fault)return b;
    const std::vector<std::uint16_t> words{0x2101,0xD4BC,0x80C0,0xD4BC,0x80C4,0xD4BC,0x80C8,
        0x2142,0xD4BC,0x80C8,0xD4B8,0x80D2,0xD4BC,0x0100,0xD4BC,0x80D0,0xD4B8,0x80D2,0xD4BC,0x0101,0x57F0};
    std::vector<std::uint8_t> prog;for(auto w:words){prog.push_back(w&255);prog.push_back(w>>8);}auto data=dsp_live_fixture::Table();
    b.resize(0x300+prog.size()+data.size());dsp_fixture::Put32(b,0x104,b.size());dsp_fixture::Put32(b,0x128,prog.size());
    dsp_fixture::Put32(b,0x150,0x300+prog.size());
    std::copy(prog.begin(),prog.end(),b.begin()+0x300);std::copy(data.begin(),data.end(),b.begin()+0x300+prog.size());
    dsp_fixture::Rehash(b,0);dsp_fixture::Rehash(b,1);return b;
}
struct Fixture {
    Kernel k;GuestMemory m;IpcRouter ipc;SvcBridge svc{k,&ipc};Handle h{};
    std::shared_ptr<DspDiscoveryService> endpoint=std::make_shared<DspDiscoveryService>(DspSpecialConfig{},DspProbeOptions{true,DspProbeReset::KnownOnly,100,true});
    Fixture(bool fault=false){CHECK(m.EnsureTlsMappings(k));CHECK(m.Map(0x08000000,0x1000,MemoryPermission::Read|MemoryPermission::Write));
        auto b=Container(fault);CHECK(m.LoadBytes(0x08000000,b));CHECK(ipc.RegisterService("dsp::DSP",endpoint)==0);CHECK(ipc.ConnectToService(k,"dsp::DSP",&h)==0);
        auto n=static_cast<std::uint32_t>(b.size());Put({0x001100C2,n,255,255,(n<<4)|10,0x08000000});CHECK(Call()==0);}
    std::uint32_t cb()const{return k.current_thread()->tls_address+kIpcCommandBufferOffset;}
    void Put(IpcCommandBuffer q){for(unsigned i=0;i<q.size();++i)CHECK(m.Write32(cb()+4*i,q[i]));}
    IpcCommandBuffer Read(){IpcCommandBuffer q{};for(unsigned i=0;i<q.size();++i)CHECK(m.Read32(cb()+4*i,&q[i]));return q;}
    std::optional<Result> Call(){return ipc.SendSyncRequest(k,m,h);}
    Handle Export(){Put({0x00160000});CHECK(Call()==0);auto q=Read();CHECK(q[0]==0x00160042&&q[1]==0&&q[2]==0);for(unsigned i=4;i<q.size();++i)CHECK(!q[i]);return q[3];}
    void Mask(std::uint32_t x){Put({0x00170040,x});CHECK(Call()==0);CHECK(Read()==IpcCommandBuffer({0x00170040}));}
    std::vector<std::uint8_t> Bank()const{auto b=endpoint->execution_probe()->memory();return {b.begin(),b.end()};}
    std::uint16_t Observed(){CHECK(endpoint->RunScheduled(k,*endpoint->next_deadline_ns()));std::uint16_t x=0,y=1;
        CHECK(m.Read16(0x1FF40200,&x)&&m.Read16(0x1FF40202,&y)&&y==0);return x;}
};
void SignalReachesFirmware(){
    for(auto value:{0U,1U,0x2000U,0x4000U,0x8000U,0xFFFFU,0xABCD1234U}){
        Fixture f;auto bank=f.Bank();auto summary=f.endpoint->execution_probe()->summary();auto now=f.k.now_ns();auto count=f.k.handles().OpenHandleCount();
        f.Mask(value);auto a=f.Export();auto b=f.Export();CHECK(a!=b&&f.k.handles().Get(a)==f.k.handles().Get(b));
        auto event=f.endpoint->semaphore_event();CHECK(event&&!event->signaled()&&event->reset_type()==ResetType::OneShot);
        CHECK(f.Bank()==bank&&f.endpoint->execution_probe()->summary()==summary&&f.k.now_ns()==now&&f.k.handles().OpenHandleCount()==count+2);
        Handle dup{};CHECK(f.k.DuplicateHandle(&dup,a)==0);CHECK(f.k.CloseHandle(a)==0&&f.k.CloseHandle(b)==0);
        CHECK(f.k.SignalEvent(dup)==0&&event->signaled());CHECK(f.k.ClearEvent(dup)==0&&!event->signaled());
        CHECK(f.endpoint->execution_probe()->summary()==summary&&f.Bank()==bank&&f.k.now_ns()==now);
        CHECK(f.Observed()==static_cast<std::uint16_t>(value));
    }
    // Mask changes alone must NOT produce a DSP semaphore or a notification.
    {Fixture f;f.Mask(0xFFFF);f.Export();CHECK(f.Observed()==0);}
    // Direct SetSemaphore and event-triggered preset both OR into the incoming
    // peripheral. The DSP itself reads/clears the actual combined bits.
    {Fixture f;auto h=f.Export();f.Mask(0x2000);f.Put({0x00070040,0xABCD4000});CHECK(f.Call()==0);
     CHECK(f.k.SignalEvent(h)==0&&f.k.SignalEvent(h)==0);CHECK(f.Observed()==0x6000);}
    // Changing the stored preset does not replace existing peripheral bits.
    {Fixture f;auto h=f.Export();f.Mask(1);CHECK(f.k.SignalEvent(h)==0);f.Mask(2);CHECK(f.k.SignalEvent(h)==0);CHECK(f.Observed()==3);}
}
void SignalViaSvcAndProtectedExport(){
    {Fixture f;auto h=f.Export();f.Mask(0x2000);a32::GuestState cpu{};
     for(unsigned i=0;i<16;++i)cpu.r[i]=0xAAAA0000+i;cpu.r[0]=h;cpu.r[15]=0x102000;auto before=cpu;
     auto ret=f.svc.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcSignalEvent},cpu,&f.m);
     CHECK(ret.kind==a32::ExitKind::Fallthrough&&cpu.r[0]==0&&cpu.r[15]==before.r[15]+4);
     for(unsigned i=1;i<15;++i)CHECK(cpu.r[i]==before.r[i]);CHECK(f.Observed()==0x2000);}
    for(unsigned mode=0;mode<3;++mode){Fixture f;IpcCommandBuffer q{0x00160000};f.Put(q);GuestMemory other;
     auto bytes=mode==1?4U:unsigned(sizeof(q));auto permission=mode==0?MemoryPermission::Read:mode==1?MemoryPermission::Read|MemoryPermission::Write:MemoryPermission::Write;
     CHECK(other.Map(f.cb(),bytes,permission));CHECK(other.LoadBytes(f.cb(),{reinterpret_cast<const std::uint8_t*>(q.data()),bytes}));
     auto count=f.k.handles().OpenHandleCount();CHECK(f.ipc.SendSyncRequest(f.k,other,f.h)==kResultInvalidPointer);
     CHECK(!f.endpoint->semaphore_event()&&f.k.handles().OpenHandleCount()==count);}
}
void IdentityAndLifetime(){
    Fixture f;Handle h=f.Export();auto obj=f.endpoint->semaphore_event();f.Mask(0x2000);
    CHECK(f.k.CloseHandle(f.h)==0);CHECK(f.ipc.ConnectToService(f.k,"dsp::DSP",&f.h)==0);auto h2=f.Export();CHECK(f.k.handles().Get(h2)==obj&&f.endpoint->preset_semaphore()==0x2000);
    Fixture independent;CHECK(independent.Export()!=0&&independent.endpoint->semaphore_event()!=obj);
    // Replace endpoint after closing session. Existing exported event is safe but
    // its weak live target expires; it must not silently deliver to another device.
    CHECK(f.k.CloseHandle(f.h)==0);CHECK(f.ipc.RegisterService("dsp::DSP",std::make_shared<DspDiscoveryService>())==0);f.endpoint.reset();
    CHECK(!f.k.SignalEvent(h)&&f.k.event_signal_error()&&obj->signaled());
    CHECK(!f.k.SignalEventObject(*obj));
}
void FailuresAndNoPartialExport(){
    for(int stage:{0,1}){Fixture f;f.Put({0x00160000});auto before=f.k.handles();fail_after=stage;auto result=f.Call();fail_after=-1;
        CHECK(!result&&!f.endpoint->semaphore_event()&&f.Read()==IpcCommandBuffer({0x00160000}));
        CHECK(f.k.handles().OpenHandleCount()==before.OpenHandleCount());auto dummy=std::make_shared<EventObject>(ResetType::OneShot);Handle a{},b{};
        CHECK(f.k.handles().Create(&a,dummy)==0&&before.Create(&b,dummy)==0&&a==b);f.Export();}
    {Fixture f;auto dummy=std::make_shared<EventObject>(ResetType::OneShot);Handle last{};
     while(f.k.handles().OpenHandleCount()<HandleTable::kMaxCount)CHECK(f.k.handles().Create(&last,dummy)==0);
     f.Put({0x00160000});auto before=f.k.handles();CHECK(f.Call()==kResultOutOfHandles&&!f.endpoint->semaphore_event());
     CHECK(f.Read()==IpcCommandBuffer({0x00160000}));CHECK(f.k.CloseHandle(last)==0&&before.Close(last)==0);
     auto event=f.Export();Handle want{};CHECK(before.Create(&want,dummy)==0&&event==want);}
    {Fixture f;f.Mask(0x2000);f.Put({0x00170040,0x4000});GuestMemory ro;CHECK(ro.Map(f.cb(),256,MemoryPermission::Read));
     auto q=f.Read();CHECK(ro.LoadBytes(f.cb(),{reinterpret_cast<const std::uint8_t*>(q.data()),sizeof(q)}));
     CHECK(f.ipc.SendSyncRequest(f.k,ro,f.h)==kResultInvalidPointer&&f.endpoint->preset_semaphore()==0x2000);}
    {Fixture f(true);auto h=f.Export();CHECK(!f.endpoint->RunScheduled(f.k,*f.endpoint->next_deadline_ns()));
     const auto old=f.endpoint->execution_probe()->summary();CHECK(!f.k.SignalEvent(h));f.Put({0x00070040,1});CHECK(!f.Call());
     CHECK(f.endpoint->execution_probe()->summary()==old&&f.Read()==IpcCommandBuffer({0x00070040,1}));}
    for(auto header:{0x00160040U,0x00160002U,0x00170000U,0x00170042U,0x00070000U,0x00070042U}){
     Fixture f;IpcCommandBuffer q{header,0xDEAD1234};f.Put(q);CHECK(!f.Call()&&f.Read()==q&&!f.endpoint->semaphore_event());}
}
void AudioPipeRequest(){
    Fixture f;const std::array<std::uint8_t,4> bytes{0x34,0x12,0xCD,0xEF};CHECK(f.m.LoadBytes(0x08000800,bytes));
    const auto before=f.Bank();const auto state=f.endpoint->execution_probe()->summary();auto now=f.k.now_ns();
    f.Put({0x000D0082,2,4,0x10402,0x08000800});CHECK(f.Call()==0&&f.Read()==IpcCommandBuffer({0x000D0040}));
    DspPipeDescriptor d{};CHECK(f.endpoint->live_device()->InspectPipe(5,d)==DspPipeResult::Complete&&d.used==4&&d.write_pointer==4&&d.read_pointer==0);
    auto bank=f.Bank();auto base=0x40000+d.address_words*2;CHECK(bank[base]==0x34&&bank[base+1]==0x12&&!bank[base+2]&&!bank[base+3]);
    for(unsigned i=0;i<4;++i){std::uint8_t v{};CHECK(f.m.Read8(0x08000800+i,&v)&&v==bytes[i]);}
    auto expected_state=state;expected_state.known_bytes+=4; // four previously unknown payload bytes now have host-write provenance
    CHECK(f.endpoint->execution_probe()->summary()==expected_state&&f.k.now_ns()==now);
    // Mailbox is now busy; a second request must preserve all SRAM and IPC.
    const IpcCommandBuffer q{0x000D0082,2,4,0x10402,0x08000800};f.Put(q);CHECK(!f.Call()&&f.Read()==q&&f.Bank()==bank);
    for(auto a:{0xFFFFFFFEU,0x09000000U}){f.Put({0x000D0082,2,4,0x10402,a});CHECK(f.Call()==kResultInvalidPointer&&f.Bank()==bank);}
    Fixture ro;ro.Put(q);GuestMemory protect;CHECK(protect.Map(ro.cb(),256,MemoryPermission::Read));CHECK(protect.LoadBytes(ro.cb(),{reinterpret_cast<const std::uint8_t*>(q.data()),sizeof(q)}));
    auto old=ro.Bank();CHECK(ro.ipc.SendSyncRequest(ro.k,protect,ro.h)==kResultInvalidPointer&&ro.Bank()==old);
    for(unsigned i:{0U,1U,2U,3U}){Fixture bad;auto wrong=q;wrong[i]^=1;bad.Put(wrong);auto previous=bad.Bank();CHECK(!bad.Call()&&bad.Read()==wrong&&bad.Bank()==previous);}
}
}
int main(){try{SignalReachesFirmware();SignalViaSvcAndProtectedExport();IdentityAndLifetime();FailuresAndNoPartialExport();AudioPipeRequest();}
 catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}if(failures)return 1;
 std::cout<<"PASS: retained semaphore event, actual DSP read/clear, mask/direct OR, lifetime/rollback and bounded audio-pipe IPC\n";}
