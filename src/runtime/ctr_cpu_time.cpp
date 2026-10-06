#include "runtime/ctr_kernel.h"
#include <algorithm>

namespace lego::ctr {
std::optional<Result> Kernel::UpdateAppCpuTimeLimit(std::uint32_t value) noexcept {
    const auto maximum = application_resource_limit_->Limit(ResourceLimitType::CpuTime);
    // Existing application maximum (80 in this recovered kernel) is inherited;
    // no new ExHeader-derived maximum or launch-policy inference is made here.
    if (maximum < 0 || maximum > 100) return std::nullopt;
    // Pinned PM ignores values above maximum but replies success. In particular
    // an unsigned encoding of a negative value must not wrap into current state.
    if (value > static_cast<std::uint32_t>(maximum)) return kResultSuccess;
    for (const auto& thread : threads_) {
        if (thread->status != ThreadStatus::Dead && thread->processor_id != 0)
            return std::nullopt;
    }
    application_resource_limit_->SetCpuTime(static_cast<std::int32_t>(value));
    app_cpu_core0_only_ = true;
    // Reference UpdateCore1AppCpuLimit changes ONLY the core-1 limiter. There is
    // no participating core-1 thread in this supported slice. Do not introduce
    // fake CPU charges, alter core-0 priorities or make up a preemption timer.
    // SVC creation and runner dispatch guards prevent later unsupported use.
    return kResultSuccess;
}

bool Kernel::AppCpuExecutionSupported() const noexcept {
    if (!app_cpu_core0_only_) return true;
    return std::all_of(threads_.begin(), threads_.end(), [](const auto& thread) {
        return thread->status == ThreadStatus::Dead || thread->processor_id == 0;
    });
}

} // namespace lego::ctr
