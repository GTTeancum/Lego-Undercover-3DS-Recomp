#pragma once

#include <cstdint>
#include <filesystem>

#include "recomp/a32_runtime.h"
#include "runtime/ctr_ipc.h"
#include "runtime/ctr_kernel.h"
#include "runtime/ctr_memory.h"
#include "runtime/ctr_svc_bridge.h"

namespace lego::ctr {
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
                 const std::filesystem::path& shared_extdata_root = {});

    bool InitializeMainThread(std::uint32_t entry_point = kTextBase,
                              std::uint32_t stack_top = kMainStackTop) noexcept;

    [[nodiscard]] IpcRouter& ipc() noexcept { return ipc_; }
    [[nodiscard]] const IpcRouter& ipc() const noexcept { return ipc_; }

    [[nodiscard]] a32::GuestState& live_state() noexcept { return live_state_; }
    [[nodiscard]] const a32::GuestState& live_state() const noexcept {
        return live_state_;
    }

    RunnerResult Run(std::uint32_t block_limit_per_dispatch = 250000U,
                     std::uint32_t host_event_limit = 10000U) noexcept;

private:
    RunnerResult Stop(RunnerStopReason reason,
                      const a32::ExecutionResult& exit,
                      std::uint32_t dispatch_rounds) const noexcept;
    bool EnsureRunnableCurrent() noexcept;
    bool AllThreadsDead() const noexcept;

    const a32::Registry& registry_;
    GuestMemory& memory_;
    Kernel& kernel_;
    std::uint64_t rtc_epoch_ms_;
    IpcRouter ipc_;
    SvcBridge svc_;
    a32::GuestState live_state_{};
};

}  // namespace lego::ctr
