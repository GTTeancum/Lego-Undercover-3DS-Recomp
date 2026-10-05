#pragma once
#include "runtime/ctr_ipc.h"

namespace lego::ctr {
// Discovery endpoint: no guessed configuration data or success replies.
class CfgService final : public IpcService {
public:
    bool CanHandle(const IpcCommandBuffer&) const noexcept override { return false; }
    Result Handle(IpcRouter&, Kernel&, GuestMemory&, ThreadObject&,
                  IpcCommandBuffer&) override { return kResultNotFound; }
};
} // namespace lego::ctr
