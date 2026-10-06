#include "runtime/ctr_runner.h"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace lego::ctr;
namespace{
int failures=0;
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
void Priority(){
 Kernel k;a32::GuestState live=k.CurrentGuestState();Handle h=0;CHECK(k.CreateThread(&h,0x101000,0,0x8001000,48,1)==0);
 auto w=std::dynamic_pointer_cast<ThreadObject>(k.handles().Get(h));Handle duplicate=0;CHECK(k.DuplicateHandle(&duplicate,h)==0);
 CHECK(k.SetFreshThreadPriority(duplicate,49)==0&&w->priority==49);
 CHECK(k.SetFreshThreadPriority(h,23)==0xD9001BEAU&&w->priority==49);
 CHECK(k.SetFreshThreadPriority(0,64)==kResultOutOfRange);CHECK(k.SetFreshThreadPriority(0,48)==kResultInvalidHandle);
 CHECK(!k.SetFreshThreadPriority(kCurrentThreadPseudoHandle,48));
 w->execution_started=true;CHECK(!k.SetFreshThreadPriority(h,50)&&w->priority==49);
 Kernel strict;CHECK(strict.UpdateAppCpuTimeLimit(30)==0&&!strict.AppCpuThreadCreationSupported(1));
 CHECK(!strict.ConfigureCpuExecution(CpuExecutionMode::DiagnosticDual));
}
}
int main(){Quota();CoreSelection();Priority();if(failures)return EXIT_FAILURE;std::cout<<"PASS: independent core priorities/TLS, quota windows, reservations, strict guards and fresh priority\n";}
