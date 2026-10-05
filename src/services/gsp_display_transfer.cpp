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
    if (request.flags != 0x00004400U)
        return fail("DisplayTransfer format/layout/scaling flags are unsupported");
    const auto width=request.input_size&0xFFFFU, height=request.input_size>>16;
    if (!width || !height || (width&7U) || (height&7U) || request.input_size!=request.output_size)
        return fail("DisplayTransfer requires equal nonzero whole-tile dimensions");
    const std::uint64_t bytes=std::uint64_t(width)*height*2U;
    if (bytes>kDisplayTransferMaxBytes)
        return fail("DisplayTransfer exceeds the host staging bound");
    if ((request.input&7U) || request.input<kGpuVramVirtualBase ||
        std::uint64_t(request.input)+bytes>std::uint64_t(kGpuVramVirtualBase)+kGpuVramBytes)
        return fail("DisplayTransfer input is outside supported aligned physical VRAM");
    if ((request.output&7U) || request.output<0x14000000U ||
        std::uint64_t(request.output)+bytes>0x1C000000ULL)
        return fail("DisplayTransfer output is outside supported aligned linear heap");

    DisplayTransferPlan staged;
    staged.request=request; staged.width=width; staged.height=height;
    staged.bytes=static_cast<std::uint32_t>(bytes); staged.output.resize(staged.bytes);
    const auto source=vram->bytes().subspan(request.input-kGpuVramVirtualBase,staged.bytes);
    // Same RGBA4 input/output: nibble expansion and re-encoding are exactly an
    // identity on each 16-bit pixel. Only its address changes. The table matches
    // the pinned VideoCore::MortonInterleave/GetMortonOffset (no vertical flip).
    constexpr std::uint32_t xbits[]{0,1,4,5,16,17,20,21};
    constexpr std::uint32_t ybits[]{0,2,8,10,32,34,40,42};
    for (std::uint32_t y=0;y<height;++y) {
        for (std::uint32_t x=0;x<width;++x) {
            const auto input_pixel=(y&~7U)*width+(x&~7U)*8+xbits[x&7U]+ybits[y&7U];
            const auto output_pixel=y*width+x;
            staged.output[2*output_pixel]=source[2*input_pixel];
            staged.output[2*output_pixel+1]=source[2*input_pixel+1];
        }
    }
    plan=std::move(staged);
    return true;
}
} // namespace lego::ctr
