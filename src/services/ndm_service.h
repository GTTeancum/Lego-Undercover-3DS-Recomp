#pragma once
#include <array>
#include "runtime/ctr_ipc.h"

namespace lego::ctr {
// Startup-only NDM service state, based on the pinned Azahar HLE. This does
// not start network daemons or claim network connectivity. Unknown commands stop.
class NdmService final : public IpcService {
public:
    enum class DaemonStatus : std::uint32_t { Busy, Idle, Suspending, Suspended };
    bool CanHandle(const IpcCommandBuffer& command) const noexcept override;
    Result Handle(IpcRouter&, Kernel&, GuestMemory&, ThreadObject&,
                  IpcCommandBuffer& command) override;
    [[nodiscard]] std::uint32_t default_mask() const noexcept { return default_mask_; }
    [[nodiscard]] std::uint32_t current_mask() const noexcept { return current_mask_; }
    [[nodiscard]] const std::array<DaemonStatus, 4>& statuses() const noexcept { return statuses_; }
    [[nodiscard]] const std::array<std::uint32_t, 4>& suspend_counts() const noexcept { return suspend_counts_; }
private:
    std::array<std::uint32_t, 4> suspend_counts_{};
    std::uint32_t default_mask_{0x9}; // CEC and Friends, matching pinned HLE.
    std::uint32_t current_mask_{0x9};
    std::array<DaemonStatus, 4> statuses_{
        DaemonStatus::Idle, DaemonStatus::Idle, DaemonStatus::Idle, DaemonStatus::Idle};
};
} // namespace lego::ctr
