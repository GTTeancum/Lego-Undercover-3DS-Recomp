#include "dsp_live_fixture.h"
#include <cstdlib>
#include <iostream>
#include <new>
#include <vector>
static int fail_after=-1;
void* operator new(std::size_t n) {if(fail_after==0){fail_after=-1;throw std::bad_alloc();}if(fail_after>0)--fail_after;
    if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
using namespace lego::ctr;
namespace {
#define CHECK(x) do {if(!(x))throw std::runtime_error("line "+std::to_string(__LINE__)+": " #x);}while(0)
constexpr std::uint32_t Output=0x08000800, Data=DspLiveDevice::DataAddress;
struct Fixture {
    Kernel k;GuestMemory m;IpcRouter ipc;Handle handle{};
    std::shared_ptr<DspDiscoveryService> service=std::make_shared<DspDiscoveryService>(DspSpecialConfig{},DspProbeOptions{true,DspProbeReset::KnownOnly,100,true});
    Fixture(bool drain=false,bool fault=false) {
        CHECK(m.EnsureTlsMappings(k));CHECK(m.Map(0x08000000,0x2000,MemoryPermission::Read|MemoryPermission::Write));
        auto b=dsp_live_fixture::Container(fault);
        if(fault){dsp_live_fixture::Put16(b,0x316,0x6800);dsp_fixture::Rehash(b,0);}
        if(drain) {
            // Extend the synthetic post-boot program: consume the real incoming
            // command2 word, then write that word to outgoing mailbox0 and loop.
            const auto start=0x300U+20U;
            dsp_live_fixture::Put16(b,start,0xD4B8);dsp_live_fixture::Put16(b,start+2,0x80CA);
            dsp_live_fixture::Put16(b,start+4,0xD4BC);dsp_live_fixture::Put16(b,start+6,0x80C0);
            // Original fixture has only four post-boot words. Insert one word
            // before its DATA segment, then fix segment offsets/header and hashes.
            b.insert(b.begin()+start+8,2,0);dsp_live_fixture::Put16(b,start+8,0x57F0);
            auto n=static_cast<std::uint32_t>(b.size());dsp_fixture::Put32(b,0x104,n);
            dsp_fixture::Put32(b,0x128,30);dsp_fixture::Put32(b,0x150,0x31E);
            dsp_fixture::Rehash(b,0);dsp_fixture::Rehash(b,1);
        }
        CHECK(m.LoadBytes(0x08000000,b));
        CHECK(ipc.RegisterService("dsp::DSP",service)==0);CHECK(ipc.ConnectToService(k,"dsp::DSP",&handle)==0);
        auto n=static_cast<std::uint32_t>(b.size());Put({0x001100C2,n,255,255,(n<<4)|10,0x08000000});CHECK(Call()==0);
        for(unsigned s=0;s<16;++s){auto at=Data+0x84+s*10;
            CHECK(m.Write16(at,static_cast<std::uint16_t>(0x1000+s*128)));CHECK(m.Write16(at+2,128));}
        for(unsigned i=0;i<128;++i)CHECK(m.Write8(Payload()+i,static_cast<std::uint8_t>(i*7+19)));
        std::vector<std::uint8_t> sentinel(256,0xA5);CHECK(m.LoadBytes(Output-64,sentinel));
        SetRing(0,32);Request(2);
    }
    std::uint32_t cb() const {return k.current_thread()->tls_address+0x80;}
    std::uint32_t table() const {return k.current_thread()->tls_address+0x180;}
    std::uint32_t Payload() const {return Data+0x2400;}
    void SetRing(std::uint16_t read,std::uint16_t write){CHECK(m.Write16(Data+0x84+4*10+4,read));CHECK(m.Write16(Data+0x84+4*10+6,write));}
    void Put(const IpcCommandBuffer& q){for(unsigned i=0;i<q.size();++i)CHECK(m.Write32(cb()+4*i,q[i]));}
    IpcCommandBuffer Get(){IpcCommandBuffer q{};for(unsigned i=0;i<q.size();++i)CHECK(m.Read32(cb()+4*i,&q[i]));return q;}
    void Request(std::uint32_t size,std::uint32_t out=Output,std::uint32_t capacity=128){Put({0x001000C0,2,0,size});CHECK(m.Write32(table(),(capacity<<14)|2));CHECK(m.Write32(table()+4,out));}
    std::optional<Result> Call(){return ipc.SendSyncRequest(k,m,handle);}
    std::vector<std::uint8_t> Snapshot() const {const auto* p=service->execution_probe();std::vector<std::uint8_t>b(p->memory().begin(),p->memory().end());b.insert(b.end(),p->provenance().begin(),p->provenance().end());return b;}
    std::uint8_t Byte(std::uint32_t at){std::uint8_t v{};CHECK(m.Read8(at,&v));return v;}
};
void ActualPayloadAndWrap(){
    for(auto size:{1U,2U,3U,30U,32U,127U,128U}){
        Fixture f;f.SetRing(0,0x8000);f.Request(size);auto now=f.k.now_ns();auto handles=f.k.handles().OpenHandleCount();
        auto summary=f.service->execution_probe()->summary();CHECK(f.Call()==0);
        CHECK(f.Get()==IpcCommandBuffer({0x00100082,0,size,(size<<14)|2,Output}));
        for(unsigned i=0;i<size;++i)CHECK(f.Byte(Output+i)==static_cast<std::uint8_t>(i*7+19));
        CHECK(f.Byte(Output-1)==0xA5&&f.Byte(Output+size)==0xA5);
        DspPipeDescriptor d{};CHECK(f.service->live_device()->InspectPipe(4,d)==DspPipeResult::Complete);
        CHECK(d.read_pointer==(size==128?0x8000:size)&&d.write_pointer==0x8000&&d.used==128-size);
        CHECK(!f.service->execution_probe()->CanSend(2));CHECK(f.service->execution_probe()->summary()==summary);
        CHECK(f.k.now_ns()==now&&f.k.handles().OpenHandleCount()==handles&&f.service->live_device()->slices()==0);
    }
    {Fixture f;f.SetRing(126,0x8002);f.Request(4);CHECK(f.Call()==0);
        for(unsigned i=0;i<4;++i)CHECK(f.Byte(Output+i)==static_cast<std::uint8_t>(((126+i)%128)*7+19));
        DspPipeDescriptor d{};CHECK(f.service->live_device()->InspectPipe(4,d)==DspPipeResult::Complete&&d.read_pointer==0x8002&&d.used==0);}
    {Fixture f;f.Request(0xABCD0002);CHECK(f.Call()==0&&f.Get()[2]==2);}
    // Nonzero byte patterns include every possible byte; no fixed startup value.
    {Fixture f;for(unsigned i=0;i<128;++i)CHECK(f.m.Write8(f.Payload()+i,i));f.SetRing(0,0x8000);f.Request(128);CHECK(f.Call()==0);
        for(unsigned i=0;i<128;++i)CHECK(f.Byte(Output+i)==i);}
    {Fixture f;for(unsigned i=0;i<128;++i)CHECK(f.m.Write8(f.Payload()+i,i+128));f.SetRing(0,0x8000);f.Request(128);CHECK(f.Call()==0);
        for(unsigned i=0;i<128;++i)CHECK(f.Byte(Output+i)==i+128);}
}
void EmptyAndBusy(){
    for(auto size:{0U,33U,128U}){Fixture f;f.Request(size);auto b=f.Snapshot();CHECK(f.Call()==0);
        CHECK(f.Get()==IpcCommandBuffer({0x00100082,0,0,2,Output}));CHECK(f.Snapshot()==b&&f.service->execution_probe()->CanSend(2));CHECK(f.Byte(Output)==0xA5);}
    // Firmware that never consumes a busy mailbox reaches the explicit bounded
    // fault AFTER committing the read pointer, without copying a success output.
    {Fixture f;CHECK(f.Call()==0);f.Request(2);auto q=f.Get();CHECK(!f.Call()&&f.Get()==q);
        CHECK(f.service->live_device()->notification_wait_slices()==4&&f.service->live_device()->error());
        CHECK(f.Byte(Output)==19&&f.Byte(Output+2)==0xA5);auto b=f.Snapshot();CHECK(!f.Call()&&f.Snapshot()==b);}
    {Fixture f(false,true);CHECK(f.Call()==0);f.Request(2,Output+2);auto q=f.Get();CHECK(!f.Call()&&f.Get()==q);
        CHECK(f.service->execution_probe()->summary().state==DspProbeState::Fault&&f.service->live_device()->error());
        CHECK(f.Byte(Output+2)==0xA5);auto b=f.Snapshot();CHECK(!f.Call()&&f.Snapshot()==b);}
    {Fixture f(true);Handle event{};CHECK(f.k.CreateEvent(&event,0)==0);
        f.Put({0x00150082,0,0,0,event});CHECK(f.Call()==0);f.Request(2);CHECK(f.Call()==0);
        f.Request(30,Output+2);auto now=f.k.now_ns();CHECK(f.Call()==0&&f.Get()[2]==30);
        CHECK(f.service->live_device()->notification_wait_slices()==1&&f.k.now_ns()==now);
        CHECK(f.service->interrupt_event(0)->signaled());
        for(unsigned i=0;i<32;++i)CHECK(f.Byte(Output+i)==static_cast<std::uint8_t>(i*7+19));}

}
void PointerAndShapeFailures(){
    for(auto out:{0x09000000U,0xFFFFFFFEU}){Fixture f;f.Request(4,out);auto q=f.Get();auto b=f.Snapshot();CHECK(f.Call()==kResultInvalidPointer&&f.Get()==q&&f.Snapshot()==b);}
    {Fixture f;CHECK(f.m.Map(0x09000000,4096,MemoryPermission::Read));f.Request(2,0x09000000);auto b=f.Snapshot();CHECK(f.Call()==kResultInvalidPointer&&f.Snapshot()==b);}
    {Fixture f;f.Request(2,Output,1);auto b=f.Snapshot();CHECK(f.Call()==kResultInvalidPointer&&f.Snapshot()==b);}
    {Fixture f;CHECK(f.m.Write32(f.table(),0x8002|0x400));auto b=f.Snapshot();CHECK(f.Call()==kResultInvalidPointer&&f.Snapshot()==b);}
    for(auto which:{0U,1U,2U,3U}){Fixture f;auto q=f.Get();if(which==0)q[0]^=1;else if(which==1)q[1]=1;else if(which==2)q[2]=1;else q[3]=129;f.Put(q);auto b=f.Snapshot();CHECK(!f.Call()&&f.Get()==q&&f.Snapshot()==b);}
    {Fixture f;auto q=f.Get();GuestMemory ro;CHECK(ro.Map(f.cb(),256,MemoryPermission::Read));CHECK(ro.LoadBytes(f.cb(),{reinterpret_cast<const std::uint8_t*>(q.data()),256}));
        auto b=f.Snapshot();CHECK(f.ipc.SendSyncRequest(f.k,ro,f.handle)==kResultInvalidPointer&&f.Snapshot()==b);}
    {Fixture f;auto q=f.Get();GuestMemory short_tls;CHECK(short_tls.Map(f.cb(),256,MemoryPermission::Read|MemoryPermission::Write));CHECK(short_tls.LoadBytes(f.cb(),{reinterpret_cast<const std::uint8_t*>(q.data()),256}));
        auto b=f.Snapshot();CHECK(f.ipc.SendSyncRequest(f.k,short_tls,f.handle)==kResultInvalidPointer&&f.Snapshot()==b);}
}
void AliasesAndReservations(){
    for(auto out_offset:{0x80U,0x180U,0x1FCU}){Fixture f;f.Request(2,f.k.current_thread()->tls_address+out_offset);auto q=f.Get();auto b=f.Snapshot();CHECK(!f.Call()&&f.Get()==q&&f.Snapshot()==b);}
    for(auto offset:{0x80U,0x180U}){Fixture f;CHECK(f.m.MapUserAlias(0x0E000000,0x08001000,0x1000,3)==0);
        f.k.current_thread()->tls_address=0x08001000;f.Request(2,0x0E000000+offset);auto q=f.Get();auto b=f.Snapshot();
        CHECK(!f.Call()&&f.Get()==q&&f.Snapshot()==b);}
    {Fixture f;f.Request(2,f.Payload());auto b=f.Snapshot();auto q=f.Get();CHECK(!f.Call()&&f.Get()==q&&f.Snapshot()==b);}
    {Fixture f;CHECK(f.m.MapUserAlias(0x0E000000,0x08000000,0x1000,3)==0);f.Request(2,0x0E000800);std::uint64_t v{},old{},after{};std::uint32_t fault{};
        CHECK(f.m.LoadExclusive(Output,2,&v,&old,&fault));CHECK(f.Call()==0);CHECK(f.m.LoadExclusive(Output,2,&v,&after,&fault));CHECK(old!=after);
        CHECK(f.Byte(Output)==19&&f.Byte(0x0E000800)==19);}
    {Fixture f;CHECK(f.m.Write16(Data+0x84+4*10+2,0));auto q=f.Get();auto b=f.Snapshot();CHECK(!f.Call()&&f.Get()==q&&f.Snapshot()==b);}
    // Unknown payload is not returned as allocation zeros.
    {Fixture f;CHECK(f.m.Write16(Data+0x84+4*10,0x6000));auto q=f.Get();auto b=f.Snapshot();CHECK(!f.Call()&&f.Get()==q&&f.Snapshot()==b);}
}
void AllocationFailure(){
    Fixture f;f.Request(32);auto q=f.Get();auto b=f.Snapshot();fail_after=0;auto result=f.Call();fail_after=-1;
    CHECK(!result&&f.Get()==q&&f.Snapshot()==b&&f.Byte(Output)==0xA5);CHECK(f.Call()==0);
}
}
int main(){try{ActualPayloadAndWrap();EmptyAndBusy();PointerAndShapeFailures();AliasesAndReservations();AllocationFailure();}
catch(const std::exception& e){fail_after=-1;std::cerr<<e.what()<<'\n';return 1;}
std::cout<<"PASS: real audio pipe reads, all-or-zero, wrap, static-buffer contract, preflight, aliases, reservations and explicit partial-fault containment\n";}
