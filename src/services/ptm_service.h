#pragma once
#include "runtime/ctr_ipc.h"

namespace lego::ctr {
// The game requests this endpoint before opening shared extdata. Discovery is
// supported, but step history/counts, battery and hardware values are unmodeled.
// Stop at each request without manufacturing a successful reply or output data.
class PtmService final : public IpcService {
public:
    bool CanHandle(const IpcCommandBuffer&) const noexcept override { return false; }
    Result Handle(IpcRouter&, Kernel&, GuestMemory&, ThreadObject&,
                  IpcCommandBuffer&) override { return kResultNotFound; }
};
} // namespace lego::ctr
