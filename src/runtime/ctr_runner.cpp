#include "runtime/ctr_runner.h"
#include "services/apt_service.h"
#include "services/ndm_service.h"
#include "services/fs_user_service.h"
#include "services/cfg_service.h"
#include "services/ptm_service.h"

#include <algorithm>
#include <stdexcept>

namespace lego::ctr {

NativeRunner::NativeRunner(const a32::Registry& registry,
                           GuestMemory& memory,
                           Kernel& kernel,
                           std::uint64_t rtc_epoch_ms,
                           const std::filesystem::path& shared_extdata_root)
    : registry_(registry), memory_(memory), kernel_(kernel), rtc_epoch_ms_(rtc_epoch_ms), ipc_(), svc_(kernel, &ipc_) {
    ipc_.RegisterService("APT:U", std::make_shared<AptService>());
    ipc_.RegisterService("cfg:u", std::make_shared<CfgService>());
    ipc_.RegisterService("ptm:u", std::make_shared<PtmService>());
    ipc_.RegisterService("ndm:u", std::make_shared<NdmService>());
    auto fs = std::make_shared<FsUserService>(0x00040000000AD500ULL);
    if (!shared_extdata_root.empty() && !fs->ConfigureSharedExtdataRoot(shared_extdata_root))
        throw std::invalid_argument("shared-extdata root must be an existing directory");
    ipc_.RegisterService("fs:USER", std::move(fs));
}

bool NativeRunner::InitializeMainThread(std::uint32_t entry_point,
                                        std::uint32_t stack_top) noexcept {
    if (!ValidRtcEpoch(rtc_epoch_ms_) ||
        !memory_.EnsureMainStack() || !memory_.EnsureTlsMappings(kernel_) ||
        !memory_.EnsureSharedClockPage(kernel_.now_ns(), rtc_epoch_ms_)) {
        return false;
    }

    live_state_ = kernel_.CurrentGuestState();
    live_state_.r[0] = 0U;
    live_state_.r[13] = stack_top;
    live_state_.r[15] = entry_point & ~1U;
    live_state_.cpsr =
        kUserModeCpsr | ((entry_point & 1U) << 5U);
    live_state_.fpscr = kMainThreadInitialFpscr;
    live_state_.thread_pointer = kernel_.current_thread()->tls_address;
    live_state_.exclusive_address = 0U;
    live_state_.exclusive_token = 0U;
    live_state_.exclusive_size = 0U;
    live_state_.exclusive_valid = false;
    kernel_.SetCurrentGuestState(live_state_);
    return true;
}

bool NativeRunner::AllThreadsDead() const noexcept {
    return std::all_of(kernel_.threads().begin(), kernel_.threads().end(),
                       [](const auto& thread) {
                           return thread->status == ThreadStatus::Dead;
                       });
}

bool NativeRunner::EnsureRunnableCurrent() noexcept {
    if (kernel_.current_thread()->status == ThreadStatus::Running) {
        return true;
    }
    kernel_.Reschedule(live_state_);
    return kernel_.current_thread()->status == ThreadStatus::Running;
}

RunnerResult NativeRunner::Stop(RunnerStopReason reason,
                                const a32::ExecutionResult& exit,
                                std::uint32_t dispatch_rounds) const noexcept {
    return {
        reason,
        exit,
        kernel_.current_thread()->thread_id,
        dispatch_rounds,
    };
}

RunnerResult NativeRunner::Run(std::uint32_t block_limit_per_dispatch,
                               std::uint32_t host_event_limit) noexcept {
    if (!memory_.EnsureTlsMappings(kernel_)) {
        return Stop(
            RunnerStopReason::MemoryFault,
            {a32::ExitKind::MemoryFault, live_state_.r[15],
             a32::FallbackReason::None,
             kernel_.current_thread()->tls_address},
            0U);
    }

    if (!EnsureRunnableCurrent()) {
        return Stop(
            AllThreadsDead() ? RunnerStopReason::ProcessExited
                             : RunnerStopReason::WaitingNoRunnableThread,
            {a32::ExitKind::Wait, live_state_.r[15],
             a32::FallbackReason::None, 0U},
            0U);
    }

    for (std::uint32_t round = 1U; round <= host_event_limit; ++round) {
        // Guest execution is single-host-threaded. Publish a complete snapshot
        // before entering it; never advance guest time merely to unblock code.
        if (!memory_.EnsureSharedClockPage(kernel_.now_ns(), rtc_epoch_ms_)) {
            return Stop(RunnerStopReason::MemoryFault,
                        {a32::ExitKind::MemoryFault, live_state_.r[15],
                         a32::FallbackReason::None, kSharedPageBase}, round);
        }
        const a32::ExecutionResult exit = a32::Dispatch(
            registry_, live_state_.r[15], live_state_, memory_,
            nullptr, nullptr, block_limit_per_dispatch);

        if (exit.kind == a32::ExitKind::Svc) {
            const a32::ExecutionResult handled =
                svc_.Handle(exit, live_state_, &memory_);
            if (handled.kind == a32::ExitKind::Svc) {
                return Stop(ipc_.unsupported_request() && exit.detail == kSvcSendSyncRequest
                                ? RunnerStopReason::UnsupportedIpc
                                : RunnerStopReason::UnsupportedSvc,
                            handled, round);
            }

            if (!memory_.EnsureTlsMappings(kernel_)) {
                return Stop(
                    RunnerStopReason::MemoryFault,
                    {a32::ExitKind::MemoryFault, handled.pc,
                     a32::FallbackReason::None,
                     kernel_.current_thread()->tls_address},
                    round);
            }

            if (handled.kind == a32::ExitKind::Wait) {
                kernel_.Reschedule(live_state_);
                if (kernel_.current_thread()->status != ThreadStatus::Running) {
                    return Stop(
                        AllThreadsDead() ? RunnerStopReason::ProcessExited
                                         : RunnerStopReason::WaitingNoRunnableThread,
                        handled, round);
                }
                continue;
            }

            if (handled.kind == a32::ExitKind::Fallthrough ||
                handled.kind == a32::ExitKind::Branch) {
                // CTR may request rescheduling after SVCs that create or wake
                // threads. A no-op reschedule simply reselects the current
                // highest-priority thread.
                kernel_.Reschedule(live_state_);
                if (kernel_.current_thread()->status != ThreadStatus::Running) {
                    return Stop(
                        AllThreadsDead() ? RunnerStopReason::ProcessExited
                                         : RunnerStopReason::WaitingNoRunnableThread,
                        handled, round);
                }
                continue;
            }

            return Stop(RunnerStopReason::OtherExit, handled, round);
        }

        if (exit.kind == a32::ExitKind::Wait) {
            kernel_.Reschedule(live_state_);
            if (kernel_.current_thread()->status == ThreadStatus::Running) {
                continue;
            }
            return Stop(
                AllThreadsDead() ? RunnerStopReason::ProcessExited
                                 : RunnerStopReason::WaitingNoRunnableThread,
                exit, round);
        }

        switch (exit.kind) {
        case a32::ExitKind::BlockLimit:
            return Stop(RunnerStopReason::BlockLimit, exit, round);
        case a32::ExitKind::MissingBlock:
            return Stop(RunnerStopReason::MissingBlock, exit, round);
        case a32::ExitKind::MemoryFault:
            return Stop(RunnerStopReason::MemoryFault, exit, round);
        case a32::ExitKind::Unsupported:
            return Stop(RunnerStopReason::Unsupported, exit, round);
        case a32::ExitKind::Fallback:
            return Stop(RunnerStopReason::Fallback, exit, round);
        default:
            return Stop(RunnerStopReason::OtherExit, exit, round);
        }
    }

    return Stop(
        RunnerStopReason::HostEventLimit,
        {a32::ExitKind::BlockLimit, live_state_.r[15],
         a32::FallbackReason::None, host_event_limit},
        host_event_limit);
}

}  // namespace lego::ctr
