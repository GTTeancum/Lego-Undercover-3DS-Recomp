#include "services/gsp_memory_fill.h"
#include <utility>

namespace lego::ctr {
bool StageMemoryFill(const MemoryFillRequest& request, const GpuVramBank* vram,
                     const PicaGpuRegisters& registers, MemoryFillPlan& plan,
                     const char*& error) {
    error=nullptr;
    const auto fail=[&](const char* message) { error=message; return false; };
    MemoryFillPlan staged;
    staged.registers=registers;
    const bool both=request.channels[0].start!=0 && request.channels[1].start!=0;
    for (unsigned i=0;i<2;++i) {
        const auto& input=request.channels[i];
        if (input.start==0) continue; // GPU::Execute ignores this channel completely.
        if (input.control&~0x0303U)
            return fail("MemoryFill control bits outside the supported field set");
        if ((input.start&7U) || (input.end&7U) || input.start<kGpuVramVirtualBase ||
            input.end<=input.start || std::uint64_t(input.end)>std::uint64_t(kGpuVramVirtualBase)+kGpuVramBytes)
            return fail("MemoryFill range is outside supported aligned device VRAM");
        auto& channel=staged.channels[i];
        channel.enabled=true;
        channel.offset=input.start-kGpuVramVirtualBase;
        channel.setup={(input.start-kGpuVramVirtualBase+kGpuVramPhysicalBase)>>3,
                       (input.end-kGpuVramVirtualBase+kGpuVramPhysicalBase)>>3,
                       input.value,input.control};
        const unsigned reg=4+4*i;
        for (unsigned n=0;n<4;++n) staged.registers[reg+n]=channel.setup[n];
        if ((input.control&1U)==0) continue; // The reference returns before performing work.
        if (!vram) return fail("MemoryFill requires an explicitly configured VRAM bank");
        const auto size=input.end-input.start;
        if (size>kMemoryFillMaxBytes) return fail("MemoryFill exceeds the per-channel host staging bound");
        const unsigned width=(input.control&0x100U) ? 3 : (input.control&0x200U) ? 4 : 2;
        if (size%width) return fail("MemoryFill partial pattern at range end is unsupported");
        channel.triggered=true;
        channel.output.resize(size);
        // Wire/register order is explicitly little-endian on every host. Both width
        // flags set follow the reference's 24-bit precedence, not native word stores.
        for (std::uint32_t offset=0;offset<size;offset+=width)
            for (unsigned byte=0;byte<width;++byte)
                channel.output[offset+byte]=static_cast<std::uint8_t>(input.value>>(8*byte));
        // When both starts are nonzero, channel 0's IRQ is suppressed and channel 1
        // requests PSC0. This selection uses starts, NOT the trigger bits.
        channel.interrupt=both ? (i==0 ? -1 : 0) : static_cast<int>(i);
        if (channel.interrupt>=0) ++staged.irqs;
        staged.registers[reg+3]=(input.control&~1U)|2U;
    }
    plan=std::move(staged);
    return true;
}
} // namespace lego::ctr
