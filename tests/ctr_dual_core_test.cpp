#include "runtime/ctr_runner.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <type_traits>
using namespace lego::ctr;
namespace{
int failures=0;
static_assert(!std::is_copy_constructible_v<ThreadObject> && !std::is_move_constructible_v<ThreadObject>);
static_assert(!std::is_copy_constructible_v<MutexObject> && !std::is_move_constructible_v<MutexObject>);
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}}while(0)
void Quota(){
 for(unsigned v=1;v<=100;++v){Core1Quota q;CHECK(q.Update(v,17));auto app=kCpuQuotaPeriodTicks*v/100,sys=kCpuQuotaPeriodTicks-app;
  CHECK(!q.allows_application()&&q.deadline()==17+sys);if(sys)CHECK(!q.Consume(17+sys-1));
  CHECK(q.Consume(17+sys)&&q.allows_application()&&q.deadline()==17+kCpuQuotaPeriodTicks);
  CHECK(q.Consume(17+kCpuQuotaPeriodTicks)&&!q.allows_application());
  CHECK(q.Update(0,99)&&q.allows_application()&&!q.deadline());
 }
 Core1Quota q;CHECK(q.Update(30,0)&&q.deadline()==375357);CHECK(q.Update(30,12)&&q.allows_application()&&q.deadline()==160878);
 const auto old=q.deadline();CHECK(!q.Update(101,0)&&q.deadline()==old);
 CHECK(!q.Update(30,std::numeric_limits<std::uint64_t>::max())&&q.deadline()==old);
 CHECK(CpuTickDeadline(4481136)==16713681);CHECK(!CpuTickDeadline(std::numeric_limits<std::uint64_t>::max()));
 for(std::uint64_t i=1;i<10000;++i){auto ns=CpuTickDeadline(i);CHECK(ns&&SystemTicksFromNanoseconds(*ns)==i);}
}
void CoreSelection(){
 Kernel k;CHECK(k.ConfigureCpuExecution(CpuExecutionMode::DiagnosticDual));a32::GuestState live=k.CurrentGuestState();
 Handle worker=0;CHECK(k.CreateThread(&worker,0x101000,7,0x8001000,63,1)==0);
 auto w=std::dynamic_pointer_cast<ThreadObject>(k.handles().Get(worker));
 CHECK(k.SelectDiagnosticCore(0,live));auto main=k.current_thread();live.r[5]=123;live.exclusive_valid=true;
 CHECK(k.SelectDiagnosticCore(1,live)&&k.current_thread()==w&&live.r[0]==7);
 CHECK(k.handles().Get(kCurrentThreadPseudoHandle)==w&&live.thread_pointer==w->tls_address);live.r[5]=456;live.exclusive_valid=true;
 CHECK(k.SelectDiagnosticCore(0,live)&&live.r[5]==123&&live.exclusive_valid);
 CHECK(k.SelectDiagnosticCore(1,live)&&live.r[5]==456&&live.exclusive_valid);
 // A true same-core replacement clears reservations, not an unrelated-core issue.
 Handle better=0;CHECK(k.CreateThread(&better,0x102000,0,0x8002000,24,1)==0);
 CHECK(k.SelectDiagnosticCore(1,live)&&k.current_thread()->thread_id!=w->thread_id&&!live.exclusive_valid&&!w->guest_state.exclusive_valid);
 CHECK(k.UpdateAppCpuTimeLimit(30)==0);const auto before=live;CHECK(!k.SelectDiagnosticCore(1,live)&&live.r==before.r);
 CHECK(k.SelectDiagnosticCore(0,live)&&k.current_thread()==main);CHECK(!k.AppCpuThreadCreationSupported(2)&&!k.AppCpuThreadCreationSupported(3));
 CHECK(k.AppCpuThreadCreationSupported(1));CHECK(k.ConsumeCpuQuota(375357));CHECK(k.SelectDiagnosticCore(1,live));
 CHECK(!k.ConfigureCpuExecution(CpuExecutionMode::Strict));
}
bool SameState(const a32::GuestState& a,const a32::GuestState& b){
 return a.r==b.r&&a.cpsr==b.cpsr&&a.fpscr==b.fpscr&&a.thread_pointer==b.thread_pointer&&
        a.vfp==b.vfp&&a.exclusive_address==b.exclusive_address&&a.exclusive_token==b.exclusive_token&&
        a.exclusive_size==b.exclusive_size&&a.exclusive_valid==b.exclusive_valid;
}
std::shared_ptr<ThreadObject> Thread(Kernel& k,Handle h){
 return std::dynamic_pointer_cast<ThreadObject>(k.handles().Get(h));
}
Handle Worker(Kernel& k,std::uint32_t priority,std::int32_t core=0){
 Handle h=0;CHECK(k.CreateThread(&h,0x101000,0,0x8001000,priority,core)==0);return h;
}
void PriorityValidationAndAbi(){
 Kernel k;SvcBridge bridge(k);const auto main=k.current_thread();
 Handle real=0,duplicate=0,event=0;CHECK(k.DuplicateHandle(&real,kCurrentThreadPseudoHandle)==0);
 CHECK(k.DuplicateHandle(&duplicate,real)==0);CHECK(k.CreateEvent(&event,0)==0);
 const auto count=k.handles().OpenHandleCount();
 const auto call=[&](Handle h,std::uint32_t priority,Result result){
  a32::GuestState state=k.CurrentGuestState();
  for(unsigned i=0;i<state.r.size();++i)state.r[i]=0x12340000U+i;
  state.r[0]=h;state.r[1]=priority;state.r[15]=0x100000;
  state.vfp[7]=0x55667788;state.exclusive_address=0x8000000;
  state.exclusive_token=123;state.exclusive_size=4;state.exclusive_valid=true;
  auto expected=state;expected.r[0]=result;expected.r[15]+=4;
  const auto got=bridge.Handle({a32::ExitKind::Svc,0x100000,a32::FallbackReason::None,kSvcSetThreadPriority},state);
  CHECK(got.kind==a32::ExitKind::Fallthrough&&got.pc==0x100004&&SameState(state,expected));
  CHECK(k.current_thread()==main&&main->status==ThreadStatus::Running);
  CHECK(k.now_ns()==0&&k.handles().OpenHandleCount()==count);
 };
 call(0,64,kResultOutOfRange);call(0,0xFFFFFFFFU,kResultOutOfRange);
 call(0,23,kResultInvalidHandle);call(event,32,kResultInvalidHandle);
 call(kCurrentProcessPseudoHandle,32,kResultInvalidHandle);
 call(real,23,0xD9001BEAU);CHECK(main->priority==48);
 call(duplicate,24,0);CHECK(main->priority==24);
 call(kCurrentThreadPseudoHandle,63,0);CHECK(main->priority==63);
 CHECK(k.CloseHandle(duplicate)==0);
 CHECK(k.SetThreadPriority(duplicate,32)==kResultInvalidHandle&&main->priority==63);
 const auto fresh=Worker(k,48);CHECK(k.SetThreadPriority(fresh,49)==0&&Thread(k,fresh)->priority==49);
 // Dormant and dead objects remain an explicit bounded host stop.
 auto dormant=std::make_shared<ThreadObject>(999);Handle dormant_handle=0;
 CHECK(k.handles().Create(&dormant_handle,dormant)==0);
 CHECK(!k.SetThreadPriority(dormant_handle,32)&&dormant->priority==48&&dormant->status==ThreadStatus::Dormant);
 k.ExitCurrentThread();CHECK(!k.SetThreadPriority(real,32)&&main->priority==63&&main->status==ThreadStatus::Dead);
 Kernel strict;CHECK(strict.UpdateAppCpuTimeLimit(30)==0&&!strict.AppCpuThreadCreationSupported(1));
 CHECK(!strict.ConfigureCpuExecution(CpuExecutionMode::DiagnosticDual));
}
void PriorityScheduling(){
 Kernel k;CHECK(k.ConfigureCpuExecution(CpuExecutionMode::DiagnosticDual));auto live=k.CurrentGuestState();
 CHECK(k.SelectDiagnosticCore(0,live));const auto main=k.current_thread();const auto h=Worker(k,52);
 Handle main_handle=0;CHECK(k.DuplicateHandle(&main_handle,kCurrentThreadPseudoHandle)==0);
 auto worker=Thread(k,h);CHECK(k.SetThreadPriority(kCurrentThreadPseudoHandle,56)==0);
 CHECK(k.current_thread()==main&&main->status==ThreadStatus::Running&&worker->status==ThreadStatus::Ready);
 CHECK(k.SelectDiagnosticCore(0,live)&&k.current_thread()==worker&&worker->execution_started);
 live.r[5]=0xAABBCCDD;live.vfp[3]=0x13572468;
 CHECK(k.SetThreadPriority(h,60)==0&&k.current_thread()==worker);
 CHECK(k.SelectDiagnosticCore(0,live)&&k.current_thread()==main&&worker->status==ThreadStatus::Ready);
 const auto saved=worker->guest_state;
 CHECK(k.SetThreadPriority(h,40)==0&&SameState(worker->guest_state,saved));
 CHECK(k.SelectDiagnosticCore(0,live)&&k.current_thread()==worker&&live.r[5]==0xAABBCCDD);
 const auto peer=Worker(k,40);CHECK(k.SetThreadPriority(h,40)==0);
 // The older main thread would win a thread-id tie; the running worker keeps it.
 CHECK(k.SetThreadPriority(main_handle,40)==0&&main->status==ThreadStatus::Ready);
 CHECK(k.SelectDiagnosticCore(0,live)&&k.current_thread()==worker&&Thread(k,peer)->status==ThreadStatus::Ready);
 const auto other=Worker(k,49,1);CHECK(k.SelectDiagnosticCore(1,live)&&k.current_thread()==Thread(k,other));
 live.r[6]=0x10203040;live.exclusive_valid=true;
 CHECK(k.SelectDiagnosticCore(0,live)&&k.current_thread()==worker);
 const auto remote=Thread(k,other)->guest_state;const auto local=live;
 CHECK(k.SetThreadPriority(other,30)==0&&SameState(Thread(k,other)->guest_state,remote));
 CHECK(SameState(live,local)&&k.current_thread()==worker&&k.now_ns()==0);
 CHECK(k.SelectDiagnosticCore(1,live)&&live.r[6]==0x10203040&&live.exclusive_valid);
 CHECK(live.thread_pointer==Thread(k,other)->tls_address);
}
void SleepingPriority(){
 Kernel k;auto live=k.CurrentGuestState();const auto h=Worker(k,40);auto worker=Thread(k,h);
 CHECK(k.Reschedule(live)&&k.current_thread()==worker);live.r[5]=0x98765432;
 CHECK(k.SleepCurrentThread(500));CHECK(k.Reschedule(live));const auto saved=worker->guest_state;
 CHECK(k.NextWakeDeadline()==500&&k.SetThreadPriority(h,32)==0);
 CHECK(k.NextWakeDeadline()==500&&worker->status==ThreadStatus::WaitSleep&&!worker->pending_wake);
 CHECK(SameState(worker->guest_state,saved)&&k.now_ns()==0);
 k.AdvanceTime(499);CHECK(worker->status==ThreadStatus::WaitSleep);
 k.AdvanceTime(1);CHECK(worker->status==ThreadStatus::Ready&&!k.NextWakeDeadline());
 CHECK(k.Reschedule(live)&&k.current_thread()==worker&&live.r[5]==0x98765432);
}
void EventPriorityAndPendingWake(){
 Kernel k;auto live=k.CurrentGuestState();Handle e0=0,e1=0;
 CHECK(k.CreateEvent(&e0,0)==0&&k.CreateEvent(&e1,0)==0);
 const std::array<Handle,2> waits{e0,e1};const auto a=Worker(k,40),b=Worker(k,41);
 CHECK(k.Reschedule(live)&&k.current_thread()==Thread(k,a));
 CHECK(k.WaitSynchronizationN(waits,false,100).blocked);CHECK(k.Reschedule(live)&&k.current_thread()==Thread(k,b));
 CHECK(k.WaitSynchronizationN(waits,false,100).blocked);CHECK(k.Reschedule(live));
 CHECK(k.SetThreadPriority(b,32)==0&&k.NextWakeDeadline()==100&&k.now_ns()==0);
 CHECK(Thread(k,a)->status==ThreadStatus::WaitSynchAny&&Thread(k,b)->status==ThreadStatus::WaitSynchAny);
 CHECK(k.SignalEvent(e1)==0&&Thread(k,b)->status==ThreadStatus::Ready&&Thread(k,b)->pending_wake);
 CHECK(Thread(k,a)->status==ThreadStatus::WaitSynchAny);
 CHECK(Thread(k,b)->wait_result==0&&Thread(k,b)->wait_index==1&&Thread(k,b)->wait_index_valid);
 const auto saved=Thread(k,b)->guest_state;
 CHECK(k.SetThreadPriority(b,45)==0&&Thread(k,b)->pending_wake&&SameState(Thread(k,b)->guest_state,saved));
 CHECK(Thread(k,b)->wait_result==0&&Thread(k,b)->wait_index==1&&Thread(k,b)->wait_index_valid);
 CHECK(k.Reschedule(live)&&k.current_thread()==Thread(k,b)&&live.r[0]==0&&live.r[1]==1);
 CHECK(!Thread(k,b)->pending_wake&&k.NextWakeDeadline()==100);
 k.AdvanceTime(100);CHECK(Thread(k,a)->status==ThreadStatus::Ready&&Thread(k,a)->wait_result==kResultTimeout);
}
void WaitAllPriority(){
 Kernel k;auto live=k.CurrentGuestState();Handle event=0,semaphore=0;
 CHECK(k.CreateEvent(&event,0)==0&&k.CreateSemaphore(&semaphore,0,1)==0&&k.SignalEvent(event)==0);
 const std::array<Handle,2> waits{event,semaphore};const auto a=Worker(k,40),b=Worker(k,41);
 CHECK(k.Reschedule(live)&&k.current_thread()==Thread(k,a));CHECK(k.WaitSynchronizationN(waits,true,-1).blocked);
 CHECK(k.Reschedule(live)&&k.current_thread()==Thread(k,b));CHECK(k.WaitSynchronizationN(waits,true,-1).blocked);
 CHECK(k.Reschedule(live));CHECK(k.SetThreadPriority(b,32)==0);
 const auto e=std::dynamic_pointer_cast<EventObject>(k.handles().Get(event));
 const auto s=std::dynamic_pointer_cast<SemaphoreObject>(k.handles().Get(semaphore));
 CHECK(e->signaled()&&s->available_count()==0&&Thread(k,b)->status==ThreadStatus::WaitSynchAll);
 std::int32_t old=-1;CHECK(k.ReleaseSemaphore(&old,semaphore,1)==0&&old==0);
 CHECK(Thread(k,b)->status==ThreadStatus::Ready&&Thread(k,a)->status==ThreadStatus::WaitSynchAll);
 CHECK(!e->signaled()&&s->available_count()==0&&!Thread(k,b)->wait_index_valid&&k.now_ns()==0);
}
void ArbiterPriority(){
 Kernel k;GuestMemory memory;auto live=k.CurrentGuestState();Handle arbiter=0;
 CHECK(memory.Map(0x8000000,4,MemoryPermission::Read|MemoryPermission::Write));
 CHECK(memory.Write32(0x8000000,0)&&k.CreateAddressArbiter(&arbiter)==0);
 const auto a=Worker(k,40),b=Worker(k,41);
 CHECK(k.Reschedule(live)&&k.current_thread()==Thread(k,a));
 CHECK(k.ArbitrateAddress(&memory,arbiter,0x8000000,3,1,100).blocked);
 CHECK(k.Reschedule(live)&&k.current_thread()==Thread(k,b));
 CHECK(k.ArbitrateAddress(&memory,arbiter,0x8000000,3,1,100).blocked);CHECK(k.Reschedule(live));
 CHECK(k.SetThreadPriority(b,32)==0&&Thread(k,b)->status==ThreadStatus::WaitArb&&k.NextWakeDeadline()==100);
 std::uint32_t value=9;CHECK(memory.Read32(0x8000000,&value)&&value==0);
 CHECK(k.ArbitrateAddress(&memory,arbiter,0x8000000,0,1,0).result==0);
 CHECK(Thread(k,b)->status==ThreadStatus::Ready&&Thread(k,b)->wait_result==0&&Thread(k,b)->pending_wake);
 CHECK(Thread(k,a)->status==ThreadStatus::WaitArb&&k.NextWakeDeadline()==100&&k.now_ns()==0);
 k.AdvanceTime(100);CHECK(Thread(k,a)->status==ThreadStatus::Ready&&Thread(k,a)->wait_result==kResultTimeout);
}
void MutexOwnershipLifetime(){
 Kernel k;const auto owner=k.current_thread();Handle first=0,second=0;
 CHECK(!owner->owns_mutex()&&k.CreateMutex(&first,true)==0&&owner->owns_mutex());
 CHECK(!k.SetThreadPriority(kCurrentThreadPseudoHandle,50)&&owner->priority==48);
 CHECK(k.SetThreadPriority(kCurrentThreadPseudoHandle,64)==kResultOutOfRange);
 CHECK(k.WaitSynchronization1(first,0).result==0&&k.CreateMutex(&second,true)==0);
 auto a=std::dynamic_pointer_cast<MutexObject>(k.handles().Get(first));
 auto b=std::dynamic_pointer_cast<MutexObject>(k.handles().Get(second));
 CHECK(a->lock_count()==2&&b->lock_count()==1);
 const auto foreign=Worker(k,60);
 CHECK(b->Release(*Thread(k,foreign))==kResultWrongLockingThread&&owner->owns_mutex()&&!Thread(k,foreign)->owns_mutex());
 CHECK(k.ReleaseMutex(first)==0&&a->lock_count()==1&&owner->owns_mutex());
 CHECK(k.ReleaseMutex(first)==0&&a->lock_count()==0&&owner->owns_mutex());
 CHECK(k.ReleaseMutex(second)==0&&!owner->owns_mutex());
 CHECK(k.SetThreadPriority(kCurrentThreadPseudoHandle,50)==0&&owner->priority==50);
 // A live but unlocked object must not decrement ownership again on destruction.
 CHECK(k.CloseHandle(first)==0&&k.CloseHandle(second)==0);a.reset();b.reset();CHECK(!owner->owns_mutex());
 Handle held=0,alias=0;CHECK(k.CreateMutex(&held,true)==0&&k.DuplicateHandle(&alias,held)==0);
 CHECK(k.CloseHandle(held)==0&&owner->owns_mutex());
 CHECK(k.CloseHandle(alias)==0&&!owner->owns_mutex());
 CHECK(k.SetThreadPriority(kCurrentThreadPseudoHandle,42)==0);
 // A mutex may outlive its weak owner; destruction must not dereference it.
 auto orphan=std::make_shared<MutexObject>();
 {
  auto temporary=std::make_shared<ThreadObject>(900);orphan->Acquire(*temporary);
  CHECK(temporary->owns_mutex()&&orphan->lock_count()==1);
 }
 CHECK(!orphan->holding_thread());orphan.reset();
 // CreateMutex acquires before handle allocation; failed allocation rolls back
 // that temporary object's ownership through its real destructor.
 Kernel full;
 for(std::size_t i=0;i<HandleTable::kMaxCount;++i){Handle event=0;CHECK(full.CreateEvent(&event,0)==0);}
 Handle output=0x12345678;CHECK(full.CreateMutex(&output,true)==kResultOutOfHandles);
 CHECK(output==0x12345678&&!full.current_thread()->owns_mutex());
 CHECK(full.SetThreadPriority(kCurrentThreadPseudoHandle,40)==0);
}
void MutexPriorityStops(){
 Kernel k;SvcBridge bridge(k);auto live=k.CurrentGuestState();const auto owner=k.current_thread();
 Handle owner_handle=0,mutex_handle=0,alias=0;
 CHECK(k.DuplicateHandle(&owner_handle,kCurrentThreadPseudoHandle)==0&&k.CreateMutex(&mutex_handle,true)==0);
 CHECK(k.DuplicateHandle(&alias,mutex_handle)==0&&k.WaitSynchronization1(mutex_handle,0).result==0);
 auto mutex=std::dynamic_pointer_cast<MutexObject>(k.handles().Get(mutex_handle));
 const auto h=Worker(k,32);auto waiter=Thread(k,h);
 CHECK(k.Reschedule(live)&&k.current_thread()==waiter&&k.WaitSynchronization1(mutex_handle,100).blocked);
 CHECK(k.Reschedule(live)&&k.current_thread()==owner);
 const std::weak_ptr<MutexObject> retained=mutex;mutex.reset();
 CHECK(k.CloseHandle(mutex_handle)==0&&k.CloseHandle(alias)==0);
 CHECK(!retained.expired()&&owner->owns_mutex());mutex=retained.lock();
 const auto count=k.handles().OpenHandleCount(),thread_count=k.threads().size();
 const auto waiter_state=waiter->guest_state,owner_state=owner->guest_state;
 for(const auto target:{h,owner_handle}){
  live.r[0]=target;live.r[1]=30;live.r[15]=0x100000;live.exclusive_valid=true;
  const auto before=live;
  const auto got=bridge.Handle({a32::ExitKind::Svc,0x100000,a32::FallbackReason::None,kSvcSetThreadPriority},live);
  CHECK(got.kind==a32::ExitKind::Svc&&got.pc==0x100000&&got.detail==kSvcSetThreadPriority&&SameState(live,before));
  CHECK(waiter->priority==32&&owner->priority==48&&waiter->status==ThreadStatus::WaitSynchAny);
  CHECK(owner->status==ThreadStatus::Running&&!waiter->pending_wake&&waiter->wait_result==kResultTimeout);
  CHECK(SameState(waiter->guest_state,waiter_state)&&SameState(owner->guest_state,owner_state));
  CHECK(mutex->holding_thread()==owner&&mutex->lock_count()==2&&k.NextWakeDeadline()==100);
  CHECK(k.now_ns()==0&&k.handles().OpenHandleCount()==count&&k.threads().size()==thread_count);
 }
 CHECK(k.SetThreadPriority(h,64)==kResultOutOfRange&&k.SetThreadPriority(h,23)==0xD9001BEAU);
 const auto unrelated=Worker(k,60,1);CHECK(k.SetThreadPriority(unrelated,56)==0);
 // Retained wait state still times out normally after both client handles closed.
 CHECK(mutex->Release(*owner)==0&&owner->owns_mutex());
 CHECK(mutex->Release(*owner)==0&&!owner->owns_mutex());
 CHECK(k.SetThreadPriority(owner_handle,48)==0);
 k.AdvanceTime(100);CHECK(waiter->pending_wake&&waiter->wait_result==kResultTimeout);
 CHECK(k.SetThreadPriority(h,30)==0&&waiter->pending_wake&&waiter->wait_result==kResultTimeout);

 Kernel all;auto context=all.CurrentGuestState();Handle unlocked=0,event=0;
 CHECK(all.CreateMutex(&unlocked,false)==0&&all.CreateEvent(&event,0)==0);
 const auto blocked=Worker(all,32);CHECK(all.Reschedule(context));
 const std::array<Handle,2> waits{unlocked,event};CHECK(all.WaitSynchronizationN(waits,true,-1).blocked);
 CHECK(all.Reschedule(context)&&all.CloseHandle(unlocked)==0);
 // An available mutex in a blocked wait-all is still a mutex registration.
 CHECK(!all.SetThreadPriority(blocked,30)&&Thread(all,blocked)->status==ThreadStatus::WaitSynchAll);
 CHECK(all.SignalEvent(event)==0&&Thread(all,blocked)->status==ThreadStatus::Ready);
}
}
int main(){Quota();CoreSelection();PriorityValidationAndAbi();PriorityScheduling();SleepingPriority();
 EventPriorityAndPendingWake();WaitAllPriority();ArbiterPriority();MutexOwnershipLifetime();MutexPriorityStops();
 if(failures)return EXIT_FAILURE;
 std::cout<<"PASS: core selection/quotas, bounded active-thread priority, wait ordering, ABI and atomic mutex guards\n";}
