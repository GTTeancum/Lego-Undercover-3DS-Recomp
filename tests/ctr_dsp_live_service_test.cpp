#include "dsp_live_fixture.h"
#include "runtime/ctr_runner.h"
#include <cstdlib>
#include <iostream>
using namespace lego::ctr;
namespace {
int failures=0;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}}while(0)
const a32::PackedOp busy[]{ {0xEAFFFFFE,a32::EncodeMetadata(a32::Opcode::Branch,a32::Condition::Al)} };
const a32::PackedOp sleep[]{ {0xEF00000A,a32::EncodeMetadata(a32::Opcode::Svc,a32::Condition::Al)} };
const a32::Block blocks[]{{0x100000,busy,1,false},{0x100100,sleep,1,false}};
const a32::BlockShard shards[]{{0x100000,0x100FFC,blocks,2}};
const a32::Registry registry{shards,1,nullptr,0};
struct Fixture {
    Kernel k;GuestMemory m;NativeRunner r;Handle h{};IpcCommandBuffer q{};
    Fixture(bool fault=false,bool probe_only=false,bool notify=false):r(registry,m,k,kDefaultRtcMsSince1900,{},PtmStepMode::Unconfigured,{},
        GpuVramMode::Unconfigured,DisplayClockMode::Disabled,CfgProfile::Unconfigured,CpuExecutionMode::DiagnosticDual,{},
        DspProbeOptions{true,DspProbeReset::KnownOnly,100,!probe_only}){
        CHECK(r.InitializeMainThread());CHECK(m.Map(0x08000000,0x1000,MemoryPermission::Read));
        const auto b=dsp_live_fixture::Container(fault,notify);CHECK(m.LoadBytes(0x08000000,b));
        const auto n=static_cast<std::uint32_t>(b.size());q={0x001100C2,n,0xFF,0xE00FF,(n<<4)|0xA,0x08000000};
        CHECK(r.ipc().ConnectToService(k,"dsp::DSP",&h)==0);Put(q);
    }
    std::uint32_t cb()const{return k.current_thread()->tls_address+kIpcCommandBufferOffset;}
    void Put(const IpcCommandBuffer& v){for(unsigned i=0;i<v.size();++i)CHECK(m.Write32(cb()+i*4,v[i]));}
    IpcCommandBuffer Read(){IpcCommandBuffer b{};for(unsigned i=0;i<b.size();++i)CHECK(m.Read32(cb()+4*i,&b[i]));return b;}
    std::optional<Result> Call(){return r.ipc().SendSyncRequest(k,m,h);}
};
void SuccessfulLoadAndGuards(){
    Fixture f;const auto now=f.k.now_ns(),handles=f.k.handles().OpenHandleCount();
    CHECK(f.Call()==0);CHECK(f.k.now_ns()==now&&f.k.handles().OpenHandleCount()==handles);
    const IpcCommandBuffer expected{0x00110082,0,1,f.q[4],f.q[5]};CHECK(f.Read()==expected);
    auto* device=f.r.dsp_diagnostics().live_device();CHECK(device&&device->attached()&&device->slices()==0);
    CHECK(f.m.IsMapped(DspLiveDevice::DataAddress,0x40000));CHECK(!f.m.IsMapped(0x1FF00000));
    CHECK(!f.m.IsReadable(DspLiveDevice::DataAddress+0x200,1));
    const auto* p=f.r.dsp_diagnostics().execution_probe();const auto before=p->summary();
    f.Put(f.q);CHECK(!f.Call()&&f.Read()==f.q&&f.r.dsp_diagnostics().execution_probe()==p&&p->summary()==before);
    const IpcCommandBuffer unsupported{0x00120000};f.Put(unsupported);CHECK(!f.Call()&&f.Read()==unsupported);
    Fixture strict(false,true);CHECK(!strict.Call()&&!strict.r.dsp_diagnostics().live_device());
    CHECK(!strict.m.IsMapped(DspLiveDevice::DataAddress));CHECK(strict.r.dsp_diagnostics().execution_probe()->summary().state==DspProbeState::ProtocolComplete);
    Fixture malformed;auto q=malformed.q;q[0]^=1;malformed.Put(q);CHECK(!malformed.Call()&&!malformed.r.dsp_diagnostics().live_device()&&malformed.Read()==q);
}
void ProtectedResponseAndMapConflict(){
    Fixture f;GuestMemory readonly;CHECK(readonly.Map(f.cb(),sizeof(IpcCommandBuffer),MemoryPermission::Read));
    CHECK(readonly.LoadBytes(f.cb(),{reinterpret_cast<const std::uint8_t*>(f.q.data()),sizeof(f.q)}));
    CHECK(f.r.ipc().SendSyncRequest(f.k,readonly,f.h)==kResultInvalidPointer);
    CHECK(!f.r.dsp_diagnostics().execution_probe()&&!f.r.dsp_diagnostics().live_device());
    Fixture conflict;CHECK(conflict.m.Map(DspLiveDevice::DataAddress,0x1000,MemoryPermission::Read|MemoryPermission::Write));
    CHECK(conflict.m.Write32(DspLiveDevice::DataAddress,0xA55A1234));CHECK(!conflict.Call());
    std::uint32_t word=0;CHECK(conflict.m.Read32(DspLiveDevice::DataAddress,&word)&&word==0xA55A1234);
    CHECK(conflict.Read()==conflict.q&&!conflict.r.dsp_diagnostics().live_device());
    CHECK(!conflict.m.IsMapped(DspLiveDevice::DataAddress+0x1000));
}
void BusyAndIdleScheduling(){
    Fixture busy;CHECK(busy.Call()==0);const auto* device=busy.r.dsp_diagnostics().live_device();
    CHECK(busy.r.Run(10000,100).reason==RunnerStopReason::BlockLimit&&device->slices()==0);
    CHECK(busy.r.Run(10000,100).reason==RunnerStopReason::BlockLimit&&device->slices()==1);
    std::uint16_t word=0;CHECK(busy.m.Read16(DspLiveDevice::DataAddress+0x200,&word)&&word==0x7B);
    Fixture idle;CHECK(idle.Call()==0);idle.r.live_state().r[15]=0x100100;idle.r.live_state().r[0]=0xFFFFFFFF;idle.r.live_state().r[1]=0xFFFFFFFF;
    CHECK(idle.r.Run(100,2).reason==RunnerStopReason::HostEventLimit);CHECK(idle.r.dsp_diagnostics().live_device()->slices()==2);
    CHECK(idle.r.issued_instructions()[0]==1&&idle.r.issued_instructions()[1]==0);
    CHECK(idle.k.current_thread()->status!=ThreadStatus::Running); // not a manufactured wake
}
void RetainedInterrupts(){
    Fixture f(false,false,true);CHECK(f.Call()==0);Handle event=0;CHECK(f.k.CreateEvent(&event,0)==0);
    auto object=std::dynamic_pointer_cast<EventObject>(f.k.handles().Get(event));CHECK(object&&!object->signaled());
    f.Put({0x00150082,0,0,0,event});const auto now=f.k.now_ns(),handles=f.k.handles().OpenHandleCount();CHECK(f.Call()==0);
    CHECK(f.Read()[0]==0x00150040&&f.Read()[1]==0&&!object->signaled());
    CHECK(f.k.now_ns()==now&&f.k.handles().OpenHandleCount()==handles);
    CHECK(f.k.CloseHandle(event)==0&&f.r.dsp_diagnostics().interrupt_event(0)==object);
    CHECK(f.r.Run(20000,100).reason==RunnerStopReason::BlockLimit&&object->signaled());
    f.Put({0x00150082,0,0,0,0});CHECK(f.Call()==0&&!f.r.dsp_diagnostics().interrupt_event(0));
    f.Put({0x00150082,0,0,0,0x12345678});CHECK(f.Call()==kResultInvalidHandle&&!f.r.dsp_diagnostics().interrupt_event(0));
    // Capacity checks preserve original registration even for a replacement, as the pin does.
    for(unsigned i=0;i<6;++i){Handle e=0;CHECK(f.k.CreateEvent(&e,0)==0);f.Put({0x00150082,2,i,0,e});CHECK(f.Call()==0);}
    Handle e=0;CHECK(f.k.CreateEvent(&e,0)==0);IpcCommandBuffer full{0x00150082,2,6,0,e};f.Put(full);
    CHECK(!f.Call()&&f.Read()==full&&!f.r.dsp_diagnostics().interrupt_event(8));
}
void FaultSealsAtDeadline(){
    Fixture f(true);CHECK(f.Call()==0);const auto deadline=*f.r.dsp_diagnostics().live_device()->next_deadline_ns();
    CHECK(f.r.Run(40000,100).reason==RunnerStopReason::UnsupportedDspEvent);CHECK(f.r.dsp_error());
    CHECK(f.k.now_ns()==deadline&&f.r.dsp_diagnostics().live_device()->slices()==0);
    const auto issued=f.r.issued_instructions();const auto s=f.r.dsp_diagnostics().execution_probe()->summary();
    CHECK(f.r.Run(100,100).reason==RunnerStopReason::UnsupportedDspEvent&&f.r.issued_instructions()==issued);
    CHECK(f.k.now_ns()==deadline&&f.r.dsp_diagnostics().execution_probe()->summary()==s);
    CHECK(!f.m.IsReadable(DspLiveDevice::DataAddress+0x84,1));
    bool rejected=false;try{Kernel k;GuestMemory m;NativeRunner r(registry,m,k,kDefaultRtcMsSince1900,{},PtmStepMode::Unconfigured,{},
        GpuVramMode::Unconfigured,DisplayClockMode::Disabled,CfgProfile::Unconfigured,CpuExecutionMode::Strict,{},DspProbeOptions{true,DspProbeReset::KnownOnly,100,true});}
    catch(const std::invalid_argument&){rejected=true;}CHECK(rejected);
}
}
int main(){try{SuccessfulLoadAndGuards();ProtectedResponseAndMapConflict();BusyAndIdleScheduling();RetainedInterrupts();FaultSealsAtDeadline();}
 catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}if(failures)return 1;
 std::cout<<"PASS: real DSP load response, coherent map, protected replies, busy/idle scheduled execution and terminal device faults\n";}
