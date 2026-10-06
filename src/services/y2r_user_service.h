#pragma once
#include <array>
#include "runtime/ctr_ipc.h"

namespace lego::ctr {
struct Y2rBuffer {
    std::uint32_t address{}, image_size{};
    std::uint16_t transfer_unit{}, gap{};
    bool operator==(const Y2rBuffer&) const = default;
};
// Pinned reference-HLE state, not a hardware register image or converted pixels.
struct Y2rConfiguration {
    std::uint8_t input_format{}, output_format{}, rotation{}, block_alignment{};
    std::uint16_t input_line_width{}, input_lines{};
    std::array<std::int16_t, 8> coefficients{};
    std::uint8_t padding{};
    std::uint16_t alpha{};
    Y2rBuffer src_y{}, src_u{}, src_v{}, src_yuyv{}, dst{};
    void DriverInitialize() noexcept {
        input_format = output_format = rotation = block_alignment = 0;
        coefficients.fill(0);
        input_line_width = 1024;
        // Pinned SetInputLines(1024) succeeds WITHOUT changing input_lines.
        // Do not turn that call into an assignment or reset unrelated fields.
        alpha = 0;
        src_y = src_u = src_v = dst = {};
        // src_yuyv, padding and input_lines deliberately retain their old values.
    }
    bool operator==(const Y2rConfiguration&) const = default;
};

// One connected session, persistent global configuration and a real one-shot
// completion object. No conversions, DMA, busy/completion claims or event timing.
class Y2rUserService final : public IpcService {
public:
    Y2rUserService() : shared_(std::make_shared<SharedState>()) {}
    Result CreateSessionHandler(std::shared_ptr<IpcService>* out) override;
    bool CanHandle(const IpcCommandBuffer& command) const noexcept override;
    Result Handle(IpcRouter&, Kernel&, GuestMemory&, ThreadObject&, IpcCommandBuffer&) override;
    [[nodiscard]] const Y2rConfiguration& configuration() const noexcept { return shared_->configuration; }
    [[nodiscard]] std::shared_ptr<EventObject> completion_event() const noexcept { return shared_->completion; }
    [[nodiscard]] bool initialized() const noexcept { return shared_->initialized; }
private:
    struct SessionIdentity {};
    struct SharedState {
        Y2rConfiguration configuration{};
        std::shared_ptr<EventObject> completion = std::make_shared<EventObject>(ResetType::OneShot);
        std::weak_ptr<SessionIdentity> session;
        bool initialized{};
    };
    explicit Y2rUserService(std::shared_ptr<SharedState> shared)
        : shared_(std::move(shared)), identity_(std::make_shared<SessionIdentity>()) {}
    std::shared_ptr<SharedState> shared_;
    std::shared_ptr<SessionIdentity> identity_;
};
} // namespace lego::ctr
