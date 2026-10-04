#include "services/apt_service.h"

namespace lego::ctr {
bool AptService::CanHandle(const IpcCommandBuffer& command) const noexcept {
    // Only shapes actually observed from this title are enabled. Unknown or
    // malformed requests stop before side effects instead of returning success.
    if (command[0] == IpcMakeHeader(1, 1, 0)) return command[1] == 0;
    if (command[0] == IpcMakeHeader(2, 2, 0))
        return !initialized_ && command[1] == 0x300 && command[2] == 0;
    return false;
}

Result AptService::Handle(IpcRouter&, Kernel& kernel, GuestMemory&, ThreadObject&,
                          IpcCommandBuffer& command) {
    if (IpcCommandId(command[0]) == 1) {
        const auto attributes = command[1];
        ::lego::ctr::Handle handle = 0;
        const Result result = kernel.handles().Create(&handle, lock_);
        if (result != kResultSuccess) return result;
        command.fill(0);
        command[0] = IpcMakeHeader(1, 3, 2);
        command[1] = kResultSuccess;
        command[2] = attributes;
        command[3] = 0; // Pinned AppletManager::GetLockHandle state.
        command[4] = IpcCopyHandleDesc();
        command[5] = handle;
        return kResultSuccess;
    }

    // First application initialization: the pinned manager enables the first
    // applet and queues a real Wakeup parameter (sender None, receiver 0x300).
    // Its parameter event represents that queued message, NOT a fake GPU or
    // notification completion. Neither event is recreated on a new APT session.
    ::lego::ctr::Handle notification_handle = 0, parameter_handle = 0;
    Result result = kernel.handles().Create(&notification_handle, notification_);
    if (result != kResultSuccess) return result;
    result = kernel.handles().Create(&parameter_handle, parameter_);
    if (result != kResultSuccess) {
        kernel.CloseHandle(notification_handle);
        return result;
    }
    initialized_ = true;
    pending_parameter_ = LaunchParameter{};
    parameter_->Signal();
    command.fill(0);
    command[0] = IpcMakeHeader(2, 1, 3);
    command[1] = kResultSuccess;
    command[2] = IpcCopyHandleDesc(2);
    command[3] = notification_handle;
    command[4] = parameter_handle;
    return kResultSuccess;
}
} // namespace lego::ctr
