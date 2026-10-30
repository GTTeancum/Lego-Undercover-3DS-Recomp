#pragma once
#include <array>
#include "runtime/ctr_host_frontend.h"
#include "runtime/ctr_ipc.h"
#include "runtime/ctr_shared_memory.h"

namespace lego::ctr {
// HID resource export plus an explicitly selected host pad/touch producer.
// Without PublishHostInput the page and events retain their original passive
// behavior. Accelerometer, gyroscope, debug pad and raw calibration stay absent.
class HidUserService final : public IpcService {
public:
    static constexpr std::size_t kSessions = 6;
    static constexpr std::size_t kEvents = 5;
    HidUserService();
    // One coherent pad/touch sample at an explicitly chosen guest deadline.
    // Desktop mode uses display cadence, not the hardware's HID sampling rate.
    // Invalid controls or timestamp rollback leave all bytes/events untouched.
    bool PublishHostInput(Kernel&, const HostInputState&, std::uint64_t ticks) noexcept;
    [[nodiscard]] std::uint64_t host_input_samples() const noexcept { return shared_->samples; }
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
        std::uint32_t next_input_index{}, previous_buttons{};
        std::uint64_t samples{}, last_input_ticks{};
    };
    HidUserService(std::shared_ptr<SharedState> shared, std::shared_ptr<SessionIdentity> identity)
        : shared_(std::move(shared)),identity_(std::move(identity)) {}
    std::shared_ptr<SharedState> shared_;
    std::shared_ptr<SessionIdentity> identity_;
};
} // namespace lego::ctr
