#include "services/pica_texture.h"
#include "services/gsp_display_transfer.h"
#include "runtime/ctr_memory.h"
#include <algorithm>
#include <bit>
#include <cfenv>
#include <cmath>
#include <limits>
#include <utility>

namespace lego::ctr {
namespace {
// Independently authored bounded sampler. Layout/register references are Azahar
// 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac, regs_texturing.h, texture_decode.cpp,
// sw_rasterizer.cpp and the OpenGL sampler/fragment-shader path. Its accelerated
// renderer selects host linear filtering; exact PICA filter precision is not
// established. This implementation deliberately specifies binary32 evaluation
// and rounds the resulting nonnegative byte color with halfway values upward.
static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559);
using Color=std::array<std::uint8_t,4>;

bool Known(const PicaUploadState& uploads,unsigned id,std::uint32_t bits) {
    for (unsigned byte=0;byte<4;++byte)
        if ((bits&(0xFFU<<(8*byte))) && !(uploads.register_known_bytes[id]&(1U<<byte)))
            return false;
    return true;
}
bool Dimensions(std::uint32_t width,std::uint32_t height) noexcept {
    return width>=8 && width<=1024 && std::has_single_bit(width) &&
           height>=8 && height<=1024 && std::has_single_bit(height);
}
bool NormalOrZero(float value) noexcept {
    const auto bits=std::bit_cast<std::uint32_t>(value),exponent=bits&0x7F800000U;
    return exponent!=0x7F800000U && (exponent || !(bits&0x007FFFFFU));
}
float Mul(float a,float b) noexcept {volatile float out=a*b;return out;}
float Add(float a,float b) noexcept {volatile float out=a+b;return out;}
std::uint32_t TileBytes(PicaTextureFormat format) noexcept {
    switch (format) {
    case PicaTextureFormat::RGBA8: return 256;
    case PicaTextureFormat::RGB8: return 192;
    case PicaTextureFormat::IA8: return 128;
    case PicaTextureFormat::ETC1: return 32;
    case PicaTextureFormat::ETC1A4: return 64;
    }
    return 0;
}

std::uint64_t Little64(const std::uint8_t* source) noexcept {
    std::uint64_t value{};
    for (unsigned byte=0;byte<8;++byte) value|=std::uint64_t(source[byte])<<(8*byte);
    return value;
}

bool DecodeEtc1(std::uint64_t block,unsigned x,unsigned y,Color& color) noexcept {
    // Independently authored from the ETC1 bit layout. PICA stores the word
    // little-endian (unlike the Khronos upload byte stream). Flip changes the
    // partition axis only; selectors and ETC1A4 alpha remain column-major.
    constexpr std::array<std::array<int,2>,8> modifiers{{
        {2,8},{5,17},{9,29},{13,42},{18,60},{24,80},{33,106},{47,183}}};
    const bool second=((block>>32)&1U)?y>=2:x>=2;
    const bool differential=((block>>33)&1U)!=0;
    std::array<int,3> base{};
    for (unsigned channel=0;channel<3;++channel) {
        if (differential) {
            const auto first=static_cast<int>((block>>(59-8*channel))&31U);
            const auto encoded_delta=static_cast<int>((block>>(56-8*channel))&7U);
            const auto delta=encoded_delta<4?encoded_delta:encoded_delta-8;
            const auto next=first+delta;
            // ETC1 defines the whole block as undefined when any differential
            // endpoint leaves [0,31]. Do not wrap it or infer an ETC2 mode.
            if (next<0 || next>31) return false;
            const auto value=second?next:first;
            base[channel]=(value<<3)|(value>>2);
        } else {
            const auto shift=(second?56U:60U)-8*channel;
            base[channel]=static_cast<int>((block>>shift)&15U)*17;
        }
    }
    const auto index=4*x+y;
    const auto table=static_cast<unsigned>((block>>(second?34U:37U))&7U);
    auto modifier=modifiers[table][(block>>index)&1U];
    if ((block>>(index+16))&1U) modifier=-modifier;
    for (unsigned channel=0;channel<3;++channel)
        color[channel]=static_cast<std::uint8_t>(std::clamp(base[channel]+modifier,0,255));
    return true;
}

bool Texel(const PicaTexturePlan& plan,std::uint32_t x,std::uint32_t y,Color& color) noexcept {
    // Clamp/wrap acts on logical coordinates before the storage Y inversion.
    const auto stored_y=plan.height-1-y;
    const auto tile=(stored_y/8)*(plan.width/8)+x/8;
    const auto tile_bytes=TileBytes(plan.format);
    if (plan.format==PicaTextureFormat::ETC1 || plan.format==PicaTextureFormat::ETC1A4) {
        const bool has_alpha=plan.format==PicaTextureFormat::ETC1A4;
        const auto subblock=(x%8)/4+2*((stored_y%8)/4);
        const auto offset=tile*tile_bytes+subblock*(has_alpha?16U:8U);
        const auto px=x%4,py=stored_y%4;
        Color decoded{};
        if (!DecodeEtc1(Little64(plan.bytes.data()+offset+(has_alpha?8:0)),px,py,decoded))
            return false;
        decoded[3]=has_alpha?static_cast<std::uint8_t>(
            ((Little64(plan.bytes.data()+offset)>>(4*(4*px+py)))&15U)*17):255;
        color=decoded;
        return true;
    }
    std::uint32_t morton{};
    for (unsigned bit=0;bit<3;++bit) {
        morton|=((x>>bit)&1U)<<(2*bit);
        morton|=((stored_y>>bit)&1U)<<(2*bit+1);
    }
    const auto offset=tile*tile_bytes+morton*(tile_bytes/64);
    if (plan.format==PicaTextureFormat::IA8) {
        const auto intensity=plan.bytes[offset+1];
        color={intensity,intensity,intensity,plan.bytes[offset]};
    } else if (plan.format==PicaTextureFormat::RGB8) {
        color={plan.bytes[offset+2],plan.bytes[offset+1],plan.bytes[offset],255};
    } else {
        color={plan.bytes[offset+3],plan.bytes[offset+2],plan.bytes[offset+1],plan.bytes[offset]};
    }
    return true;
}

struct Axis {
    std::uint32_t first{},second{};
    float fraction{};
};
bool SupportedWrap(PicaTextureWrap mode) noexcept {
    return mode==PicaTextureWrap::ClampToEdge || mode==PicaTextureWrap::Repeat;
}
float BoundedCoordinate(float normalized,PicaTextureWrap mode) noexcept {
    // Preserve the signed remainder: adding one to a tiny negative value can
    // round to 1 and incorrectly select texel zero in nearest filtering.
    // A power-of-two scale is exact here, and all integer conversions stay
    // bounded even when the original finite coordinate is enormous.
    return mode==PicaTextureWrap::Repeat?std::fmod(normalized,1.0f):std::clamp(normalized,0.0f,1.0f);
}
std::uint32_t WrapTexel(std::int32_t coordinate,std::uint32_t size,PicaTextureWrap mode) noexcept {
    const auto extent=static_cast<std::int32_t>(size);
    if (mode==PicaTextureWrap::ClampToEdge)
        return static_cast<std::uint32_t>(std::clamp(coordinate,0,extent-1));
    const auto remainder=coordinate%extent;
    return static_cast<std::uint32_t>(remainder<0?remainder+extent:remainder);
}
std::uint32_t NearestAxis(float normalized,std::uint32_t size,PicaTextureWrap mode) noexcept {
    const auto position=Mul(BoundedCoordinate(normalized,mode),static_cast<float>(size));
    return WrapTexel(static_cast<std::int32_t>(std::floor(position)),size,mode);
}
Axis LinearAxis(float normalized,std::uint32_t size,PicaTextureWrap mode) noexcept {
    const auto position=Add(Mul(BoundedCoordinate(normalized,mode),static_cast<float>(size)),-0.5f);
    const auto lower=std::floor(position);
    const auto integer=static_cast<std::int32_t>(lower); // Proven [-1025,1023].
    return {WrapTexel(integer,size,mode),WrapTexel(integer+1,size,mode),Add(position,-lower)};
}
float Lerp(float left,float right,float fraction) noexcept {
    const auto left_weight=Add(1,-fraction);
    return Add(Mul(left,left_weight),Mul(right,fraction));
}
} // namespace

bool StagePicaTexture0(const PicaGpuRegisters& registers,const PicaUploadState& uploads,
                       GuestMemory* memory,const GpuVramBank& vram,
                       PicaTexturePlan& plan,const char*& error) {
    error=nullptr;
    const auto fail=[&](const char* message){error=message;return false;};
    const auto reg=[&](unsigned id){return registers[0x400U+id];};
    if (!Known(uploads,0x82,0x07FF07FFU) || !Known(uploads,0x83,0x71007706U) ||
        !Known(uploads,0x84,0x0F0F1FFFU) || !Known(uploads,0x85,0x0FFFFFFFU) ||
        !Known(uploads,0x8E,15))
        return fail("PICA texture0 consumed register state is unknown");
    if ((reg(0x83)&0x70000000U) || (reg(0x84)&0x0F0F1FFFU))
        return fail("PICA texture0 requires ordinary 2D, base level0 and zero LOD bias");
    const auto wrap_s=static_cast<PicaTextureWrap>((reg(0x83)>>12)&7U);
    const auto wrap_t=static_cast<PicaTextureWrap>((reg(0x83)>>8)&7U);
    if (!SupportedWrap(wrap_s) || !SupportedWrap(wrap_t))
        return fail("PICA texture0 requires clamp-to-edge or repeat wrapping");
    const auto format=static_cast<PicaTextureFormat>(reg(0x8E)&15U);
    const auto tile_bytes=TileBytes(format);
    if (!tile_bytes)
        return fail("PICA texture0 format is outside the supported RGBA8/RGB8/IA8/ETC1/ETC1A4 slice");
    const auto magnify=(reg(0x83)>>1)&1U,minify=(reg(0x83)>>2)&1U;
    if (magnify!=minify)
        return fail("PICA texture0 requires identical minification and magnification filters");
    // Pinned Azahar forwards min/max LOD to its host sampler. With both zero
    // and equal min/mag filters, either mip-filter bit selects the same base
    // samples (OpenGL3.3 sections3.8.11-12). This does not enable mipmap storage.

    PicaTexturePlan staged;
    staged.format=format;staged.wrap_s=wrap_s;staged.wrap_t=wrap_t;
    staged.width=(reg(0x82)>>16)&0x7FFU;staged.height=reg(0x82)&0x7FFU;
    if (!Dimensions(staged.width,staged.height))
        return fail("PICA texture0 dimensions are outside supported power-of-two bounds");
    const auto bytes=std::uint64_t(staged.width/8)*(staged.height/8)*tile_bytes;
    if (bytes>kPicaTextureMaxBytes)
        return fail("PICA texture0 exceeds the host byte bound");
    const auto physical=(reg(0x85)&0x0FFFFFFFU)*8U;
    const auto end=std::uint64_t(physical)+bytes;
    const auto count=static_cast<std::uint32_t>(bytes);
    std::uint32_t virtual_address{};
    const bool fcram=physical>=0x20000000U && end<=0x28000000ULL;
    const bool device=physical>=kGpuVramPhysicalBase &&
                      end<=std::uint64_t(kGpuVramPhysicalBase)+kGpuVramBytes;
    if (fcram) {
        virtual_address=physical-0x20000000U+0x14000000U;
        // Same preflight as vertex ReadPhysical: no shared or device callbacks,
        // and the entire source must be readable within one private backing.
        if (!memory || !memory->IsCachelessPrivateRange(virtual_address,count))
            return fail("PICA texture0 lacks complete readable private FCRAM backing");
    } else if (!device) {
        return fail("PICA texture0 is outside supported physical FCRAM/device VRAM");
    }
    staged.filter=magnify?PicaTextureFilter::Linear:PicaTextureFilter::Nearest;
    staged.read={physical,count};staged.bytes.resize(count);
    if (fcram) {
        for (std::uint32_t i=0;i<count;++i)
            if (!memory->Read8(virtual_address+i,&staged.bytes[i]))
                return fail("PICA texture0 private FCRAM snapshot read failed");
    } else {
        const auto source=vram.bytes().subspan(physical-kGpuVramPhysicalBase,count);
        std::copy(source.begin(),source.end(),staged.bytes.begin());
    }
    plan=std::move(staged);
    return true;
}

bool SamplePicaTexture(const PicaTexturePlan& plan,float u,float v,
                       Color& color,const char*& error) noexcept {
    error=nullptr;
    const auto fail=[&](const char* message){error=message;return false;};
    if (!Dimensions(plan.width,plan.height))
        return fail("PICA texture sample has invalid staged dimensions");
    const auto tile_bytes=TileBytes(plan.format);
    const auto bytes=std::uint64_t(plan.width/8)*(plan.height/8)*tile_bytes;
    if (!tile_bytes || bytes>kPicaTextureMaxBytes || plan.bytes.size()!=bytes || plan.read.bytes!=bytes ||
        (plan.filter!=PicaTextureFilter::Nearest && plan.filter!=PicaTextureFilter::Linear) ||
        !SupportedWrap(plan.wrap_s) || !SupportedWrap(plan.wrap_t))
        return fail("PICA texture sample has an invalid staged format, extent, filter or wrap mode");
    if (!NormalOrZero(u) || !NormalOrZero(v) || std::fegetround()!=FE_TONEAREST)
        return fail("PICA texture sample requires finite normal/zero coordinates and nearest host rounding");
    const auto tap=[&](std::uint32_t x,std::uint32_t y,Color& value) {
        if (!Texel(plan,x,y,value)) return fail("PICA ETC1 block has undefined differential endpoints");
        return true;
    };
    Color sampled{};
    if (plan.filter==PicaTextureFilter::Nearest) {
        const auto x=NearestAxis(u,plan.width,plan.wrap_s),y=NearestAxis(v,plan.height,plan.wrap_t);
        if (!tap(x,y,sampled)) return false;
    } else {
        const auto x=LinearAxis(u,plan.width,plan.wrap_s),y=LinearAxis(v,plan.height,plan.wrap_t);
        Color a{},b{},c{},d{};
        if (!tap(x.first,y.first,a) || !tap(x.second,y.first,b) ||
            !tap(x.first,y.second,c) || !tap(x.second,y.second,d)) return false;
        for (unsigned channel=0;channel<4;++channel) {
            const auto low=Lerp(static_cast<float>(a[channel]),static_cast<float>(b[channel]),x.fraction);
            const auto high=Lerp(static_cast<float>(c[channel]),static_cast<float>(d[channel]),x.fraction);
            const auto mixed=Lerp(low,high,y.fraction);
            if (!std::isfinite(mixed) || mixed<0 || mixed>255)
                return fail("PICA texture linear interpolation exceeds the byte color range");
            sampled[channel]=static_cast<std::uint8_t>(std::round(mixed));
        }
    }
    color=sampled;
    return true;
}
} // namespace lego::ctr
