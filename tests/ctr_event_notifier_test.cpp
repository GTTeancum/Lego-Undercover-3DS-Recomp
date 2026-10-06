#include "runtime/ctr_svc_bridge.h"
#include "runtime/ctr_ipc.h"
#include "runtime/ctr_memory.h"
#include <cstdlib>
#include <iostream>
using namespace lego::ctr;
namespace {
int failures=0;
#define CHECK(x) do { if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;} }while(0)
struct Target final : EventSignalTarget {
    unsigned calls{};EventObject* event{};bool saw_signaled{};const char* failure{};
    const char* OnSignal() noexcept override { ++calls;saw_signaled=event->signaled();return failure; }
};
void OrderAndRepeatedSignals() {
    for(auto reset:{ResetType::OneShot,ResetType::Sticky,ResetType::Pulse}) {
        Kernel k;auto t=std::make_shared<Target>();auto e=std::make_shared<EventObject>(reset,t);t->event=e.get();Handle h{};
        CHECK(k.handles().Create(&h,e)==0);auto before=k.now_ns();
        CHECK(k.SignalEvent(h)==0&&t->calls==1&&t->saw_signaled);
        CHECK(e->signaled()==(reset!=ResetType::Pulse));
        CHECK(k.SignalEvent(h)==0&&t->calls==2);CHECK(k.ClearEvent(h)==0&&t->calls==2&&!e->signaled());
        auto thread=k.current_thread();CHECK(k.WaitSynchronization1(h,-1).blocked);
        CHECK(k.SignalEvent(h)==0&&t->calls==3);
        CHECK(thread->status==ThreadStatus::Ready&&thread->pending_wake);
        // A OneShot waiter consumes before the notifier. Pulse clears afterwards.
        CHECK(t->saw_signaled==(reset!=ResetType::OneShot));
        CHECK(e->signaled()==(reset==ResetType::Sticky)&&k.now_ns()==before);
        CHECK(k.SignalEventObject(*e)&&t->calls==4);CHECK(k.event_signal_error()==nullptr);
    }
}
void FailureAndSvc() {
    Kernel k;IpcRouter ipc;SvcBridge svc(k,&ipc);GuestMemory m;
    auto t=std::make_shared<Target>();auto e=std::make_shared<EventObject>(ResetType::OneShot,t);t->event=e.get();t->failure="test target failed";
    Handle h{};CHECK(k.handles().Create(&h,e)==0);a32::GuestState s{};for(unsigned n=0;n<16;++n)s.r[n]=0xCCAA0000+n;
    s.r[0]=h;s.r[15]=0x123400;auto old=s;
    auto ret=svc.Handle({a32::ExitKind::Svc,s.r[15],a32::FallbackReason::None,kSvcSignalEvent},s,&m);
    CHECK(ret.kind==a32::ExitKind::Svc&&s.r==old.r&&t->calls==1&&e->signaled());
    CHECK(k.event_signal_error()==t->failure&&ipc.last_host_error()==t->failure);
    // No rollback claimed: ordinary signal state remains despite the host stop.
    CHECK(k.SignalEvent(0xDEADBEEF)==kResultInvalidHandle&&k.event_signal_error()==nullptr);
    t->failure=nullptr;CHECK(k.SignalEvent(h)==0&&t->calls==2);
    CHECK(k.CloseHandle(h)==0);CHECK(k.SignalEventObject(*e)&&t->calls==3);
}
}
int main(){OrderAndRepeatedSignals();FailureAndSvc();if(failures)return EXIT_FAILURE;
 std::cout<<"PASS: service notifier ordering, waiter consumption, repeated signals, retained objects and explicit failure stops\n";}
