#include "services/ndm_service.h"
#include <limits>

namespace lego::ctr {
bool NdmService::CanHandle(const IpcCommandBuffer& command) const noexcept {
    if (command[0] == IpcMakeHeader(0x14, 1, 0)) return true;
    if (command[0] != IpcMakeHeader(0x6, 1, 0)) return false;
    // Keep exhaustion explicit rather than wrapping a nesting count to zero.
    for (std::size_t i = 0; i < suspend_counts_.size(); ++i)
        if ((command[1] & (1U << i)) &&
            suspend_counts_[i] == std::numeric_limits<std::uint32_t>::max()) return false;
    return true;
}

Result NdmService::Handle(IpcRouter&, Kernel&, GuestMemory&, ThreadObject&,
                         IpcCommandBuffer& command) {
    if (!CanHandle(command)) return kResultNotFound;
    const auto id = IpcCommandId(command[0]);
    const auto mask = command[1] & 0xFU;
    if (id == 0x14) {
        default_mask_ = current_mask_ = mask;
        for (std::size_t i = 0; i < statuses_.size(); ++i)
            if (mask & (1U << i)) statuses_[i] = DaemonStatus::Idle;
    } else {
        // Pinned NDM HLE updates this mask from the default, not the old mask.
        // Nested suspension is represented separately by per-daemon counts.
        current_mask_ = default_mask_ & ~mask;
        for (std::size_t i = 0; i < statuses_.size(); ++i)
            if ((mask & (1U << i)) && suspend_counts_[i]++ == 0)
                statuses_[i] = DaemonStatus::Suspended;
    }
    command.fill(0);
    command[0] = IpcMakeHeader(id, 1, 0);
    command[1] = kResultSuccess;
    return kResultSuccess;
}
} // namespace lego::ctr
