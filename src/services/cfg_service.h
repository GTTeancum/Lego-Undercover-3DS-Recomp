#pragma once
#include <array>
#include "runtime/ctr_ipc.h"

namespace lego::ctr {
enum class CfgProfile { Unconfigured, ReferenceStereo };
inline constexpr std::uint32_t kCfgStereoBlock = 0x00050005U;
inline constexpr std::uint32_t kCfgStereoBytes = 32U;

// Only the explicitly selected pinned HLE stereo default is available. This is
// not a recovered console configuration, physical camera calibration or a NAND
// save. Other blocks and commands remain untouched diagnostic stops.
class CfgService final : public IpcService {
public:
    explicit CfgService(CfgProfile profile = CfgProfile::Unconfigured);
    [[nodiscard]] CfgProfile profile() const noexcept { return profile_; }
    [[nodiscard]] static const std::array<std::uint8_t, kCfgStereoBytes>&
        ReferenceStereoBytes() noexcept;
    bool CanHandle(const IpcCommandBuffer& command) const noexcept override;
    Result Handle(IpcRouter&, Kernel&, GuestMemory&, ThreadObject&,
                  IpcCommandBuffer&) override;
private:
    const CfgProfile profile_;
};
} // namespace lego::ctr
