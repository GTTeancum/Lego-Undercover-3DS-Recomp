#include "runtime/ctr_kernel.h"

namespace lego::ctr {
// Mirror the existing handle-based SignalEvent semantics for the service's
// retained object. No new handle, fabricated wake result or time advance.
void Kernel::SignalEventObject(EventObject& event) noexcept {
    event.Signal();
    TryWakeWaitingThreads();
    if (event.reset_type() == ResetType::Pulse) event.Clear();
}
} // namespace lego::ctr
