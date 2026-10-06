#pragma once
#include "runtime/ctr_ipc.h"
namespace lego::ctr {
// Discovery only. The observed GetIPCHandles and all input/sensor operations
// stop before output. No shared input buffer, controller state or event is seeded.
class HidUserService final : public IpcService {
public:
    bool CanHandle(const IpcCommandBuffer&) const noexcept override{return false;}
    Result Handle(IpcRouter&,Kernel&,GuestMemory&,ThreadObject&,IpcCommandBuffer&) override {
        return kResultNotFound;
    }
};
} // namespace lego::ctr
