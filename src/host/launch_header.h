#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include "host/sha256.h"

namespace lego::host {
inline constexpr std::string_view kLegoExHeaderSha256 =
    "d7641f0a3bb89697ca8f0751e88d31703b54aa185ac78f9e74e41169dfcdc004";
struct LaunchPolicy {
    std::uint64_t program_id{};
    std::uint32_t maximum_cpu{}, ideal_processor{}, affinity_bits{}, priority{}, category{};
    bool multi{};
    bool operator==(const LaunchPolicy&) const = default;
};
// Layout only: NOT proof of authenticity. The native launcher uses the separate
// hash-gated function below. Offsets follow pinned ExHeader_ARM11_SystemLocalCaps.
inline std::optional<LaunchPolicy> ReadLaunchPolicy(std::span<const std::uint8_t> bytes) noexcept {
    if(bytes.size()!=0x800)return std::nullopt;
    LaunchPolicy out;
    for(unsigned i=0;i<8;++i)out.program_id|=std::uint64_t(bytes[0x200+i])<<(8*i);
    const auto descriptor=std::uint32_t(bytes[0x210]) | (std::uint32_t(bytes[0x211])<<8);
    out.maximum_cpu=descriptor&0x7FU;out.multi=(descriptor&0x80U)!=0;
    out.ideal_processor=bytes[0x20E]&3U;out.affinity_bits=(bytes[0x20E]>>2)&3U;
    out.priority=bytes[0x20F];out.category=bytes[0x36F];
    return out;
}
inline std::optional<LaunchPolicy> VerifiedLegoLaunchPolicy(std::span<const std::uint8_t> bytes) {
    const auto policy=ReadLaunchPolicy(bytes);
    if(!policy || Sha256(bytes)!=kLegoExHeaderSha256)return std::nullopt;
    if(policy->program_id!=0x00040000000AD500ULL || !policy->multi ||
       policy->maximum_cpu!=30 || policy->ideal_processor!=0 ||
       policy->priority!=48 || policy->category!=0)return std::nullopt;
    return policy;
}
} // namespace lego::host
