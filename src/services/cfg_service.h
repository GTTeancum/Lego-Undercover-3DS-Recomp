#pragma once
#include <array>
#include "runtime/ctr_ipc.h"

namespace lego::ctr {
enum class CfgProfile { Unconfigured, ReferenceStereo };
// User-selected host sound preference, independent of the camera profile.
// Values match the pinned CFG SoundOutputMode enum; omitted stays unsupported.
enum class CfgSoundMode : std::uint8_t { Mono = 0, Stereo = 1, Surround = 2, Unconfigured = 0xFF };
inline constexpr std::uint32_t kCfgSoundBlock = 0x00070001U;
inline constexpr std::uint32_t kCfgStereoBlock = 0x00050005U;
inline constexpr std::uint32_t kCfgStereoBytes = 32U;

// Camera defaults and sound preference are selected independently. Neither is
// recovered console configuration/calibration or a NAND save. Other blocks and
// commands remain untouched diagnostic stops.
class CfgService final : public IpcService {
public:
    explicit CfgService(CfgProfile profile = CfgProfile::Unconfigured,
                        CfgSoundMode sound_mode = CfgSoundMode::Unconfigured);
    [[nodiscard]] CfgSoundMode sound_mode() const noexcept { return sound_mode_; }
    [[nodiscard]] CfgProfile profile() const noexcept { return profile_; }
    [[nodiscard]] static const std::array<std::uint8_t, kCfgStereoBytes>&
        ReferenceStereoBytes() noexcept;
    bool CanHandle(const IpcCommandBuffer& command) const noexcept override;
    Result Handle(IpcRouter&, Kernel&, GuestMemory&, ThreadObject&,
                  IpcCommandBuffer&) override;
private:
    const CfgProfile profile_;
    const CfgSoundMode sound_mode_;
};
} // namespace lego::ctr
