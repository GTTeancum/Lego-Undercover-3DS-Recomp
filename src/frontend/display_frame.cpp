#include "frontend/display_frame.h"
#include "services/gsp_gpu_service.h"

#include <algorithm>
#include <utility>

namespace lego::frontend {

bool DualScreenFrame::HasNonBlackPixels() const noexcept {
    for (const auto* screen : {&top,&bottom})
        for (std::size_t i=0; i+3<screen->rgba.size(); i+=4)
            if (screen->rgba[i] || screen->rgba[i+1] || screen->rgba[i+2]) return true;
    return false;
}

bool DecodeRgb8Scanout(std::span<const std::uint8_t> vram,
                      const ScanoutDescriptor& descriptor, int screen_width,
                      ScreenImage& image, std::string& error) {
    error.clear();
    const auto fail=[&](const char* message) { error=message; return false; };
    if (screen_width!=400 && screen_width!=320)
        return fail("unsupported LCD dimensions");
    if (descriptor.lcd_color_fill!=0 && descriptor.lcd_color_fill!=0x01000000U)
        return fail("unsupported LCD color-fill state");
    ScreenImage staged{screen_width,240,{}};
    if (descriptor.lcd_color_fill==0x01000000U) {
        staged.rgba.resize(static_cast<std::size_t>(screen_width)*240*4,0);
        for (std::size_t i=3;i<staged.rgba.size();i+=4) staged.rgba[i]=255;
        image=std::move(staged);
        return true;
    }
    const auto raw_width=descriptor.size_word&0xFFFFU;
    const auto raw_height=descriptor.size_word>>16;
    if (raw_width!=240 || raw_height!=static_cast<std::uint32_t>(screen_width))
        return fail("unsupported rotated LCD scanout geometry");
    if ((descriptor.format_word&7U)!=1)
        return fail("desktop scanout currently requires RGB8 buffers");
    if (descriptor.active_buffer>1)
        return fail("unsupported active LCD buffer selector");
    if (descriptor.stride<raw_width*3)
        return fail("LCD scanout stride is shorter than a RGB8 row");
    const auto address=descriptor.addresses[descriptor.active_buffer];
    if (address<ctr::kGpuVramPhysicalBase)
        return fail("LCD scanout does not point into device VRAM");
    const auto offset=std::uint64_t(address)-ctr::kGpuVramPhysicalBase;
    const auto bytes=std::uint64_t(descriptor.stride)*raw_height;
    if (offset>vram.size() || bytes>vram.size()-offset)
        return fail("LCD scanout extends beyond configured device VRAM");
    staged.rgba.resize(static_cast<std::size_t>(screen_width)*240*4);
    // RGB8 is stored B,G,R. LCD rows are rotated: landscape(x,y) reads
    // scanout row x, column 239-y. No resampling or guest GPU replay occurs.
    for (int y=0;y<240;++y) for (int x=0;x<screen_width;++x) {
        const auto source=static_cast<std::size_t>(offset)+
            static_cast<std::size_t>(x)*descriptor.stride+static_cast<std::size_t>(239-y)*3;
        const auto target=(static_cast<std::size_t>(y)*screen_width+x)*4;
        staged.rgba[target]=vram[source+2];
        staged.rgba[target+1]=vram[source+1];
        staged.rgba[target+2]=vram[source];
        staged.rgba[target+3]=255;
    }
    image=std::move(staged);
    return true;
}

bool ReadDisplayFrame(const ctr::GspGpuService& gpu, DualScreenFrame& frame,
                      std::string& error) {
    DualScreenFrame staged;
    const auto bank=gpu.vram_bank();
    const auto bytes=bank ? bank->bytes() : std::span<const std::uint8_t>{};
    for (unsigned screen=0;screen<2;++screen) {
        const auto base=0x100U+0x40U*screen;
        auto word=[&](std::uint32_t index) {return gpu.register_word(0x400000U+4*index);};
        const auto size=word(base+0x17),format=word(base+0x1C),stride=word(base+0x24);
        const auto active=word(base+0x1E),first=word(base+0x1A),second=word(base+0x1B);
        const auto fill=gpu.lcd_color_fill_word(screen);
        if (!size || !format || !stride || !active || !first || !second || !fill) {
            error="LCD scanout registers are unavailable";return false;
        }
        const ScanoutDescriptor descriptor{*size,*format,*stride,*active,{*first,*second},*fill};
        if (!DecodeRgb8Scanout(bytes,descriptor,screen ? 320 : 400,
                               screen ? staged.bottom : staged.top,error)) return false;
    }
    frame=std::move(staged);
    return true;
}

} // namespace lego::frontend
