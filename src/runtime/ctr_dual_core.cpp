#include "runtime/ctr_kernel.h"

namespace lego::ctr {
bool Kernel::ConfigureCpuExecution(CpuExecutionMode mode,
                                   std::optional<std::uint32_t> launch_cpu_maximum) noexcept {
    if(mode!=CpuExecutionMode::Strict && mode!=CpuExecutionMode::DiagnosticDual)return false;
    if(launch_cpu_maximum && (mode!=CpuExecutionMode::DiagnosticDual || *launch_cpu_maximum>100))return false;
    // A different runner may not switch the execution policy of a live process.
    if(cpu_mode_!=CpuExecutionMode::Strict)return !launch_cpu_maximum && cpu_mode_==mode;
    if(mode==CpuExecutionMode::Strict)return true;
    if(threads_.size()!=1 || app_cpu_core0_only_ || now_ns_!=0 ||
       current_thread_->processor_id!=0 || current_thread_->status!=ThreadStatus::Running)return false;
    if(launch_cpu_maximum) {
        // Only before launch. Native callers authenticate the title ExHeader;
        // this setter neither guesses a ceiling nor changes a running process.
        application_resource_limit_->limits_[static_cast<std::size_t>(ResourceLimitType::CpuTime)]=
            static_cast<std::int32_t>(*launch_cpu_maximum);
        application_resource_limit_->SetCpuTime(0); // Pinned nonzero ExHeader launch path.
    }
    cpu_mode_=mode;return true;
}
bool Kernel::DiagnosticCoreReady(std::uint32_t core) const noexcept {
    if(core>1 || (core==1 && !core1_quota_.allows_application()))return false;
    for(const auto& t:threads_)
        if(t->processor_id==static_cast<std::int32_t>(core) &&
           (t->status==ThreadStatus::Ready || t->status==ThreadStatus::Running))return true;
    return false;
}
bool Kernel::SelectDiagnosticCore(std::uint32_t core,a32::GuestState& live) noexcept {
    if(cpu_mode_!=CpuExecutionMode::DiagnosticDual || core>1)return false;
    // Always save the currently loaded context, even if its SVC just blocked it.
    current_thread_->guest_state=live;
    current_thread_->guest_state.thread_pointer=current_thread_->tls_address;
    if(current_thread_->status==ThreadStatus::Running)current_thread_->status=ThreadStatus::Ready;
    if(core==1 && !core1_quota_.allows_application())return false;
    const auto prior=diagnostic_core_threads_[core].lock();
    std::shared_ptr<ThreadObject> next;
    for(const auto& t:threads_) {
        if(t->status!=ThreadStatus::Ready || t->processor_id!=static_cast<std::int32_t>(core))continue;
        if(!next || t->priority<next->priority ||
           (t->priority==next->priority && (t==prior || (next!=prior && t->thread_id<next->thread_id))))next=t;
    }
    if(!next)return false;
    if(next!=prior) {
        // A real logical-core context replacement loses its local reservation.
        // Alternating host execution between cores does NOT invalidate it.
        if(prior)prior->guest_state.exclusive_valid=false;
        next->guest_state.exclusive_valid=false;
    }
    diagnostic_core_threads_[core]=next;
    current_thread_=next;next->status=ThreadStatus::Running;next->execution_started=true;
    handles_.SetPseudoObjects(current_thread_,current_process_);
    ApplyPendingWakeToContext(*next);
    live=next->guest_state;live.thread_pointer=next->tls_address;
    return true;
}
bool Kernel::ConsumeCpuQuota(std::uint64_t tick) noexcept {
    if(!core1_quota_.Consume(tick))return false;
    if(!core1_quota_.allows_application())
        if(const auto prior=diagnostic_core_threads_[1].lock())prior->guest_state.exclusive_valid=false;
    return true;
}
std::optional<Result> Kernel::SetThreadPriority(Handle handle,std::uint32_t priority) noexcept {
    if(priority>kThreadPriorityLowest)return kResultOutOfRange;
    const auto thread=std::dynamic_pointer_cast<ThreadObject>(handles_.Get(handle));
    if(!thread)return kResultInvalidHandle;
    if(priority<static_cast<std::uint32_t>(application_resource_limit_->Limit(ResourceLimitType::Priority)))
        return 0xD9001BEAU; // pinned OS NotAuthorized
    switch(thread->status) {
    case ThreadStatus::Running:
    case ThreadStatus::Ready:
    case ThreadStatus::WaitSleep:
    case ThreadStatus::WaitArb:
    case ThreadStatus::WaitSynchAny:
    case ThreadStatus::WaitSynchAll:
        break;
    default:
        return std::nullopt;
    }
    if(thread->owns_mutex())return std::nullopt;
    // There is no nominal/effective priority split or mutex donation model yet.
    // Inspect retained wait references, not open handles: closing the last client
    // handle cannot hide a mutex dependency. Wait-all registrations count too.
    for(const auto& object:thread->wait_objects_)
        if(std::dynamic_pointer_cast<MutexObject>(object))return std::nullopt;
    // Ready selection and waiter wake ordering already sort the live priority.
    // Do not wake a sleeper, consume a signal, or overwrite a pending wake result.
    thread->priority=priority;return kResultSuccess;
}
} // namespace lego::ctr
