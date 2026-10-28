#include "services/pica_vertex_fetch.h"
#include "runtime/ctr_memory.h"
#include "services/gsp_display_transfer.h"
#include <algorithm>
#include <bit>
#include <limits>
#include <utility>

namespace lego::ctr {
namespace {
// Register/layout and conversion behavior follow Azahar 86a9f9236ae42bb5a2b995d
// bc933d599d8ea07ac: pica/regs_pipeline.h, vertex_loader.cpp and shader_unit.cpp.
// This is an independent bounded fetch stage; no reference renderer is embedded.
static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559);

struct AttributeSource {
    std::uint64_t offset{};
    std::uint32_t stride{},format{},components{},element_bytes{};
    bool assigned{};
};

bool Known(const PicaUploadState& uploads,std::uint32_t id,std::uint32_t bits) {
    std::uint32_t known{};
    for (unsigned byte=0;byte<4;++byte)
        if (uploads.register_known_bytes[id]&(1U<<byte)) known|=0xFFU<<(byte*8);
    return (known&bits)==bits;
}

std::uint32_t IntegerBits(std::int32_t value) {
    // Every supported 8/16-bit integer is represented exactly in binary32.
    return std::bit_cast<std::uint32_t>(static_cast<float>(value));
}

bool ReadPhysical(GuestMemory& memory,const GpuVramBank* vram,
                  std::uint32_t address,std::span<std::uint8_t> destination) {
    const auto end=std::uint64_t(address)+destination.size();
    if (address>=0x20000000U && end<=0x28000000ULL) {
        const auto va=address-0x20000000U+0x14000000U;
        const auto size=static_cast<std::uint32_t>(destination.size());
        // Metadata preflight excludes shared memory and all device callbacks.
        if (!memory.IsCachelessPrivateRange(va,size)) return false;
        for (std::uint32_t i=0;i<size;++i)
            if (!memory.Read8(va+i,&destination[i])) return false;
        return true;
    }
    if (address>=kGpuVramPhysicalBase &&
        end<=std::uint64_t(kGpuVramPhysicalBase)+kGpuVramBytes && vram) {
        const auto source=vram->bytes().subspan(address-kGpuVramPhysicalBase,destination.size());
        std::copy(source.begin(),source.end(),destination.begin());
        return true;
    }
    return false;
}
} // namespace

bool StagePicaVertexFetch(std::uint32_t draw_register,
                          const PicaGpuRegisters& registers,const PicaUploadState& uploads,
                          GuestMemory& memory,const GpuVramBank* vram,
                          PicaVertexFetchPlan& plan,const char*& error) {
    error=nullptr;
    const auto fail=[&](const char* message) {error=message;return false;};
    const auto reg=[&](std::uint32_t id) {return registers[0x400U+id];};
    if (draw_register!=0x22E)
        return fail("PICA vertex fetch supports only non-indexed draw arrays");
    if (!Known(uploads,0x229,3))
        return fail("PICA vertex-fetch geometry mode is unknown");
    if (reg(0x229)&3U)
        return fail("PICA vertex fetch does not support geometry shaders");
    if (!Known(uploads,0x2B9,0xFF00FF0FU))
        return fail("PICA vertex-shader input configuration is unknown");
    const auto input_config=reg(0x2B9);
    if ((input_config>>24)!=0xA0U || (input_config&0xFF00U))
        return fail("PICA vertex fetch requires VS mode without input-to-uniform routing");
    if (!Known(uploads,0x202,0xFFFF0000U))
        return fail("PICA vertex attribute count/default mask is unknown");
    if (!Known(uploads,0x228,0xFFFFFFFFU) || !Known(uploads,0x22A,0xFFFFFFFFU))
        return fail("PICA vertex count or start offset is unknown");

    const auto count=reg(0x228),start=reg(0x22A);
    if (count>kPicaVertexFetchMaxVertices)
        return fail("PICA vertex count exceeds the host staging bound");
    if (count && std::uint64_t(start)+count-1>std::numeric_limits<std::uint32_t>::max())
        return fail("PICA vertex index overflows the supported address calculation");
    const auto attribute_count=(reg(0x202)>>28)+1U;
    const auto input_count=(input_config&15U)+1U;
    if (input_count>attribute_count)
        return fail("PICA vertex-shader inputs require unknown retained attributes");
    const auto default_mask=(reg(0x202)>>16)&0xFFFU;
    const auto is_default=[&](std::uint32_t attribute) {
        return attribute>=12 || (default_mask&(1U<<attribute));
    };

    std::array<AttributeSource,12> sources{};
    for (std::uint32_t loader=0;loader<12;++loader) {
        const auto id=0x203U+loader*3U;
        if (!Known(uploads,id+2,0xF0000000U))
            return fail("PICA vertex loader enable/count is unknown");
        const auto config=reg(id+2),components=config>>28;
        if (!components) continue;
        if (components>12)
            return fail("PICA vertex loader has more than twelve descriptors");
        if (!Known(uploads,id,0x0FFFFFFFU) || !Known(uploads,id+2,0x00FF0000U))
            return fail("PICA vertex loader offset or stride is unknown");
        std::uint64_t offset{};
        for (std::uint32_t component=0;component<components;++component) {
            const auto component_reg=id+(component<8?1U:2U);
            const auto shift=(component%8U)*4U;
            if (!Known(uploads,component_reg,15U<<shift))
                return fail("PICA vertex loader descriptor is unknown");
            const auto attribute=(reg(component_reg)>>shift)&15U;
            if (attribute>=12) {
                // Descriptors 12..15 are aligned padding, not array attributes.
                offset=(offset+3U)&~std::uint64_t(3U);
                offset+=4U*(attribute-11U);
                continue;
            }
            const auto format_reg=attribute<8?0x201U:0x202U;
            const auto format_shift=(attribute%8U)*4U;
            if (!Known(uploads,format_reg,15U<<format_shift))
                return fail("PICA vertex attribute format is unknown");
            const auto descriptor=(reg(format_reg)>>format_shift)&15U;
            const auto format=descriptor&3U,elements=(descriptor>>2)+1U;
            const auto element_bytes=format==3?4U:(format==2?2U:1U);
            offset=(offset+element_bytes-1U)&~std::uint64_t(element_bytes-1U);
            if (sources[attribute].assigned)
                return fail("PICA vertex attribute has ambiguous duplicate loaders");
            sources[attribute]={std::uint64_t(reg(id)&0x0FFFFFFFU)+offset,
                                (config>>16)&0xFFU,format,elements,element_bytes,true};
            offset+=elements*element_bytes;
        }
    }

    std::uint32_t bytes_per_vertex{},array_attributes{};
    for (std::uint32_t attribute=0;attribute<attribute_count;++attribute) {
        if (is_default(attribute)) {
            if (attribute>=kPicaDefaultAttributeCount ||
                !uploads.default_attributes.written[attribute])
                return fail("PICA vertex default attribute has not been uploaded");
        } else {
            if (!sources[attribute].assigned)
                return fail("PICA vertex attribute requires unsupported retained array state");
            bytes_per_vertex+=sources[attribute].components*sources[attribute].element_bytes;
            ++array_attributes;
        }
    }
    if (std::uint64_t(bytes_per_vertex)*count>kPicaVertexFetchMaxBytes)
        return fail("PICA vertex reads exceed the host staging bound");
    std::array<std::uint32_t,16> input_registers{};
    for (std::uint32_t attribute=0;attribute<input_count;++attribute) {
        const auto id=attribute<8?0x2BBU:0x2BCU,shift=(attribute%8U)*4U;
        if (!Known(uploads,id,15U<<shift))
            return fail("PICA vertex-shader input register mapping is unknown");
        input_registers[attribute]=(reg(id)>>shift)&15U;
    }
    if (array_attributes && !Known(uploads,0x200,0x1FFFFFFEU))
        return fail("PICA vertex physical base address is unknown");
    const auto base=std::uint64_t((reg(0x200)>>1)&0x0FFFFFFFU)*16U;

    PicaVertexFetchPlan staged;
    staged.attribute_count=attribute_count;staged.input_count=input_count;
    staged.vertices.resize(count);
    staged.reads.reserve(std::size_t(count)*array_attributes);
    for (std::uint32_t ordinal=0;ordinal<count;++ordinal) {
        auto& vertex=staged.vertices[ordinal];vertex.source_index=start+ordinal;
        for (std::uint32_t attribute=0;attribute<attribute_count;++attribute) {
            auto& value=vertex.attributes[attribute];
            if (is_default(attribute)) {
                value=uploads.default_attributes.floats[attribute];
            } else {
                const auto& source=sources[attribute];
                const auto address=base+source.offset+
                                   std::uint64_t(source.stride)*vertex.source_index;
                const auto bytes=source.components*source.element_bytes;
                if (address>std::numeric_limits<std::uint32_t>::max() ||
                    address+bytes>0x100000000ULL)
                    return fail("PICA vertex source address overflows the physical address space");
                std::array<std::uint8_t,16> data{};
                const auto physical=static_cast<std::uint32_t>(address);
                if (!ReadPhysical(memory,vram,physical,std::span(data).first(bytes)))
                    return fail("PICA vertex source is outside readable private FCRAM/configured VRAM");
                staged.reads.push_back({physical,bytes,vertex.source_index,attribute});
                value={0,0,0,0x3F800000U};
                for (std::uint32_t component=0;component<source.components;++component) {
                    std::uint32_t raw{};
                    for (std::uint32_t byte=0;byte<source.element_bytes;++byte)
                        raw|=std::uint32_t(data[component*source.element_bytes+byte])<<(8*byte);
                    if (source.format==3) value[component]=raw;
                    else {
                        auto integer=static_cast<std::int32_t>(raw);
                        if (source.format==0 && (raw&0x80U)) integer-=256;
                        if (source.format==2 && (raw&0x8000U)) integer-=65536;
                        value[component]=IntegerBits(integer);
                    }
                }
            }
            vertex.attributes_written.set(attribute);
        }
        // The pinned shader unit maps in increasing attribute order; a repeated
        // destination input register receives the last mapped attribute.
        for (std::uint32_t attribute=0;attribute<input_count;++attribute) {
            const auto input=input_registers[attribute];
            vertex.inputs[input]=vertex.attributes[attribute];
            vertex.inputs_written.set(input);
        }
    }
    plan=std::move(staged);
    return true;
}
} // namespace lego::ctr
