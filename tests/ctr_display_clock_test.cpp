#include "runtime/ctr_runner.h"
#include "services/gsp_gpu_service.h"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace lego::ctr;
namespace {
int failures=0;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}}while(0)
void RationalDeadlines(){
 DisplayClock c(123);std::uint64_t previous=0;
 for(std::uint64_t n=1;n<=1000;++n){
  const auto expected=123+(n*4481136ULL*1000000000ULL+268111856ULL-1)/268111856ULL;
  CHECK(c.next_deadline_ns()==expected);CHECK(expected>previous);
  CHECK(!c.Consume(expected-1));CHECK(c.Consume(expected));CHECK(c.periods_delivered()==n);previous=expected;
 }
 const auto first=*DisplayClock{}.next_deadline_ns();CHECK(first==16713681);
 DisplayClock edge(std::numeric_limits<std::uint64_t>::max()-first);
 CHECK(edge.next_deadline_ns()==std::numeric_limits<std::uint64_t>::max());
 CHECK(edge.Consume(std::numeric_limits<std::uint64_t>::max()));CHECK(!edge.next_deadline_ns());
 DisplayClock overflow(std::numeric_limits<std::uint64_t>::max());CHECK(!overflow.next_deadline_ns());
}
struct Fixture {
 a32::Registry registry{};Kernel k;GuestMemory m;NativeRunner runner;
 Handle session{},event{};std::shared_ptr<GspGpuService> g;
 explicit Fixture(DisplayClockMode mode=DisplayClockMode::ReferenceIdle):runner(registry,m,k,kDefaultRtcMsSince1900,{},PtmStepMode::Unconfigured,{},GpuVramMode::Unconfigured,mode){
  CHECK(runner.InitializeMainThread());CHECK(runner.ipc().ConnectToService(k,"gsp::Gpu",&session)==0);
  CHECK(k.CreateEvent(&event,0)==0);IpcCommandBuffer q{0x00130042,0,0,event};
  const auto cb=k.current_thread()->tls_address+kIpcCommandBufferOffset;
  for(unsigned i=0;i<q.size();++i)CHECK(m.Write32(cb+4*i,q[i]));
  const auto result=runner.ipc().SendSyncRequest(k,m,session);CHECK(result&&*result==0);
  g=std::dynamic_pointer_cast<GspGpuService>(std::dynamic_pointer_cast<ClientSessionObject>(k.handles().Get(session))->service);
 }
};
void IdleRealWakeAndStrict(){
 Fixture f;CHECK(f.k.WaitSynchronization1(f.event,-1).blocked);CHECK(!f.k.NextWakeDeadline());
 const auto result=f.runner.Run(100,10);CHECK(result.reason==RunnerStopReason::MissingBlock);
 CHECK(f.k.now_ns()==16713681&&f.runner.display_periods()==1);
 CHECK(f.runner.live_state().r[0]==0&&f.g->shared_memory()->bytes()[1]==2);
 auto event=f.g->relay_event();CHECK(event->signaled());
 // Remove both queued IRQs like a client and re-wait. Deadlines retain their phase.
 const std::uint8_t zero=0;CHECK(f.g->shared_memory()->Write(1,{&zero,1}));f.k.ClearEvent(f.event);
 CHECK(f.k.WaitSynchronization1(f.event,-1).blocked);CHECK(f.runner.Run(100,10).reason==RunnerStopReason::MissingBlock);
 CHECK(f.runner.display_periods()==2&&f.k.now_ns()==33427362);
 Fixture strict(DisplayClockMode::Disabled);CHECK(strict.k.WaitSynchronization1(strict.event,-1).blocked);
 CHECK(strict.runner.Run(100,3).reason==RunnerStopReason::WaitingNoRunnableThread);
 CHECK(strict.k.now_ns()==0&&strict.runner.display_periods()==0&&strict.g->shared_memory()->bytes()[1]==0);
 Fixture busy;CHECK(busy.runner.Run().reason==RunnerStopReason::MissingBlock);CHECK(busy.k.now_ns()==0);
}
void TimerOrderAndBudgets(){
 Fixture early;CHECK(early.k.WaitSynchronization1(early.event,100).blocked);CHECK(early.k.NextWakeDeadline()==100);
 CHECK(early.runner.Run().reason==RunnerStopReason::MissingBlock);CHECK(early.k.now_ns()==100);
 CHECK(early.runner.live_state().r[0]==kResultTimeout&&early.runner.display_periods()==0);
 Fixture equal;CHECK(equal.k.WaitSynchronization1(equal.event,16713681).blocked);
 CHECK(equal.runner.Run().reason==RunnerStopReason::MissingBlock);CHECK(equal.k.now_ns()==16713681);
 CHECK(equal.runner.live_state().r[0]==kResultTimeout&&equal.runner.display_periods()==1);
 Fixture later;CHECK(later.k.WaitSynchronization1(later.event,20000000).blocked);
 CHECK(later.runner.Run().reason==RunnerStopReason::MissingBlock);CHECK(later.k.now_ns()==16713681&&later.runner.live_state().r[0]==0);
 Fixture sleep;CHECK(sleep.k.SleepCurrentThread(77));CHECK(sleep.k.NextWakeDeadline()==77);
 CHECK(sleep.runner.Run().reason==RunnerStopReason::MissingBlock&&sleep.k.now_ns()==77&&sleep.runner.display_periods()==0);
 Fixture ignored;const std::uint8_t one=1;CHECK(ignored.g->shared_memory()->Write(3,{&one,1}));
 CHECK(ignored.k.WaitSynchronization1(ignored.event,-1).blocked);
 CHECK(ignored.runner.Run(100,3).reason==RunnerStopReason::HostEventLimit);CHECK(ignored.runner.display_periods()==3);
 const auto before=ignored.k.now_ns();CHECK(ignored.runner.Run(100,0).reason==RunnerStopReason::HostEventLimit);CHECK(ignored.k.now_ns()==before);
 CHECK(ignored.runner.Run(100,2).reason==RunnerStopReason::HostEventLimit&&ignored.runner.display_periods()==5);
 Fixture dead;dead.k.ExitCurrentThread();CHECK(dead.runner.Run().reason==RunnerStopReason::ProcessExited&&dead.k.now_ns()==0);
}
void FailureBeforeTime(){
 Fixture f;const std::uint8_t bad=53;CHECK(f.g->shared_memory()->Write(1,{&bad,1}));CHECK(f.k.WaitSynchronization1(f.event,-1).blocked);
 CHECK(f.runner.Run().reason==RunnerStopReason::UnsupportedDisplayEvent);
 CHECK(f.runner.display_error()!=nullptr&&f.k.now_ns()==0&&f.runner.display_periods()==0);
 CHECK(f.k.current_thread()->status==ThreadStatus::WaitSynchAny&&!f.g->relay_event()->signaled());
 const std::uint8_t zero=0;CHECK(f.g->shared_memory()->Write(1,{&zero,1}));
 CHECK(f.runner.Run().reason==RunnerStopReason::MissingBlock&&f.runner.display_error()==nullptr);
}
}
int main(){RationalDeadlines();IdleRealWakeAndStrict();TimerOrderAndBudgets();FailureBeforeTime();if(failures)return EXIT_FAILURE;
 std::cout<<"PASS: rational display deadlines, explicit idle time, timer ties, strict mode and bounded events\n";}
