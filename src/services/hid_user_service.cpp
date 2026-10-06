#include "services/hid_user_service.h"

namespace lego::ctr {
HidUserService::SharedState::SharedState()
    : memory(std::make_shared<ServiceSharedMemoryObject>(1)) {
    // Pinned HID module owns five distinct unsignaled OneShot events and a
    // 4 KiB page. These objects survive client handle closure/reconnection.
    for (auto& event : events) event = std::make_shared<EventObject>(ResetType::OneShot);
}
HidUserService::HidUserService() : shared_(std::make_shared<SharedState>()) {}

Result HidUserService::CreateSessionHandler(std::shared_ptr<IpcService>* out) {
    if (!out) return kResultInvalidPointer;
    for (auto& weak : shared_->sessions) {
        if (!weak.expired()) continue;
        auto identity = std::make_shared<SessionIdentity>();
        auto session = std::shared_ptr<HidUserService>(new HidUserService(shared_,identity));
        weak = identity;
        *out = std::move(session);
        return kResultSuccess;
    }
    return kResultMaxConnectionsReached;
}

Result HidUserService::Handle(IpcRouter&, Kernel& kernel, GuestMemory&, ThreadObject&,
                              IpcCommandBuffer& command) {
    if (!CanHandle(command)) return kResultNotFound;
    // The router has preflighted the entire readable/writable reply before this
    // method. Returning six genuine copied handles does not sample or reset input.
    const std::array<std::shared_ptr<KernelObject>,6> objects{
        shared_->memory, shared_->events[0], shared_->events[1], shared_->events[2],
        shared_->events[3], shared_->events[4]};
    std::array<lego::ctr::Handle,6> handles{};
    const auto result = kernel.handles().CreateCopies(objects, handles);
    if (result != kResultSuccess) return result; // no partial reply/handle exports
    command.fill(0);
    command[0] = IpcMakeHeader(0x000A,1,7);
    command[1] = kResultSuccess;
    command[2] = IpcCopyHandleDesc(6);
    for (std::size_t i = 0; i < handles.size(); ++i) command[3+i] = handles[i];
    return kResultSuccess;
}
} // namespace lego::ctr
