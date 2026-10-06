#include "services/y2r_user_service.h"

namespace lego::ctr {
Result Y2rUserService::CreateSessionHandler(std::shared_ptr<IpcService>* out) {
    if (!out) return kResultInvalidPointer;
    if (!shared_->session.expired()) return kResultMaxConnectionsReached;
    auto handler = std::shared_ptr<Y2rUserService>(new Y2rUserService(shared_));
    shared_->session = handler->identity_;
    *out = std::move(handler);
    return kResultSuccess;
}

bool Y2rUserService::CanHandle(const IpcCommandBuffer& command) const noexcept {
    return identity_ && command[0] == IpcMakeHeader(0x002B, 0, 0);
}

Result Y2rUserService::Handle(IpcRouter&, Kernel&, GuestMemory&, ThreadObject&,
                             IpcCommandBuffer& command) {
    if (!CanHandle(command)) return kResultNotFound;
    shared_->configuration.DriverInitialize();
    shared_->completion->Clear();
    shared_->initialized = true;
    command.fill(0);
    command[0] = IpcMakeHeader(0x002B, 1, 0);
    command[1] = kResultSuccess;
    return kResultSuccess;
}
} // namespace lego::ctr
