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

Color Texel(const PicaTexturePlan& plan,std::uint32_t x,std::uint32_t y) noexcept {
    // Clamp/wrap acts on logical coordinates before the storage Y inversion.
    const auto stored_y=plan.height-1-y;
    const auto tile=(stored_y/8)*(plan.width/8)+x/8;
    std::uint32_t morton{};
    for (unsigned bit=0;bit<3;++bit) {
        morton|=((x>>bit)&1U)<<(2*bit);
        morton|=((stored_y>>bit)&1U)<<(2*bit+1);
    }
    const auto offset=tile*256+4*morton;
    return {plan.bytes[offset+3],plan.bytes[offset+2],plan.bytes[offset+1],plan.bytes[offset]};
}

struct Axis {
    std::uint32_t first{},second{};
    float fraction{};
};
Axis LinearAxis(float normalized,std::uint32_t size) noexcept {
    // For clamp-to-edge, clamping normalized coordinates first is equivalent
    // outside [0,1] and prevents large finite inputs from overflowing the scale
    // or integer conversion. No repeat/border semantics are inferred here.
    const auto position=Add(Mul(std::clamp(normalized,0.0f,1.0f),static_cast<float>(size)),-0.5f);
    const auto lower=std::floor(position);
    const auto integer=static_cast<std::int32_t>(lower); // Proven [-1,1023].
    const auto upper=static_cast<std::int32_t>(size-1);
    return {static_cast<std::uint32_t>(std::clamp(integer,0,upper)),
            static_cast<std::uint32_t>(std::clamp(integer+1,0,upper)),Add(position,-lower)};
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
    if ((reg(0x83)&0x71007700U) || (reg(0x84)&0x0F0F1FFFU) || (reg(0x8E)&15U))
        return fail("PICA texture0 requires ordinary RGBA8, base level0 and clamp-to-edge");
    const auto magnify=(reg(0x83)>>1)&1U,minify=(reg(0x83)>>2)&1U;
    if (magnify!=minify)
        return fail("PICA texture0 requires identical minification and magnification filters");

    PicaTexturePlan staged;
    staged.width=(reg(0x82)>>16)&0x7FFU;staged.height=reg(0x82)&0x7FFU;
    if (!Dimensions(staged.width,staged.height))
        return fail("PICA texture0 dimensions are outside supported power-of-two bounds");
    const auto bytes=std::uint64_t(staged.width)*staged.height*4;
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
    const auto bytes=std::uint64_t(plan.width)*plan.height*4;
    if (bytes>kPicaTextureMaxBytes || plan.bytes.size()!=bytes || plan.read.bytes!=bytes ||
        (plan.filter!=PicaTextureFilter::Nearest && plan.filter!=PicaTextureFilter::Linear))
        return fail("PICA texture sample has an invalid staged extent or filter");
    if (!NormalOrZero(u) || !NormalOrZero(v) || std::fegetround()!=FE_TONEAREST)
        return fail("PICA texture sample requires finite normal/zero coordinates and nearest host rounding");
    Color sampled{};
    if (plan.filter==PicaTextureFilter::Nearest) {
        const auto x=static_cast<std::uint32_t>(Mul(std::clamp(u,0.0f,1.0f),static_cast<float>(plan.width)));
        const auto y=static_cast<std::uint32_t>(Mul(std::clamp(v,0.0f,1.0f),static_cast<float>(plan.height)));
        sampled=Texel(plan,std::min(x,plan.width-1),std::min(y,plan.height-1));
    } else {
        const auto x=LinearAxis(u,plan.width),y=LinearAxis(v,plan.height);
        const auto a=Texel(plan,x.first,y.first),b=Texel(plan,x.second,y.first);
        const auto c=Texel(plan,x.first,y.second),d=Texel(plan,x.second,y.second);
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
