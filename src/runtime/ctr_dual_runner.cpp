#include "runtime/ctr_runner.h"
#include "runtime/ctr_recorded_step.h"
#include "services/gsp_gpu_service.h"
#include <algorithm>
#include <limits>

namespace lego::ctr {
// Complete all timed events through target. Timeouts precede quota and display
// events at ties. No guest code executes between events at the same timestamp.
// Display state is preflighted before time is advanced to its event.
bool NativeRunner::AdvanceDiagnosticTime(std::uint64_t target,std::uint32_t limit) noexcept {
    if(target<kernel_.now_ns()){cpu_error_="diagnostic clock rollback";return false;}
    for(;;) {
        const auto timer=kernel_.NextWakeDeadline();
        const auto display=display_mode_==DisplayClockMode::Disabled
            ? std::optional<std::uint64_t>{}:display_clock_.next_deadline_ns();
        const auto quota_tick=kernel_.core1_quota().deadline();
        const auto quota=quota_tick?CpuTickDeadline(*quota_tick,diagnostic_origin_):std::nullopt;
        if((display_mode_!=DisplayClockMode::Disabled&&!display)||(quota_tick&&!quota)) {
            cpu_error_="diagnostic event deadline overflow";return false;
        }
        std::optional<std::uint64_t> next;
        for(const auto t:{timer,quota,display})if(t && (!next || *t<*next))next=t;
        if(!next || *next>target)break;
        const auto event_ns=std::max(*next,kernel_.now_ns());
        if(event_ns>target)break;
        if(idle_events_>=limit){cpu_error_="diagnostic timed-event work limit";return false;}
        const bool pdc=display && *display<=event_ns;
        DisplayPeriodPlan plan;
        if(pdc&&!gsp_->PrepareDisplayPeriod(plan,display_error_))return false;
        kernel_.AdvanceTime(event_ns-kernel_.now_ns());
        diagnostic_tick_=SystemTicksFromNanoseconds(event_ns-diagnostic_origin_);
        kernel_.SetDiagnosticTick(diagnostic_tick_);
        ++idle_events_;
        if(quota && *quota<=event_ns) {
            if(!kernel_.ConsumeCpuQuota(diagnostic_tick_)) {
                cpu_error_="diagnostic quota consumption overflow/invariant";return false;
            }
            if(!kernel_.core1_quota().allows_application() && kernel_.current_thread()->processor_id==1)
                live_state_.exclusive_valid=false;
            ++quota_transitions_;
        }
        if(pdc) {
            if(!gsp_->CommitDisplayPeriod(kernel_,plan)||!display_clock_.Consume(event_ns)) {
                cpu_error_="diagnostic display commit invariant";return false;
            }
        }
    }
    kernel_.AdvanceTime(target-kernel_.now_ns());
    diagnostic_tick_=SystemTicksFromNanoseconds(target-diagnostic_origin_);
    kernel_.SetDiagnosticTick(diagnostic_tick_);
    if(!memory_.EnsureSharedClockPage(kernel_.now_ns(),rtc_epoch_ms_)) {
        cpu_error_="diagnostic shared clock publication failed";return false;
    }
    return true;
}

RunnerResult NativeRunner::RunDiagnosticDual(std::uint32_t instruction_limit,
                                             std::uint32_t event_limit) noexcept {
    display_error_=nullptr;cpu_error_=nullptr;idle_events_=0;
    std::uint32_t rounds=0, since_service=0;
    const auto stop=[&](RunnerStopReason reason,const a32::ExecutionResult& e){return Stop(reason,e,rounds);};
    const auto stop_here=[&](RunnerStopReason reason){
        return stop(reason,{a32::ExitKind::Wait,live_state_.r[15],a32::FallbackReason::None,0});
    };
    const auto timing_stop=[&](){
        return stop_here(display_error_?RunnerStopReason::UnsupportedDisplayEvent:
                         idle_events_>=event_limit?RunnerStopReason::HostEventLimit:
                         RunnerStopReason::UnsupportedCpuExecution);
    };
    if(!instruction_limit)return stop_here(RunnerStopReason::BlockLimit);
    if(!event_limit)return stop_here(RunnerStopReason::HostEventLimit);
    if(!memory_.EnsureTlsMappings(kernel_))return stop_here(RunnerStopReason::MemoryFault);
    for(;;) {
        if(AllThreadsDead())return stop_here(RunnerStopReason::ProcessExited);
        if(!kernel_.AppCpuExecutionSupported())return stop_here(RunnerStopReason::UnsupportedCpuExecution);
        // Deliver a due timer (including Sleep(0)) before the next issue slot.
        if(!AdvanceDiagnosticTime(kernel_.now_ns(),event_limit))return timing_stop();
        if(rounds>=event_limit)return stop_here(RunnerStopReason::HostEventLimit);
        if(since_service>=instruction_limit)return stop_here(RunnerStopReason::BlockLimit);

        if(diagnostic_slot_==2) {
            if(diagnostic_tick_work_) {
                if(diagnostic_tick_==std::numeric_limits<std::uint64_t>::max()) {
                    cpu_error_="diagnostic CPU tick overflow";return timing_stop();
                }
                if(!pending_issue_deadline_)
                    pending_issue_deadline_=CpuTickDeadline(diagnostic_tick_+1,diagnostic_origin_);
                if(!pending_issue_deadline_){cpu_error_="diagnostic CPU deadline overflow";return timing_stop();}
                if(!AdvanceDiagnosticTime(*pending_issue_deadline_,event_limit))return timing_stop();
                pending_issue_deadline_.reset();diagnostic_slot_=0;diagnostic_tick_work_=false;
                continue;
            }
            // Neither core issued. Advance only to an actual timer, enabled
            // display event, or the quota window needed by a ready core-1 thread.
            std::optional<std::uint64_t> next=kernel_.NextWakeDeadline();
            const auto consider=[&](std::optional<std::uint64_t> t){if(t&&(!next||*t<*next))next=t;};
            if(display_mode_!=DisplayClockMode::Disabled)consider(display_clock_.next_deadline_ns());
            bool core1_pending=false;
            for(const auto& t:kernel_.threads())if(t->processor_id==1 &&
                (t->status==ThreadStatus::Ready||t->status==ThreadStatus::Running))core1_pending=true;
            if(core1_pending) {
                const auto d=kernel_.core1_quota().deadline();
                if(d)consider(CpuTickDeadline(*d,diagnostic_origin_));
            }
            if(!next)return stop_here(RunnerStopReason::WaitingNoRunnableThread);
            if(!AdvanceDiagnosticTime(std::max(*next,kernel_.now_ns()),event_limit))return timing_stop();
            diagnostic_slot_=0;
            continue;
        }
        const auto core=diagnostic_slot_;
        if(!kernel_.SelectDiagnosticCore(core,live_state_)){++diagnostic_slot_;continue;}
        const auto e=StepRecordedA32(registry_,live_state_,memory_);
        if(e.kind==a32::ExitKind::Svc) {
            ++rounds;
            a32::ExecutionResult handled=e;
            if(e.detail==0x0C) {
                // Observed first priority change on the newly created worker.
                const auto value=kernel_.SetFreshThreadPriority(live_state_.r[0],live_state_.r[1]);
                if(value) {
                    live_state_.r[0]=*value;live_state_.r[15]=e.pc+4;
                    handled={a32::ExitKind::Fallthrough,e.pc+4,a32::FallbackReason::None,0};
                }
            } else handled=svc_.Handle(e,live_state_,&memory_);
            if(handled.kind==a32::ExitKind::Svc) {
                return stop(ipc_.unsupported_request()&&e.detail==kSvcSendSyncRequest
                    ?RunnerStopReason::UnsupportedIpc:RunnerStopReason::UnsupportedSvc,e);
            }
            if(handled.kind!=a32::ExitKind::Fallthrough && handled.kind!=a32::ExitKind::Branch &&
               handled.kind!=a32::ExitKind::Wait)return stop(RunnerStopReason::OtherExit,handled);
            if(!memory_.EnsureTlsMappings(kernel_))return stop(RunnerStopReason::MemoryFault,handled);
            // A supported exception/service round trip invalidates this core's
            // local reservation. Unsupported calls preserve their original state.
            live_state_.exclusive_valid=false;
            since_service=0;
        } else if(e.kind==a32::ExitKind::Fallthrough || e.kind==a32::ExitKind::Branch) {
            ++since_service;
        } else {
            const auto reason=e.kind==a32::ExitKind::MemoryFault?RunnerStopReason::MemoryFault:
                e.kind==a32::ExitKind::MissingBlock?RunnerStopReason::MissingBlock:
                e.kind==a32::ExitKind::Fallback?RunnerStopReason::Fallback:RunnerStopReason::Unsupported;
            return stop(reason,e);
        }
        kernel_.SetCurrentGuestState(live_state_);
        ++issued_[core];diagnostic_tick_work_=true;++diagnostic_slot_;
    }
}
} // namespace lego::ctr
