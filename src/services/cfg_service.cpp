#include "services/cfg_service.h"
#include <exception>
#include <stdexcept>

namespace lego::ctr {
namespace {
// azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac,
// cfg_defaults.cpp DEFAULT_STEREO_CAMERA_SETTINGS (Global / UserRead).
// Exact binary32 words in source order, serialized explicitly little-endian.
// The reference describes these as compatibility defaults, NOT measured reset
// state or fully understood calibration fields. Do not name unknown fields.
constexpr std::array<std::uint32_t, 8> kStereoWords{
    0x42780000U, 0x43908000U, 0x4299999AU, 0x423851ECU,
    0x41200000U, 0x40A00000U, 0x425E51ECU, 0x41AC8F5CU,
};
constexpr auto kStereoBytes = [] {
    std::array<std::uint8_t, kCfgStereoBytes> out{};
    for (std::size_t i = 0; i < kStereoWords.size(); ++i)
        for (unsigned b = 0; b < 4; ++b) out[i * 4 + b] = kStereoWords[i] >> (b * 8);
    return out;
}();
}

CfgService::CfgService(CfgProfile profile, CfgSoundMode sound_mode)
    : profile_(profile), sound_mode_(sound_mode) {
    if (sound_mode != CfgSoundMode::Unconfigured && sound_mode != CfgSoundMode::Mono &&
        sound_mode != CfgSoundMode::Stereo && sound_mode != CfgSoundMode::Surround)
        throw std::invalid_argument("unsupported CFG sound mode");
    if (profile != CfgProfile::Unconfigured && profile != CfgProfile::ReferenceStereo)
        throw std::invalid_argument("unsupported CFG profile");
}

const std::array<std::uint8_t, kCfgStereoBytes>&
CfgService::ReferenceStereoBytes() noexcept { return kStereoBytes; }

bool CfgService::CanHandle(const IpcCommandBuffer& q) const noexcept {
    // Exact observed write-only mapped-buffer shape, not a static buffer.
    // Size mismatches and other blocks remain host stops, not fabricated Results
    // or reference error-path zero output. No guest-controlled allocation size.
    if (q[0] != IpcMakeHeader(1, 2, 2)) return false;
    if (q[2] == kCfgSoundBlock)
        return sound_mode_ != CfgSoundMode::Unconfigured && q[1] == 1U && q[3] == 0x1CU;
    return profile_ == CfgProfile::ReferenceStereo && q[1] == kCfgStereoBytes &&
           q[2] == kCfgStereoBlock && q[3] == ((kCfgStereoBytes << 4) | 0xCU);
}

Result CfgService::Handle(IpcRouter& router, Kernel&, GuestMemory& memory,
                          ThreadObject& thread, IpcCommandBuffer& q) {
    if (!CanHandle(q)) return kResultNotFound;
    const auto address = q[4];
    const auto size = q[1]; // CanHandle bounds this to exactly 1 or 32.
    const std::array<std::uint8_t, 1> sound{static_cast<std::uint8_t>(sound_mode_)};
    const std::span<const std::uint8_t> bytes = q[2] == kCfgSoundBlock
        ? std::span<const std::uint8_t>(sound) : std::span<const std::uint8_t>(kStereoBytes);
    if (std::uint64_t(address) + size > 0x100000000ULL ||
        !memory.IsWritable(address, size))
        return kResultInvalidPointer;
    const auto cb64 = std::uint64_t(thread.tls_address) + kIpcCommandBufferOffset;
    if (cb64 + sizeof(IpcCommandBuffer) > 0x100000000ULL ||
        !memory.IsReadable(static_cast<std::uint32_t>(cb64), sizeof(IpcCommandBuffer)) ||
        !memory.IsWritable(static_cast<std::uint32_t>(cb64), sizeof(IpcCommandBuffer)))
        return kResultInvalidPointer;
    if (memory.SpansAlias(address, size,
                          static_cast<std::uint32_t>(cb64), sizeof(IpcCommandBuffer))) {
        router.RequestHostStop("CFG output aliases IPC response");
        return kResultSuccess; // Router stops without committing a guest reply.
    }
    try {
        // Reuse the reservation-safe private write path. Shared output and
        // cross-region spans are outside this bounded direct-buffer model.
        // This is host containment policy, not a claim that firmware forbids them.
        if (!memory.PrepareDeviceWrite(address, size)) {
            router.RequestHostStop("CFG output requires private writable backing");
            return kResultSuccess;
        }
    } catch (const std::exception& error) {
        router.RequestHostStop(error.what()); // No output bytes were changed.
        return kResultSuccess;
    }
    if (!memory.CommitDeviceWrite(address, bytes)) {
        router.RequestHostStop("CFG prepared output commit invariant failed");
        return kResultSuccess;
    }
    const auto descriptor = q[3];
    q.fill(0);
    q[0] = IpcMakeHeader(1, 1, 2);
    q[1] = kResultSuccess;
    q[2] = descriptor;
    q[3] = address;
    return kResultSuccess;
}
} // namespace lego::ctr
