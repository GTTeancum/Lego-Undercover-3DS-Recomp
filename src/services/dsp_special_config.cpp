#include "services/dsp_special_config.h"
#include "host/sha256.h"
#include <algorithm>
#include <new>

namespace lego::ctr {
DspSpecialConfig DspSpecialConfig::EmptySystemConfig() noexcept {
    DspSpecialConfig source;
    source.profile_=DspSpecialProfile::EmptySystemConfig;
    return source;
}
std::optional<DspSpecialConfig> DspSpecialConfig::FromBlock(std::span<const std::uint8_t> data) noexcept {
    if (data.size()!=kDspSpecialConfigBytes) return std::nullopt;
    DspSpecialConfig source;
    source.profile_=DspSpecialProfile::SuppliedBlock;
    std::copy(data.begin(),data.end(),source.block_.begin());
    return source;
}
DspConfigRead DspSpecialConfig::Read(std::span<const std::uint8_t>& output) const noexcept {
    output={};
    if (profile_==DspSpecialProfile::Unconfigured) return DspConfigRead::Unconfigured;
    if (profile_==DspSpecialProfile::EmptySystemConfig) return DspConfigRead::Missing;
    output=block_;
    return DspConfigRead::Available;
}
DspSpecialError StageDspSpecial(const DspSpecialConfig& config, Dsp1Image& image,
                               DspSpecialReceipt& receipt) noexcept {
    using E=DspSpecialError;
    if (!image.special.required) return E::None;
    const auto& s=image.special;
    const auto type=static_cast<unsigned>(s.type);
    if (type>2 || s.bytes!=kDspSpecialConfigBytes) return E::UnsupportedShape;
    const std::uint64_t offset=std::uint64_t(s.target_words)*2;
    const auto max_words=type==0 ? 0x20000U : 0x10000U;
    if (s.target_words>=max_words || offset+s.bytes>kDsp1BankBytes) return E::Range;
    const auto layout=type==2 ? image.memory_layout>>8 : image.memory_layout&255;
    for (auto page=offset/0x8000;page<=(offset+s.bytes-1)/0x8000;++page)
        if (!(layout&(1U<<page))) return E::Layout;
    auto& bank=type==2 ? image.data : image.program;
    const auto begin=bank.known_mask().subspan(static_cast<std::size_t>(offset),s.bytes);
    if (std::any_of(begin.begin(),begin.end(),[](auto b){return b!=0;})) return E::AlreadyKnown;
    std::span<const std::uint8_t> data;
    const auto status=config.Read(data);
    if (status==DspConfigRead::Unconfigured) return E::Unresolved;
    const std::array<std::uint8_t,kDspSpecialConfigBytes> zero{};
    // Documented DSP module fallback after THIS explicit source's failed read.
    // No host file error and no unconfigured source are silently converted to zero.
    if (status==DspConfigRead::Missing) data=zero;
    try {
        DspSpecialReceipt next;
        next.source=status==DspConfigRead::Missing ? DspSpecialSource::MissingConfigZeroFallback
                                                 : DspSpecialSource::SuppliedConfigBlock;
        next.read_status=status;next.block_id=kDspSpecialConfigBlock;
        next.target_bytes=static_cast<std::uint32_t>(offset);next.bytes=s.bytes;
        next.memory_type=s.type;
        const auto hash=host::Sha256(data); // All fallible work BEFORE mutation.
        std::copy(hash.begin(),hash.end(),next.sha256.begin());
        if (!bank.Stage(next.target_bytes,data)) return E::AlreadyKnown;
        receipt=next; // fixed-size, nonthrowing
        return E::None;
    } catch (const std::bad_alloc&) { return E::Allocation; }
}
const char* DspSpecialErrorName(DspSpecialError e) noexcept {
    switch(e) {
    case DspSpecialError::None:return "none";
    case DspSpecialError::Unresolved:return "DSP special configuration is unconfigured";
    case DspSpecialError::UnsupportedShape:return "unsupported DSP special configuration shape";
    case DspSpecialError::Range:return "DSP special configuration target is outside its bank";
    case DspSpecialError::Layout:return "DSP special configuration target is outside selected memory";
    case DspSpecialError::AlreadyKnown:return "DSP special configuration would overwrite staged bytes";
    case DspSpecialError::Allocation:return "host allocation failed during DSP special configuration staging";
    }
    return "unknown DSP special configuration error";
}
} // namespace lego::ctr
