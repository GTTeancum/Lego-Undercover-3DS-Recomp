#pragma once
#include <optional>
#include "runtime/ctr_ipc.h"

namespace lego::ctr {
// Application-only APT recovery slice. Other applets/commands remain stops.
class AptService final : public IpcService {
public:
    struct LaunchParameter {
        std::uint32_t sender_id{};
        std::uint32_t destination_id{0x300};
        std::uint32_t signal{1}; // APT Wakeup; no payload or attached object.
    };
    bool CanHandle(const IpcCommandBuffer& command) const noexcept override;
    Result Handle(IpcRouter&, Kernel& kernel, GuestMemory&, ThreadObject&,
                  IpcCommandBuffer& command) override;
    [[nodiscard]] bool initialized() const noexcept { return initialized_; }
    [[nodiscard]] const std::optional<LaunchParameter>& pending_parameter() const noexcept {
        return pending_parameter_;
    }
private:
    std::shared_ptr<MutexObject> lock_{std::make_shared<MutexObject>()};
    std::shared_ptr<EventObject> notification_{std::make_shared<EventObject>(ResetType::OneShot)};
    std::shared_ptr<EventObject> parameter_{std::make_shared<EventObject>(ResetType::OneShot)};
    bool initialized_{};
    std::optional<LaunchParameter> pending_parameter_;
};
} // namespace lego::ctr
