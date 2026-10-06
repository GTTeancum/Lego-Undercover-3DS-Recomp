#include "runtime/ctr_kernel.h"

namespace lego::ctr {
bool Kernel::ConfigureCpuExecution(CpuExecutionMode mode) noexcept {
    if(mode!=CpuExecutionMode::Strict && mode!=CpuExecutionMode::DiagnosticDual)return false;
    // A different runner may not switch the execution policy of a live process.
    if(cpu_mode_!=CpuExecutionMode::Strict)return cpu_mode_==mode;
    if(mode==CpuExecutionMode::Strict)return true;
    if(threads_.size()!=1 || app_cpu_core0_only_ || now_ns_!=0 ||
       current_thread_->processor_id!=0)return false;
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
std::optional<Result> Kernel::SetFreshThreadPriority(Handle handle,std::uint32_t priority) noexcept {
    if(priority>kThreadPriorityLowest)return kResultOutOfRange;
    const auto thread=std::dynamic_pointer_cast<ThreadObject>(handles_.Get(handle));
    if(!thread)return kResultInvalidHandle;
    if(priority<static_cast<std::uint32_t>(application_resource_limit_->Limit(ResourceLimitType::Priority)))
        return 0xD9001BEAU; // pinned OS NotAuthorized
    if(thread->execution_started || thread==current_thread_ || thread->status!=ThreadStatus::Ready ||
       !thread->wait_objects_.empty() || thread->pending_wake)return std::nullopt;
    // It has never executed and cannot own or await a mutex. Ready selection
    // consults this same priority on the next issue slot; no unrelated wake.
    thread->priority=priority;return kResultSuccess;
}
} // namespace lego::ctr
