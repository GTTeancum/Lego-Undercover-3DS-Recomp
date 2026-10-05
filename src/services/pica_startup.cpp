#include "services/pica_startup.h"

namespace lego::ctr {
namespace {
std::uint32_t Word(std::span<const std::uint8_t> b,std::size_t n) noexcept {
    return std::uint32_t(b[n]) | (std::uint32_t(b[n+1])<<8) |
           (std::uint32_t(b[n+2])<<16) | (std::uint32_t(b[n+3])<<24);
}
std::uint32_t ByteMask(std::uint32_t mask) noexcept {
    std::uint32_t result=0;
    for(unsigned i=0;i<4;++i) if(mask&(1U<<i)) result|=0xFFU<<(i*8);
    return result;
}
// Exact lossless reference f24::FromRaw conversion to IEEE bits, including
// signed zero/denormals/infinity/NaNs. No host floating-point arithmetic needed.
std::uint32_t Float24Bits(std::uint32_t raw) noexcept {
    const auto sign=(raw&0x800000U)<<8;
    auto exponent=(raw>>16)&127U, mantissa=raw&65535U;
    if(exponent==127) return sign|0x7F800000U|(mantissa<<7);
    if(!exponent) {
        if(!mantissa) return sign;
        exponent=65;
        while(!(mantissa&0x10000U)){--exponent;mantissa<<=1;}
        return sign|(exponent<<23)|((mantissa&65535U)<<7);
    }
    return sign|((exponent+64)<<23)|(mantissa<<7);
}
bool AnyByteMatch(std::uint32_t a,std::uint32_t b) noexcept {
    for(unsigned i=0;i<4;++i) if(((a^b)&(0xFFU<<(i*8)))==0) return true;
    return false;
}
bool In(std::uint32_t x,std::uint32_t lo,std::uint32_t hi) noexcept { return x>=lo&&x<=hi; }
// Batch handling for these special registers differs between the reference's
// optimized and debug paths. This startup slice rejects repeated batches here,
// rather than silently choosing a policy for unobserved ambiguous cases.
bool NonUploadSpecial(std::uint32_t id) noexcept {
    return id==0x10 || id==0x25E || id==0x25F || id==0x280 || id==0x2B0 ||
           id==0x2BD || In(id,0x281,0x284) || In(id,0x2B1,0x2B4);
}
struct Executor {
    PicaListPlan& p;
    bool Fail(const char* error) noexcept {p.result.error=error;return false;}
    std::uint32_t& Reg(std::uint32_t id) noexcept {return p.registers[0x400+id];}
    bool MirrorVs() noexcept {return !(Reg(0x244)&1U) && (Reg(0x229)&3U)==0;}
    bool FloatWord(bool vs,std::uint32_t raw) noexcept {
        auto& shader=vs?p.uploads.vs:p.uploads.gs;
        auto& config=Reg(vs?0x2C0:0x290);
        const bool fp32=(config>>31)!=0;
        if(shader.packed_count>=4) return Fail("PICA packed-uniform queue exceeds capacity");
        shader.packed[shader.packed_count++]=raw;
        if(shader.packed_count<(fp32?4U:3U))return true;
        const auto index=config&127U;
        if(index>=96) return Fail("PICA uniform index outside supported 96 vectors");
        auto& dst=shader.floats[index];const auto& b=shader.packed;
        if(fp32) dst={b[3],b[2],b[1],b[0]};
        else dst={Float24Bits(b[2]&0xFFFFFFU),Float24Bits(((b[1]&65535)<<8)|(b[2]>>24)),
                  Float24Bits(((b[0]&255)<<16)|(b[1]>>16)),Float24Bits(b[0]>>8)};
        shader.packed_count=0;shader.floats_written.set(index);
        config=(config&~127U)|((index+1)&127U);
        if(vs&&MirrorVs()) {p.uploads.gs.floats[index]=dst;p.uploads.gs.floats_written.set(index);}
        ++p.result.uniform_vectors;return true;
    }
    bool Write(std::uint32_t id,std::uint32_t value,std::uint32_t mask) noexcept {
        p.result.register_id=id;
        if(id>=kPicaInternalWords) return Fail("PICA register outside supported internal bank");
        // These have genuine side effects even when data/mask is zero. Reject the
        // complete plan, not just the action while pretending its store succeeded.
        if(id==0x22E||id==0x22F) return Fail("PICA draw execution is unimplemented");
        if(id==0x23C||id==0x23D) return Fail("PICA command-list chaining is unimplemented");
        if(In(id,0x232,0x235))return Fail("PICA default/immediate attributes are unimplemented");
        if(In(id,0x2A6,0x2AD)||In(id,0x2D6,0x2DD))return Fail("PICA swizzle upload is unimplemented");
        if(In(id,0xE8,0xEF)||In(id,0xB0,0xB7))return Fail("PICA fog/procedural lookup upload is unimplemented");
        auto& word=Reg(id);const auto expanded=ByteMask(mask);
        word=(word&~expanded)|(value&expanded);
        p.uploads.registers_written.set(id);++p.result.writes;
        if(id==0x10) {
            // Pinned HLE's any-byte comparator, not claimed hardware IRQ parity.
            if(AnyByteMatch(word,Reg(0x20))) {
                ++p.result.irqs;
                p.result.autostopped=Reg(0x34)!=0;
            }
        } else if(id==0x25E) {
            p.uploads.topology=(word>>8)&3U; // Reconfigure an empty assembler.
        } else if(id==0x25F) {
            // No draw/vertex submission is supported; assembler remains empty.
        } else if(id==0x280||id==0x2B0) {
            auto& shader=id==0x280?p.uploads.gs:p.uploads.vs;
            shader.booleans=static_cast<std::uint16_t>(word);shader.booleans_written=true;
            if(id==0x2B0&&MirrorVs()) {p.uploads.gs.booleans=shader.booleans;p.uploads.gs.booleans_written=true;}
        } else if(In(id,0x281,0x284)||In(id,0x2B1,0x2B4)) {
            const bool vs=id>=0x2B1;const auto index=id-(vs?0x2B1:0x281);
            auto& shader=vs?p.uploads.vs:p.uploads.gs;
            shader.integers[index]=word;shader.integers_written.set(index);
            if(vs&&MirrorVs()) {p.uploads.gs.integers[index]=word;p.uploads.gs.integers_written.set(index);}
        } else if(id==0x2BD) {
            // Reference uses the raw argument here, not the merged word.
            if(MirrorVs())Reg(0x28D)=(Reg(0x28D)&0xFFFF0000U)|(value&65535U);
        } else if(In(id,0x291,0x298)||In(id,0x2C1,0x2C8)) {
            return FloatWord(id>=0x2C1,value); // Upload ports consume raw data.
        } else if(In(id,0x29C,0x2A3)||In(id,0x2CC,0x2D3)) {
            const bool vs=id>=0x2CC;auto& offset=Reg(vs?0x2CB:0x29B);
            if(offset>=(vs?512U:4096U))return Fail("PICA shader program upload outside capacity");
            auto& shader=vs?p.uploads.vs:p.uploads.gs;
            shader.program[offset]=value;shader.program_written.set(offset);
            if(vs&&MirrorVs()) {p.uploads.gs.program[offset]=value;p.uploads.gs.program_written.set(offset);}
            ++offset;++p.result.program_words;
        } else if(In(id,0x1C8,0x1CF)) {
            auto& config=Reg(0x1C5);const auto type=(config>>8)&31U,index=config&255U;
            if(type>=24)return Fail("PICA lighting LUT type outside capacity");
            p.uploads.lighting[type][index]=value;p.uploads.lighting_written[type].set(index);
            config=(config&~255U)|((index+1)&255U);++p.result.lut_words;
        }
        return true;
    }
};
} // namespace
bool StagePicaStartupList(std::span<const std::uint8_t> bytes,
                          const PicaGpuRegisters& initial,const PicaUploadState& uploads,
                          PicaListPlan& plan) noexcept {
    plan.registers=initial;plan.uploads=uploads;plan.result={};Executor ex{plan};
    if(bytes.size()>kPicaMaxListBytes || (bytes.size()&7U))
        return ex.Fail("PICA list size violates supported aligned host bounds");
    std::size_t offset=0;
    while(offset<bytes.size()) {
        plan.result.byte_offset=static_cast<std::uint32_t>(offset);
        if(bytes.size()-offset<8)return ex.Fail("PICA list has a truncated command header");
        const auto value=Word(bytes,offset),header=Word(bytes,offset+4);
        const auto id=header&65535U,mask=(header>>16)&15U,extra=(header>>20)&255U;
        const bool grouped=(header>>31)!=0;
        plan.result.register_id=id;
        const auto packet_bytes=8U+extra*4U;
        if(packet_bytes>bytes.size()-offset)return ex.Fail("PICA list has truncated extra words");
        if(!grouped&&extra&&NonUploadSpecial(id))
            return ex.Fail("PICA repeated non-upload special register is unsupported");
        ++plan.result.packets;
        if(!ex.Write(id,value,mask))return false;
        if(plan.result.autostopped)break;
        for(std::uint32_t n=0;n<extra;++n) {
            plan.result.byte_offset=static_cast<std::uint32_t>(offset+8+n*4);
            if(!ex.Write(id+(grouped?n+1:0),Word(bytes,offset+8+n*4),mask))return false;
            if(plan.result.autostopped)break;
        }
        if(plan.result.autostopped)break;
        // Each header/value starts at an 8-byte boundary. An odd extra count
        // leaves a padding word, whose value is irrelevant to command semantics.
        offset+=(packet_bytes+7U)&~7U;
    }
    return true;
}
} // namespace lego::ctr
