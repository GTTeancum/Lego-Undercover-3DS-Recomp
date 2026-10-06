#pragma once
#include <array>
#include "runtime/ctr_ipc.h"
#include "runtime/ctr_shared_memory.h"

namespace lego::ctr {
// Observed HID resource export only. The page is unpopulated service storage,
// not sampled controller/sensor data. No periodic producer or input event signal
// is implemented here; all other commands remain untouched diagnostic stops.
class HidUserService final : public IpcService {
public:
    static constexpr std::size_t kSessions = 6;
    static constexpr std::size_t kEvents = 5;
    HidUserService();
    Result CreateSessionHandler(std::shared_ptr<IpcService>* out) override;
    bool CanHandle(const IpcCommandBuffer& command) const noexcept override {
        return identity_ && command[0] == IpcMakeHeader(0x000A,0,0);
    }
    Result Handle(IpcRouter&,Kernel&,GuestMemory&,ThreadObject&,IpcCommandBuffer&) override;
    [[nodiscard]] std::shared_ptr<ServiceSharedMemoryObject> shared_memory() const noexcept {
        return shared_->memory;
    }
    [[nodiscard]] std::shared_ptr<EventObject> event(std::size_t index) const noexcept {
        return index < kEvents ? shared_->events[index] : nullptr;
    }
private:
    struct SessionIdentity {};
    struct SharedState {
        SharedState();
        std::shared_ptr<ServiceSharedMemoryObject> memory;
        std::array<std::shared_ptr<EventObject>,kEvents> events;
        std::array<std::weak_ptr<SessionIdentity>,kSessions> sessions;
    };
    HidUserService(std::shared_ptr<SharedState> shared, std::shared_ptr<SessionIdentity> identity)
        : shared_(std::move(shared)),identity_(std::move(identity)) {}
    std::shared_ptr<SharedState> shared_;
    std::shared_ptr<SessionIdentity> identity_;
};
} // namespace lego::ctr
