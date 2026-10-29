#include "runtime/ctr_runner.h"
#include "services/gsp_gpu_service.h"
#include <cstdlib>
#include <iostream>
using namespace lego::ctr;
namespace{
int failures=0;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}}while(0)
// CPU0 polls a shared flag while CPU1 writes it. No host writes the completion.
const a32::PackedOp main_ops[]{
 {0xE5920000,a32::EncodeMetadata(a32::Opcode::CoreMemory,a32::Condition::Al)},
 {0xE3500001,a32::EncodeMetadata(a32::Opcode::CoreAlu,a32::Condition::Al)},
 {0x1AFFFFFC,a32::EncodeMetadata(a32::Opcode::Branch,a32::Condition::Ne)},
 {0xEF000009,a32::EncodeMetadata(a32::Opcode::Svc,a32::Condition::Al)}};
const a32::PackedOp worker_ops[]{
 {0xE3A00001,a32::EncodeMetadata(a32::Opcode::CoreAlu,a32::Condition::Al)},
 {0xE5820000,a32::EncodeMetadata(a32::Opcode::CoreMemory,a32::Condition::Al)},
 {0xEF000009,a32::EncodeMetadata(a32::Opcode::Svc,a32::Condition::Al)}};
const a32::PackedOp sleeping_ops[]{
 {0xEF00000A,a32::EncodeMetadata(a32::Opcode::Svc,a32::Condition::Al)},
 {0xE3A00001,a32::EncodeMetadata(a32::Opcode::CoreAlu,a32::Condition::Al)},
 {0xE5820000,a32::EncodeMetadata(a32::Opcode::CoreMemory,a32::Condition::Al)},
 {0xEF000009,a32::EncodeMetadata(a32::Opcode::Svc,a32::Condition::Al)}};
const a32::Block blocks0[]{ {0x100000,main_ops,4,false} };
const a32::Block blocks1[]{ {0x101000,worker_ops,3,false},{0x101100,sleeping_ops,4,false} };
const a32::BlockShard shards[]{{0x100000,0x100FFC,blocks0,1},{0x101000,0x101FFC,blocks1,2}};
const a32::Registry registry{shards,2,nullptr,0};
struct Fixture{
 Kernel k;GuestMemory m;NativeRunner r;Handle h{};
 Fixture(bool sleep=false,DisplayClockMode display=DisplayClockMode::Disabled):r(registry,m,k,kDefaultRtcMsSince1900,{},PtmStepMode::Unconfigured,{},GpuVramMode::Unconfigured,display,CfgProfile::Unconfigured,CpuExecutionMode::DiagnosticDual){
  CHECK(r.InitializeMainThread());CHECK(m.Map(0x8000000,4,MemoryPermission::Read|MemoryPermission::Write));
  r.live_state().r[2]=0x8000000;
  CHECK(k.CreateThread(&h,sleep?0x101100:0x101000,sleep?100:0,0x8001000,48,1)==0);
  auto w=std::dynamic_pointer_cast<ThreadObject>(k.handles().Get(h));w->guest_state.r[2]=0x8000000;
 }
};
void ConcurrentAndResume(){
 Fixture f;CHECK(f.r.Run(100,10).reason==RunnerStopReason::ProcessExited);
 CHECK(f.r.issued_instructions()[0]==7&&f.r.issued_instructions()[1]==3);
 CHECK(f.k.now_ns()==*CpuTickDeadline(6));std::uint32_t value=0;CHECK(f.m.Read32(0x8000000,&value)&&value==1);
 Fixture sliced;RunnerResult got;unsigned calls=0;
 do{got=sliced.r.Run(2,10);++calls;}while(got.reason==RunnerStopReason::BlockLimit&&calls<30);
 CHECK(got.reason==RunnerStopReason::ProcessExited&&sliced.r.issued_instructions()==f.r.issued_instructions()&&sliced.k.now_ns()==f.k.now_ns());
 Fixture zero;CHECK(zero.r.Run(100,0).reason==RunnerStopReason::HostEventLimit&&zero.k.now_ns()==0&&zero.r.issued_instructions()[0]==0);
}
void QuotaAndBusyTimer(){
 Fixture f;CHECK(f.k.UpdateAppCpuTimeLimit(30)==0);CHECK(f.r.Run(100,10).reason==RunnerStopReason::BlockLimit);
 CHECK(f.r.issued_instructions()[1]==0&&f.k.now_ns()<*CpuTickDeadline(375357));
 CHECK(f.r.Run(800000,10).reason==RunnerStopReason::ProcessExited);
 CHECK(f.r.quota_transitions()==1&&f.r.issued_instructions()[1]==3&&f.k.now_ns()>=*CpuTickDeadline(375357));
 Fixture sleeping(true);CHECK(sleeping.r.Run(1000,10).reason==RunnerStopReason::ProcessExited);
 CHECK(sleeping.k.now_ns()>=100&&sleeping.r.issued_instructions()[1]==4&&sleeping.r.issued_instructions()[0]>7);
}
std::shared_ptr<GspGpuService> SetupGsp(Fixture& f){
 Handle h=0,event=0;CHECK(f.r.ipc().ConnectToService(f.k,"gsp::Gpu",&h)==0);CHECK(f.k.CreateEvent(&event,0)==0);
 IpcCommandBuffer q{0x00130042,0,0,event};const auto cb=f.k.current_thread()->tls_address+kIpcCommandBufferOffset;
 for(unsigned n=0;n<q.size();++n)CHECK(f.m.Write32(cb+4*n,q[n]));CHECK(f.r.ipc().SendSyncRequest(f.k,f.m,h)==0);
 return std::dynamic_pointer_cast<GspGpuService>(std::dynamic_pointer_cast<ClientSessionObject>(f.k.handles().Get(h))->service);
}
void EventFailureResume(){
 Fixture clean(false,DisplayClockMode::ReferenceIdle),interrupted(false,DisplayClockMode::ReferenceIdle);
 auto a=SetupGsp(clean),b=SetupGsp(interrupted);const std::uint8_t bad=53,zero=0;CHECK(b->shared_memory()->Write(1,{&bad,1}));
 clean.k.AdvanceTime(16713681-4);interrupted.k.AdvanceTime(16713681-4);
 CHECK(clean.r.Run(100,10).reason==RunnerStopReason::ProcessExited);
 CHECK(interrupted.r.Run(100,10).reason==RunnerStopReason::UnsupportedDisplayEvent);
 CHECK(interrupted.r.display_periods()==0&&interrupted.r.issued_instructions()[0]>0);
 CHECK(b->shared_memory()->Write(1,{&zero,1}));
 CHECK(interrupted.r.Run(100,10).reason==RunnerStopReason::ProcessExited);
 CHECK(clean.r.issued_instructions()==interrupted.r.issued_instructions()&&clean.k.now_ns()==interrupted.k.now_ns());
 CHECK(clean.r.display_periods()==1&&interrupted.r.display_periods()==1);
 CHECK(std::equal(a->shared_memory()->bytes().begin(),a->shared_memory()->bytes().end(),b->shared_memory()->bytes().begin()));
}
// GetIPCHandles is now implemented; retain this stop check on unsupported EnableAccelerometer.
void StopsAndHid(){
 Fixture f;f.r.live_state().r[15]=0x100800;const auto before=f.r.live_state();
 CHECK(f.r.Run(100,10).reason==RunnerStopReason::MissingBlock&&f.r.live_state().r==before.r&&f.k.now_ns()==0);
 f.k.threads()[1]->processor_id=2;CHECK(f.r.Run().reason==RunnerStopReason::UnsupportedCpuExecution);
 Fixture h;Handle hid=0;CHECK(h.r.ipc().ConnectToService(h.k,"hid:USER",&hid)==0);
 const auto cb=h.k.current_thread()->tls_address+kIpcCommandBufferOffset;IpcCommandBuffer q{0x00110000};
 for(unsigned n=0;n<q.size();++n)CHECK(h.m.Write32(cb+4*n,q[n]));
 const auto count=h.k.handles().OpenHandleCount();CHECK(!h.r.ipc().SendSyncRequest(h.k,h.m,hid));
 for(unsigned n=0;n<q.size();++n){std::uint32_t w=0;CHECK(h.m.Read32(cb+4*n,&w)&&w==q[n]);}
 CHECK(h.k.handles().OpenHandleCount()==count&&h.k.now_ns()==0&&h.r.ipc().last_session_name()=="hid:USER");
}
// Authored guest program: demote the current thread, then observe a peer's write.
// The completion value is produced only by guest instructions in both CPU modes.
const a32::PackedOp priority_svc_ops[]{
 {0xEF00000C,a32::EncodeMetadata(a32::Opcode::Svc,a32::Condition::Al)}};
const a32::PackedOp priority_resume_ops[]{
 {0xE5923000,a32::EncodeMetadata(a32::Opcode::CoreMemory,a32::Condition::Al)},
 {0xE5823004,a32::EncodeMetadata(a32::Opcode::CoreMemory,a32::Condition::Al)},
 {0xEF000009,a32::EncodeMetadata(a32::Opcode::Svc,a32::Condition::Al)}};
const a32::Block priority_main_blocks[]{
 {0x100000,priority_svc_ops,1,false},{0x100004,priority_resume_ops,3,false}};
const a32::Block priority_peer_blocks[]{{0x101000,worker_ops,3,false}};
const a32::BlockShard priority_shards[]{
 {0x100000,0x100FFC,priority_main_blocks,2},{0x101000,0x101FFC,priority_peer_blocks,1}};
const a32::Registry priority_registry{priority_shards,2,nullptr,0};
bool SameState(const a32::GuestState& a,const a32::GuestState& b){
 return a.r==b.r&&a.cpsr==b.cpsr&&a.fpscr==b.fpscr&&a.thread_pointer==b.thread_pointer&&a.vfp==b.vfp&&
        a.exclusive_address==b.exclusive_address&&a.exclusive_token==b.exclusive_token&&
        a.exclusive_size==b.exclusive_size&&a.exclusive_valid==b.exclusive_valid;
}
void PrioritySvcReschedules(){
 for(const auto mode:{CpuExecutionMode::Strict,CpuExecutionMode::DiagnosticDual}){
  Kernel k;GuestMemory memory;
  NativeRunner runner(priority_registry,memory,k,kDefaultRtcMsSince1900,{},PtmStepMode::Unconfigured,{},
                      GpuVramMode::Unconfigured,DisplayClockMode::Disabled,CfgProfile::Unconfigured,mode);
  CHECK(runner.InitializeMainThread());const auto main=k.current_thread();
  CHECK(memory.Map(0x8000000,8,MemoryPermission::Read|MemoryPermission::Write));
  runner.live_state().r[0]=kCurrentThreadPseudoHandle;runner.live_state().r[1]=56;runner.live_state().r[2]=0x8000000;
  Handle peer_handle=0;CHECK(k.CreateThread(&peer_handle,0x101000,0,0x8001000,52,0)==0);
  const auto peer=std::dynamic_pointer_cast<ThreadObject>(k.handles().Get(peer_handle));peer->guest_state.r[2]=0x8000000;
  CHECK(runner.Run(100,20).reason==RunnerStopReason::ProcessExited);
  std::uint32_t flag=0,observed=0;
  CHECK(memory.Read32(0x8000000,&flag)&&memory.Read32(0x8000004,&observed)&&flag==1&&observed==1);
  CHECK(main->priority==56&&peer->priority==52&&main->status==ThreadStatus::Dead&&peer->status==ThreadStatus::Dead);
  CHECK(main->guest_state.r[0]==0&&main->guest_state.r[1]==56&&main->guest_state.r[3]==1);
 }
}
void PrioritySvcAtomicStop(){
 for(const auto mode:{CpuExecutionMode::Strict,CpuExecutionMode::DiagnosticDual}){
  for(const bool target_waiter:{false,true}){
   Kernel k;GuestMemory memory;
   NativeRunner runner(priority_registry,memory,k,kDefaultRtcMsSince1900,{},PtmStepMode::Unconfigured,{},
                       GpuVramMode::Unconfigured,DisplayClockMode::Disabled,CfgProfile::Unconfigured,mode);
   CHECK(runner.InitializeMainThread());const auto owner=k.current_thread();
   CHECK(memory.Map(0x8000000,8,MemoryPermission::Read|MemoryPermission::Write));
   Handle mutex_handle=0,target=kCurrentThreadPseudoHandle;CHECK(k.CreateMutex(&mutex_handle,true)==0);
   const auto mutex=std::dynamic_pointer_cast<MutexObject>(k.handles().Get(mutex_handle));
   std::shared_ptr<ThreadObject> waiter;
   if(target_waiter){
    CHECK(k.CreateThread(&target,0x101000,0,0x8001000,32,0)==0);
    waiter=std::dynamic_pointer_cast<ThreadObject>(k.handles().Get(target));
    CHECK(k.Reschedule(runner.live_state())&&k.current_thread()==waiter);
    CHECK(k.WaitSynchronization1(mutex_handle,100).blocked);
    CHECK(k.Reschedule(runner.live_state())&&k.current_thread()==owner);
   }
   if(mode==CpuExecutionMode::DiagnosticDual)CHECK(k.SelectDiagnosticCore(0,runner.live_state()));
   auto& live=runner.live_state();live.r[0]=target;live.r[1]=56;live.r[2]=0x8000000;
   live.vfp[5]=0xDEADBEEF;live.exclusive_address=0x8000000;live.exclusive_token=7;
   live.exclusive_size=4;live.exclusive_valid=true;k.SetCurrentGuestState(live);
   const auto before=live;const auto handles=k.handles().OpenHandleCount(),threads=k.threads().size();
   const auto deadline=k.NextWakeDeadline();
   const auto stopped=runner.Run(100,20);
   CHECK(stopped.reason==RunnerStopReason::UnsupportedSvc&&stopped.exit.detail==kSvcSetThreadPriority);
   CHECK(stopped.exit.pc==0x100000&&stopped.dispatch_rounds==1&&SameState(live,before));
   CHECK(k.current_thread()==owner&&owner->status==ThreadStatus::Running&&owner->priority==48&&owner->owns_mutex());
   CHECK(mutex->holding_thread()==owner&&mutex->lock_count()==1&&k.NextWakeDeadline()==deadline);
   CHECK(k.now_ns()==0&&k.handles().OpenHandleCount()==handles&&k.threads().size()==threads);
   CHECK(runner.issued_instructions()[0]==0&&runner.issued_instructions()[1]==0);
   if(waiter)CHECK(waiter->priority==32&&waiter->status==ThreadStatus::WaitSynchAny&&!waiter->pending_wake);
   std::uint32_t untouched=1;CHECK(memory.Read32(0x8000004,&untouched)&&untouched==0);
  }
 }
}
}
int main(){ConcurrentAndResume();QuotaAndBusyTimer();EventFailureResume();StopsAndHid();
 PrioritySvcReschedules();PrioritySvcAtomicStop();if(failures)return EXIT_FAILURE;
 std::cout<<"PASS: dual-core progress, timers/quotas, shared priority SVC scheduling and atomic unsupported stops\n";}
