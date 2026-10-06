#include "runtime/ctr_kernel.h"

namespace lego::ctr {
// Mirror the existing handle-based SignalEvent semantics for the service's
// retained object. No new handle, fabricated wake result or time advance.
bool Kernel::SignalEventObject(EventObject& event) noexcept {
    event.Signal();
    TryWakeWaitingThreads();
    // Pinned WaitObject calls its notifier even when a OneShot waiter already
    // consumed the signal. ClearEvent, export and mapping do not notify.
    event_signal_error_ = event.NotifySignal();
    if (event.reset_type() == ResetType::Pulse) event.Clear();
    return event_signal_error_ == nullptr;
}
std::optional<std::uint64_t> Kernel::NextWakeDeadline() const noexcept {
    std::optional<std::uint64_t> next;
    for (const auto& thread:threads_) {
        const auto status=thread->status;
        if (status!=ThreadStatus::WaitSleep && status!=ThreadStatus::WaitArb &&
            status!=ThreadStatus::WaitSynchAny && status!=ThreadStatus::WaitSynchAll) continue;
        if (thread->wake_deadline_ns_ && (!next || *thread->wake_deadline_ns_<*next))
            next=thread->wake_deadline_ns_;
    }
    return next;
}
} // namespace lego::ctr
