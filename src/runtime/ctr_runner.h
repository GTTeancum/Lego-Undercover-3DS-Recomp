#pragma once

#include <cstdint>
#include <filesystem>

#include "recomp/a32_runtime.h"
#include "runtime/ctr_ipc.h"
#include "runtime/ctr_display_clock.h"
#include "runtime/ctr_host_frontend.h"
#include "runtime/ctr_kernel.h"
#include "runtime/ctr_memory.h"
#include "runtime/ctr_svc_bridge.h"
#include "services/ptm_service.h"
#include "services/cfg_service.h"
#include "services/dsp_special_config.h"
#include "services/dsp_execution_probe.h"
#include "services/fs_romfs_service.h"
#include "services/gsp_display_transfer.h"

namespace lego::ctr {
class GspGpuService;
class DspDiscoveryService;
class HidUserService;
namespace a32 = oot3d::recomp::a32;

enum class RunnerStopReason : std::uint8_t {
    ProcessExited,
    WaitingNoRunnableThread,
    UnsupportedSvc,
    UnsupportedIpc,
    BlockLimit,
    MissingBlock,
    MemoryFault,
    Unsupported,
    Fallback,
    HostEventLimit,
    OtherExit,
    UnsupportedDisplayEvent,
    UnsupportedCpuExecution,
    UnsupportedDspEvent,
    HostQuit,
    HostFrontendError,
};

struct RunnerResult {
    RunnerStopReason reason{RunnerStopReason::OtherExit};
    a32::ExecutionResult exit{};
    std::uint32_t thread_id{};
    std::uint32_t dispatch_rounds{};
};

class NativeRunner final {
public:
    NativeRunner(const a32::Registry& registry, GuestMemory& memory,
                 Kernel& kernel,
                 std::uint64_t rtc_epoch_ms = kDefaultRtcMsSince1900,
                 const std::filesystem::path& shared_extdata_root = {},
                 PtmStepMode ptm_step_mode = PtmStepMode::Unconfigured,
                 std::shared_ptr<const RomfsImage> romfs = {},
                 GpuVramMode vram_mode = GpuVramMode::Unconfigured,
                 DisplayClockMode display_mode = DisplayClockMode::Disabled,
                 CfgProfile cfg_profile = CfgProfile::Unconfigured,
                 CpuExecutionMode cpu_mode = CpuExecutionMode::Strict,
                 DspSpecialConfig dsp_config = {},
                 DspProbeOptions dsp_probe = {},
                 CfgSoundMode cfg_sound_mode = CfgSoundMode::Unconfigured,
                 CfgLanguage cfg_language = CfgLanguage::Unconfigured,
                 const std::filesystem::path& save_data_root = {});

    bool InitializeMainThread(std::uint32_t entry_point = kTextBase,
                              std::uint32_t stack_top = kMainStackTop) noexcept;

    [[nodiscard]] IpcRouter& ipc() noexcept { return ipc_; }
    [[nodiscard]] const IpcRouter& ipc() const noexcept { return ipc_; }

    [[nodiscard]] a32::GuestState& live_state() noexcept { return live_state_; }
    [[nodiscard]] const a32::GuestState& live_state() const noexcept {
        return live_state_;
    }

    [[nodiscard]] const std::array<std::uint64_t,2>& issued_instructions() const noexcept{return issued_;}
    [[nodiscard]] std::uint64_t diagnostic_ticks() const noexcept{return diagnostic_tick_;}
    [[nodiscard]] std::uint64_t quota_transitions() const noexcept{return quota_transitions_;}
    [[nodiscard]] const GspGpuService& gpu_diagnostics() const noexcept{return *gsp_;}
    [[nodiscard]] const DspDiscoveryService& dsp_diagnostics() const noexcept {return *dsp_;}
    [[nodiscard]] const HidUserService& hid_diagnostics() const noexcept { return *hid_; }
    // The first desktop slice uses diagnostic-dual's instruction boundaries and
    // the explicit display clock. Select once before Run; null keeps headless.
    bool SetHostFrontend(HostFrontend* frontend) noexcept;
    [[nodiscard]] const char* frontend_error() const noexcept { return frontend_error_; }
    [[nodiscard]] const char* dsp_error() const noexcept{return dsp_error_;}
    [[nodiscard]] const char* cpu_error() const noexcept{return cpu_error_;}
    [[nodiscard]] std::uint64_t display_periods() const noexcept { return display_clock_.periods_delivered(); }
    [[nodiscard]] std::optional<std::uint64_t> next_display_deadline() const noexcept { return display_clock_.next_deadline_ns(); }
    [[nodiscard]] const char* display_error() const noexcept { return display_error_; }
    RunnerResult Run(std::uint32_t block_limit_per_dispatch = 250000U,
                     std::uint32_t host_event_limit = 10000U) noexcept;

private:
    RunnerResult Stop(RunnerStopReason reason,
                      const a32::ExecutionResult& exit,
                      std::uint32_t dispatch_rounds) const noexcept;
    RunnerResult RunDiagnosticDual(std::uint32_t instruction_limit,std::uint32_t event_limit) noexcept;
    bool AdvanceDiagnosticTime(std::uint64_t ns,std::uint32_t event_limit) noexcept;
    bool EnsureRunnableCurrent() noexcept;
    bool AllThreadsDead() const noexcept;
    bool PumpIdleEvents() noexcept;
    RunnerStopReason IdleStopReason() const noexcept;
    bool PumpHostFrontend(bool frame_ready, bool sample_input) noexcept;
    [[nodiscard]] RunnerStopReason FrontendStopReason() const noexcept {
        return frontend_result_==HostFrontendResult::Quit ? RunnerStopReason::HostQuit
                                                        : RunnerStopReason::HostFrontendError;
    }

    const a32::Registry& registry_;
    GuestMemory& memory_;
    Kernel& kernel_;
    std::uint64_t rtc_epoch_ms_;
    IpcRouter ipc_;
    SvcBridge svc_;
    a32::GuestState live_state_{};
    std::shared_ptr<GspGpuService> gsp_;
    std::shared_ptr<DspDiscoveryService> dsp_;
    std::shared_ptr<HidUserService> hid_;
    HostFrontend* frontend_{};
    HostInputState host_input_{};
    HostFrontendResult frontend_result_{HostFrontendResult::Continue};
    const char* frontend_error_{};
    std::uint32_t frontend_poll_instructions_{};
    bool run_started_{}, frontend_started_{};
    DisplayClockMode display_mode_;
    DisplayClock display_clock_;
    std::uint32_t idle_events_{},idle_limit_{};
    const char* display_error_{};
    CpuExecutionMode cpu_mode_;
    std::uint64_t diagnostic_origin_{},diagnostic_tick_{},quota_transitions_{};
    std::array<std::uint64_t,2> issued_{};
    std::uint32_t diagnostic_slot_{};
    bool diagnostic_tick_work_{};
    std::optional<std::uint64_t> pending_issue_deadline_;
    const char* cpu_error_{};
    const char* dsp_error_{};
};

}  // namespace lego::ctr
