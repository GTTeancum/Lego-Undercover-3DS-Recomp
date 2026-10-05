#pragma once
#include "runtime/ctr_ipc.h"

namespace lego::ctr {
// An explicit new, empty desktop pedometer profile, not recovered console data.
// No sensor inputs or step events are supplied by this reconstruction.
enum class PtmStepMode { Unconfigured, EmptyHistory };
// Host work/output bound, not a discovered firmware history limit.
inline constexpr std::uint32_t kMaxPtmHistoryHours = 2048;

class PtmService final : public IpcService {
public:
    explicit PtmService(PtmStepMode mode = PtmStepMode::Unconfigured) : mode_(mode) {}
    bool CanHandle(const IpcCommandBuffer& command) const noexcept override;
    Result Handle(IpcRouter&, Kernel&, GuestMemory&, ThreadObject&,
                  IpcCommandBuffer& command) override;
private:
    const PtmStepMode mode_;
};
} // namespace lego::ctr
