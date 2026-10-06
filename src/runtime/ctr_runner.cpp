#include "runtime/ctr_runner.h"
#include "services/apt_service.h"
#include "services/ndm_service.h"
#include "services/fs_user_service.h"
#include "services/cfg_service.h"
#include "services/gsp_gpu_service.h"
#include "services/ptm_service.h"
#include "services/y2r_user_service.h"
#include "services/hid_user_service.h"
#include "services/dsp_discovery_service.h"

#include <algorithm>
#include <stdexcept>

namespace lego::ctr {

NativeRunner::NativeRunner(const a32::Registry& registry,
                           GuestMemory& memory,
                           Kernel& kernel,
                           std::uint64_t rtc_epoch_ms,
                           const std::filesystem::path& shared_extdata_root,
                           PtmStepMode ptm_step_mode,
                           std::shared_ptr<const RomfsImage> romfs,
                           GpuVramMode vram_mode,
                           DisplayClockMode display_mode,
                           CfgProfile cfg_profile,
                           CpuExecutionMode cpu_mode,
                           DspSpecialConfig dsp_config, DspProbeOptions dsp_probe)
    : registry_(registry), memory_(memory), kernel_(kernel), rtc_epoch_ms_(rtc_epoch_ms), ipc_(), svc_(kernel, &ipc_),
      display_mode_(display_mode), display_clock_(kernel.now_ns()),
      cpu_mode_(cpu_mode),diagnostic_origin_(kernel.now_ns()) {
    if(dsp_probe.live && (!dsp_probe.enabled || cpu_mode!=CpuExecutionMode::DiagnosticDual))
        throw std::invalid_argument("live DSP requires enabled executor and diagnostic-dual clock");
    if(!kernel_.ConfigureCpuExecution(cpu_mode))
        throw std::invalid_argument("CPU mode must be configured before thread creation/resource setup");
    if (display_mode!=DisplayClockMode::Disabled && display_mode!=DisplayClockMode::ReferenceIdle)
        throw std::invalid_argument("invalid display clock mode");
    ipc_.RegisterService("APT:U", std::make_shared<AptService>());
    ipc_.RegisterService("cfg:u", std::make_shared<CfgService>(cfg_profile));
    auto gsp=std::make_shared<GspGpuService>();
    if (vram_mode==GpuVramMode::ReferenceZero)
        gsp->ConfigureVram(GpuVramBank::ReferenceZero());
    else if (vram_mode!=GpuVramMode::Unconfigured)
        throw std::invalid_argument("invalid GPU VRAM mode");
    gsp_=gsp;
    ipc_.RegisterService("gsp::Gpu",std::move(gsp));
    ipc_.RegisterService("hid:USER",std::make_shared<HidUserService>());
    dsp_=std::make_shared<DspDiscoveryService>(dsp_config,dsp_probe);
    ipc_.RegisterService("dsp::DSP",dsp_);
    ipc_.RegisterService("y2r:u", std::make_shared<Y2rUserService>());
    ipc_.RegisterService("ptm:u", std::make_shared<PtmService>(ptm_step_mode));
    ipc_.RegisterService("ndm:u", std::make_shared<NdmService>());
    auto fs = std::make_shared<FsUserService>(0x00040000000AD500ULL);
    if (!shared_extdata_root.empty() && !fs->ConfigureSharedExtdataRoot(shared_extdata_root))
        throw std::invalid_argument("shared-extdata root must be an existing directory");
    if (romfs) fs->ConfigureRomfs(std::move(romfs));
    ipc_.RegisterService("fs:USER", std::move(fs));
}

bool NativeRunner::InitializeMainThread(std::uint32_t entry_point,
                                        std::uint32_t stack_top) noexcept {
    if (!ValidRtcEpoch(rtc_epoch_ms_) ||
        !memory_.EnsureMainStack() || !memory_.EnsureTlsMappings(kernel_) ||
        !memory_.EnsureSharedClockPage(kernel_.now_ns(), rtc_epoch_ms_)) {
        return false;
    }

    kernel_.current_thread()->execution_started=true;
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

// ReferenceIdle is an explicit deterministic device-clock policy. Guest CPU
// dispatches currently cost no modeled cycles. ONLY idle time moves to the next
// actual timer/display deadline. CPU-busy vblank and cycle accuracy remain open.
bool NativeRunner::PumpIdleEvents() noexcept {
    if (display_mode_==DisplayClockMode::Disabled) return false;
    while (kernel_.current_thread()->status!=ThreadStatus::Running && !AllThreadsDead()) {
        if (idle_events_>=idle_limit_) return false;
        const auto display=display_clock_.next_deadline_ns();
        if (!display) {display_error_="display clock deadline overflow";return false;}
        const auto timer=kernel_.NextWakeDeadline();
        auto next=*display;
        if (timer && *timer<next)next=*timer;
        const auto now=kernel_.now_ns();
        if (next<now)next=now;
        const bool display_due=next>=*display;
        DisplayPeriodPlan plan;
        if (display_due && !gsp_->PrepareDisplayPeriod(plan,display_error_)) return false;
        // Explicit tie policy: timeout expiration precedes display notification at
        // the same nanosecond. No guest executes between those two operations.
        kernel_.AdvanceTime(next-now);
        ++idle_events_;
        if (display_due) {
            if (!gsp_->CommitDisplayPeriod(kernel_,plan)) {
                display_error_="prepared display event changed before commit";return false;
            }
            if (!display_clock_.Consume(next)) {
                display_error_="display clock consumption invariant failed";return false;
            }
        }
        kernel_.Reschedule(live_state_);
    }
    return kernel_.current_thread()->status==ThreadStatus::Running;
}
RunnerStopReason NativeRunner::IdleStopReason() const noexcept {
    if (display_error_) return RunnerStopReason::UnsupportedDisplayEvent;
    if (AllThreadsDead()) return RunnerStopReason::ProcessExited;
    if (display_mode_!=DisplayClockMode::Disabled && idle_events_>=idle_limit_)
        return RunnerStopReason::HostEventLimit;
    return RunnerStopReason::WaitingNoRunnableThread;
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
    if(cpu_mode_==CpuExecutionMode::DiagnosticDual)
        return RunDiagnosticDual(block_limit_per_dispatch,host_event_limit);
    idle_events_=0; idle_limit_=host_event_limit; display_error_=nullptr;
    if (!kernel_.AppCpuExecutionSupported()) {
        return Stop(RunnerStopReason::UnsupportedCpuExecution,
                    {a32::ExitKind::Unsupported,live_state_.r[15],a32::FallbackReason::None,1},0);
    }
    if (!memory_.EnsureTlsMappings(kernel_)) {
        return Stop(
            RunnerStopReason::MemoryFault,
            {a32::ExitKind::MemoryFault, live_state_.r[15],
             a32::FallbackReason::None,
             kernel_.current_thread()->tls_address},
            0U);
    }

    if (!EnsureRunnableCurrent() && !PumpIdleEvents()) {
        return Stop(
            IdleStopReason(),
            {a32::ExitKind::Wait, live_state_.r[15],
             a32::FallbackReason::None, 0U},
            0U);
    }

    for (std::uint32_t round = 1U; round <= host_event_limit; ++round) {
        // Fail closed even for externally created/retargeted threads. A changed
        // resource value must never make unmetered core-1 execution look supported.
        if (!kernel_.AppCpuExecutionSupported()) {
            return Stop(RunnerStopReason::UnsupportedCpuExecution,
                        {a32::ExitKind::Unsupported,live_state_.r[15],a32::FallbackReason::None,1},round);
        }
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
                if (kernel_.current_thread()->status != ThreadStatus::Running && !PumpIdleEvents()) {
                    return Stop(
                        IdleStopReason(),
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
                if (kernel_.current_thread()->status != ThreadStatus::Running && !PumpIdleEvents()) {
                    return Stop(
                        IdleStopReason(),
                        handled, round);
                }
                continue;
            }

            return Stop(RunnerStopReason::OtherExit, handled, round);
        }

        if (exit.kind == a32::ExitKind::Wait) {
            kernel_.Reschedule(live_state_);
            if (kernel_.current_thread()->status == ThreadStatus::Running || PumpIdleEvents()) {
                continue;
            }
            return Stop(
                IdleStopReason(),
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
