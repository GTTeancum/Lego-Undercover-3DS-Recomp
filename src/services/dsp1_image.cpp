#include "services/dsp1_image.h"
#include "host/sha256.h"
#include <algorithm>
#include <new>

namespace lego::ctr {
bool Dsp1Bank::IsKnown(std::uint32_t offset, std::uint32_t size) const noexcept {
    return offset <= kDsp1BankBytes && size <= kDsp1BankBytes-offset &&
           std::all_of(known_.begin()+offset, known_.begin()+offset+size,
                       [](auto b) { return b != 0; });
}
std::optional<std::uint8_t> Dsp1Bank::Read(std::uint32_t offset) const noexcept {
    return IsKnown(offset, 1) ? std::optional(bytes_[offset]) : std::nullopt;
}
std::uint32_t Dsp1Bank::known_bytes() const noexcept {
    return static_cast<std::uint32_t>(std::count(known_.begin(), known_.end(), 1));
}
bool Dsp1Bank::Stage(std::uint32_t offset, std::span<const std::uint8_t> bytes) noexcept {
    if (offset > kDsp1BankBytes || bytes.size() > kDsp1BankBytes-offset) return false;
    const auto end=offset+bytes.size();
    if (std::any_of(known_.begin()+offset, known_.begin()+end, [](auto b){return b!=0;}))
        return false;
    std::copy(bytes.begin(),bytes.end(),bytes_.begin()+offset);
    std::fill(known_.begin()+offset,known_.begin()+end,1);
    return true;
}
namespace {
std::uint32_t U32(std::span<const std::uint8_t> b, std::size_t p) noexcept {
    return std::uint32_t(b[p]) | (std::uint32_t(b[p+1])<<8) |
           (std::uint32_t(b[p+2])<<16) | (std::uint32_t(b[p+3])<<24);
}
bool Overlap(std::uint64_t a, std::uint64_t n, std::uint64_t b, std::uint64_t m) noexcept {
    return a < b+m && b < a+n;
}
bool SameBank(Dsp1MemoryType a, Dsp1MemoryType b) noexcept {
    return (a == Dsp1MemoryType::Data) == (b == Dsp1MemoryType::Data);
}
bool TargetValid(std::uint8_t type, std::uint32_t words, std::uint32_t size) noexcept {
    if (type > 2 || !size) return false;
    const std::uint32_t max_start=type==0 ? 0x20000U : 0x10000U;
    return words < max_start && std::uint64_t(words)*2+size <= kDsp1BankBytes;
}
bool LayoutContains(std::uint16_t layout, Dsp1MemoryType type,
                    std::uint32_t words, std::uint32_t size) noexcept {
    const auto mask=type==Dsp1MemoryType::Data ? layout>>8 : layout&255U;
    const auto first=(words*2)/0x8000U, last=(words*2+size-1)/0x8000U;
    for (auto bank=first; bank<=last; ++bank) if (!(mask & (1U<<bank))) return false;
    return true;
}
std::string Hex(std::span<const std::uint8_t> b) {
    constexpr char h[]="0123456789abcdef";
    std::string s; s.reserve(b.size()*2);
    for (auto v:b) { s+=h[v>>4]; s+=h[v&15]; }
    return s;
}
}
Dsp1Result StageDsp1Image(std::span<const std::uint8_t> b,
                          std::unique_ptr<Dsp1Image>& output) noexcept {
    using E=Dsp1Error;
    if (b.size()<kDsp1HeaderBytes || b.size()>kDsp1MaxBytes) return {E::Size};
    if (b[0x100]!='D'||b[0x101]!='S'||b[0x102]!='P'||b[0x103]!='1') return {E::Magic};
    if (U32(b,0x104)!=b.size()) return {E::BinarySize};
    const auto count=b[0x10E], flags=b[0x10F];
    if (count<1 || count>10) return {E::SegmentCount};
    if (flags & ~3U) return {E::Flags};
    try {
        auto image=std::make_unique<Dsp1Image>();
        image->binary_bytes=static_cast<std::uint32_t>(b.size());
        image->memory_layout=std::uint16_t(b[0x108])|(std::uint16_t(b[0x109])<<8);
        image->receive_startup_replies=(flags&1)!=0;
        image->segments.reserve(count);
        for (std::uint32_t i=0; i<count; ++i) {
            const auto p=0x120+48*i;
            const auto offset=U32(b,p), words=U32(b,p+4), size=U32(b,p+8);
            const auto raw_type=b[p+15];
            if (raw_type>2) return {E::Type,i};
            const auto type=static_cast<Dsp1MemoryType>(raw_type);
            if (!size || offset<kDsp1HeaderBytes || offset>b.size() || size>b.size()-offset)
                return {E::SourceRange,i};
            if ((offset|size)&1U) return {E::Alignment,i};
            if (!TargetValid(raw_type,words,size)) return {E::TargetRange,i};
            if (!LayoutContains(image->memory_layout,type,words,size)) return {E::MemoryLayout,i};
            for (const auto& prior:image->segments) {
                if (Overlap(offset,size,prior.source_offset,prior.bytes)) return {E::SourceOverlap,i};
                if (SameBank(type,prior.type) && Overlap(std::uint64_t(words)*2,size,
                        std::uint64_t(prior.target_words)*2,prior.bytes)) return {E::TargetOverlap,i};
            }
            const auto hash=host::Sha256(b.subspan(offset,size));
            if (hash!=Hex(b.subspan(p+16,32))) return {E::SegmentHash,i};
            image->segments.push_back({offset,words,size,type,hash});
        }
        image->special.required=(flags&2)!=0;
        if (image->special.required) {
            const auto type=b[0x10D]; const auto words=U32(b,0x110), size=U32(b,0x114);
            if (!TargetValid(type,words,size) || (size&1U)) return {E::SpecialRange};
            image->special={true,static_cast<Dsp1MemoryType>(type),words,size};
            if (!LayoutContains(image->memory_layout,image->special.type,words,size))
                return {E::MemoryLayout};
            for (const auto& s:image->segments)
                if (SameBank(s.type,image->special.type) &&
                    Overlap(std::uint64_t(words)*2,size,std::uint64_t(s.target_words)*2,s.bytes))
                    return {E::SpecialOverlap};
        }
        // Unknown/reserved header bytes, RSA bytes and inactive records are not
        // treated as trusted metadata. In particular, SHA checks do not verify RSA.
        image->component_sha256=host::Sha256(b);
        for (const auto& s:image->segments) {
            auto& bank=s.type==Dsp1MemoryType::Data ? image->data : image->program;
            if (!bank.Stage(s.target_words*2,b.subspan(s.source_offset,s.bytes)))
                return {E::TargetOverlap};
        }
        output=std::move(image);
        return {};
    } catch (const std::bad_alloc&) { return {E::Allocation}; }
}
const char* Dsp1ErrorName(Dsp1Error error) noexcept {
    switch(error) {
    case Dsp1Error::None: return "none";
    case Dsp1Error::Size: return "component size outside host inspection bound";
    case Dsp1Error::Magic: return "DSP1 magic mismatch";
    case Dsp1Error::BinarySize: return "declared component size mismatch";
    case Dsp1Error::SegmentCount: return "invalid segment count";
    case Dsp1Error::Flags: return "unsupported DSP1 flags";
    case Dsp1Error::Type: return "unsupported memory type";
    case Dsp1Error::SourceRange: return "segment source outside component";
    case Dsp1Error::TargetRange: return "segment destination outside DSP bank";
    case Dsp1Error::Alignment: return "unsupported odd segment offset or length";
    case Dsp1Error::SourceOverlap: return "overlapping source segments";
    case Dsp1Error::TargetOverlap: return "overlapping target segments";
    case Dsp1Error::MemoryLayout: return "segment outside selected memory banks";
    case Dsp1Error::SegmentHash: return "segment SHA-256 mismatch";
    case Dsp1Error::SpecialRange: return "invalid special-segment range";
    case Dsp1Error::SpecialOverlap: return "special segment overlaps program data";
    case Dsp1Error::Allocation: return "host DSP image allocation failed";
    }
    return "unknown DSP image error";
}
} // namespace lego::ctr
