#include "services/gsp_display_transfer.h"
#include <cstring>
#include <utility>

namespace lego::ctr {
std::shared_ptr<GpuVramBank> GpuVramBank::ReferenceZero() {
    return std::shared_ptr<GpuVramBank>(new GpuVramBank);
}

bool GpuVramBank::Write(std::uint32_t offset, std::span<const std::uint8_t> data) noexcept {
    if (offset > bytes_.size() || data.size() > bytes_.size()-offset) return false;
    if (!data.empty()) std::memmove(bytes_.data()+offset, data.data(), data.size());
    return true;
}

bool StageDisplayTransfer(const DisplayTransferRequest& request, const GpuVramBank* vram,
                          DisplayTransferPlan& plan, const char*& error) {
    error = nullptr;
    const auto fail = [&](const char* message) { error=message; return false; };
    if (!vram) return fail("DisplayTransfer requires an explicitly configured VRAM bank");
    const bool scaled=request.flags==0x01001004U;
    if (!scaled && request.flags!=0x00004400U)
        return fail("DisplayTransfer format/layout/scaling flags are unsupported");
    const auto input_width=request.input_size&0xFFFFU, input_height=request.input_size>>16;
    const auto programmed_width=request.output_size&0xFFFFU, programmed_height=request.output_size>>16;
    if (!input_width || !input_height || (input_width&7U) || (input_height&7U))
        return fail("DisplayTransfer input requires nonzero whole-tile dimensions");
    if (!scaled && request.input_size!=request.output_size)
        return fail("DisplayTransfer RGBA4 requires equal dimensions");
    if (scaled && (!programmed_width || !programmed_height || (programmed_width&1U) ||
                   programmed_width>input_width || programmed_height>input_height))
        return fail("DisplayTransfer scaled crop is outside the input or has odd width");
    const auto width=scaled ? programmed_width/2U : input_width;
    const auto height=scaled ? programmed_height : input_height;
    const std::uint64_t input_bytes=std::uint64_t(input_width)*input_height*(scaled?4U:2U);
    const std::uint64_t bytes=std::uint64_t(width)*height*(scaled?3U:2U);
    if (input_bytes>kDisplayTransferMaxBytes || bytes>kDisplayTransferMaxBytes)
        return fail("DisplayTransfer exceeds the host staging bound");
    if ((request.input&7U) || request.input<kGpuVramVirtualBase ||
        std::uint64_t(request.input)+input_bytes>std::uint64_t(kGpuVramVirtualBase)+kGpuVramBytes)
        return fail("DisplayTransfer input is outside supported aligned physical VRAM");
    if (scaled) {
        if ((request.output&7U) || request.output<kGpuVramVirtualBase ||
            std::uint64_t(request.output)+bytes>std::uint64_t(kGpuVramVirtualBase)+kGpuVramBytes)
            return fail("DisplayTransfer scaled output is outside aligned device VRAM");
        if (std::uint64_t(request.input)<std::uint64_t(request.output)+bytes &&
            std::uint64_t(request.output)<std::uint64_t(request.input)+input_bytes)
            return fail("DisplayTransfer input/output device spans overlap");
    } else if ((request.output&7U) || request.output<0x14000000U ||
               std::uint64_t(request.output)+bytes>0x1C000000ULL) {
        return fail("DisplayTransfer output is outside supported aligned linear heap");
    }

    DisplayTransferPlan staged;
    staged.request=request; staged.width=width; staged.height=height;
    staged.bytes=static_cast<std::uint32_t>(bytes);
    staged.input_bytes=static_cast<std::uint32_t>(input_bytes);
    staged.output_vram=scaled; staged.output.resize(staged.bytes);
    const auto source=vram->bytes().subspan(request.input-kGpuVramVirtualBase,staged.input_bytes);
    constexpr std::uint32_t xbits[]{0,1,4,5,16,17,20,21};
    constexpr std::uint32_t ybits[]{0,2,8,10,32,34,40,42};
    for (std::uint32_t y=0;y<height;++y) {
        for (std::uint32_t x=0;x<width;++x) {
            const auto input_x=scaled ? 2U*x : x;
            const auto input_pixel=(y&~7U)*input_width+(input_x&~7U)*8+xbits[input_x&7U]+ybits[y&7U];
            const auto output_pixel=y*width+x;
            if (scaled) {
                // Pinned sw_blitter ScaleX averages adjacent Morton words. With
                // even input_x they are exactly the horizontal pair, including
                // tile boundaries. Vec4<u8> addition promotes to int: no byte wrap.
                // RGBA8 bytes are A,B,G,R; RGB8 bytes are B,G,R. Drop alpha only.
                // The crop flag needs no extra offset without vertical flip.
                for (std::uint32_t c=0;c<3;++c) {
                    const unsigned sum=unsigned(source[4*input_pixel+1+c])+
                                       unsigned(source[4*(input_pixel+1)+1+c]);
                    staged.output[3*output_pixel+c]=static_cast<std::uint8_t>(sum/2U);
                }
            } else {
                // RGBA4 nibble expansion/re-encoding preserves the 16-bit pixel.
                staged.output[2*output_pixel]=source[2*input_pixel];
                staged.output[2*output_pixel+1]=source[2*input_pixel+1];
            }
        }
    }
    plan=std::move(staged);
    return true;
}
} // namespace lego::ctr
