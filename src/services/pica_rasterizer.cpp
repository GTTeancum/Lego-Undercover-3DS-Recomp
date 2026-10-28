#include "services/pica_rasterizer.h"
#include "services/gsp_display_transfer.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cfenv>
#include <cmath>
#include <limits>
#include <utility>

namespace lego::ctr {
namespace {
// Independently authored bounded stage. Register formats and reference behavior:
// Azahar 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac, pica/{regs_rasterizer,
// regs_framebuffer,regs_texturing,output_vertex,primitive_assembly} and the
// renderer_software/{sw_clipper,sw_rasterizer,sw_texturing,sw_framebuffer} files.
// Reference coverage and float32 interpolation are approximations, not a claim
// of measured PICA subpixel or float24 arithmetic accuracy.
static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559);
using Color=std::array<std::uint8_t,4>;

bool Known(const PicaUploadState& state,std::uint32_t id,std::uint32_t bits) {
    for (unsigned byte=0;byte<4;++byte)
        if ((bits&(0xFFU<<(byte*8))) && !(state.register_known_bytes[id]&(1U<<byte)))
            return false;
    return true;
}

float Float24(std::uint32_t raw) {
    const auto sign=(raw&0x800000U)<<8;
    auto exponent=(raw>>16)&127U,mantissa=raw&65535U;
    std::uint32_t bits{};
    if (exponent==127) bits=sign|0x7F800000U|(mantissa<<7);
    else if (!exponent) {
        if (!mantissa) bits=sign;
        else {
            exponent=65;
            while (!(mantissa&0x10000U)) {--exponent;mantissa<<=1;}
            bits=sign|(exponent<<23)|((mantissa&65535U)<<7);
        }
    } else bits=sign|((exponent+64)<<23)|(mantissa<<7);
    return std::bit_cast<float>(bits);
}

bool NormalOrZero(float value) {
    const auto bits=std::bit_cast<std::uint32_t>(value),exponent=bits&0x7F800000U;
    return exponent!=0x7F800000U && (exponent || !(bits&0x007FFFFFU));
}

// Explicit binary32 rounding boundaries prevent contraction into FMA. The
// supported numeric slice also refuses host-flushed nonzero underflows.
struct Arithmetic {
    bool valid{true};
    float Mul(float a,float b) {
        if (!NormalOrZero(a)||!NormalOrZero(b)) {valid=false;return 0;}
        volatile float rounded=a*b; const float out=rounded;
        if (!NormalOrZero(out) || (out==0 && a!=0 && b!=0)) valid=false;
        return out;
    }
    float Add(float a,float b) {
        if (!NormalOrZero(a)||!NormalOrZero(b)) {valid=false;return 0;}
        volatile float rounded=a+b; const float out=rounded;
        if (!NormalOrZero(out) || (out==0 && !(a==0&&b==0) && a!=-b)) valid=false;
        return out;
    }
    float Divide(float a,float b) {
        if (!NormalOrZero(a)||!NormalOrZero(b)||b==0) {valid=false;return 0;}
        volatile float rounded=a/b; const float out=rounded;
        if (!NormalOrZero(out)||(out==0&&a!=0)) valid=false;
        return out;
    }
    float Dot(const std::array<float,3>& a,const std::array<float,3>& b) {
        const auto x=Mul(a[0],b[0]),y=Mul(a[1],b[1]),z=Mul(a[2],b[2]);
        return Add(Add(x,y),z);
    }
};

struct Vertex {
    std::int32_t x{},y{}; // 12.4 raster coordinates, within the supported framebuffer.
    float inverse_w{};
    std::array<float,4> color_over_w{};
    std::array<float,2> texture_over_w{};
    float z_over_w{};
};
struct Triangle {
    std::array<std::size_t,3> vertex{};
    std::array<std::int64_t,3> bias{};
    std::uint32_t left{},bottom{},right{},top{}; // Exclusive right/top pixel bounds.
};
std::int64_t Edge(const Vertex& a,const Vertex& b,std::int32_t x,std::int32_t y) {
    return std::int64_t(b.x-a.x)*(y-a.y)-std::int64_t(b.y-a.y)*(x-a.x);
}
std::int64_t EdgeBias(const Vertex& opposite,const Vertex& a,const Vertex& b) {
    if (a.y==b.y) return opposite.y<a.y?-1:0;
    // Integer truncation in the reference's edge-side test is intentional.
    const auto numerator=std::int64_t(b.x-a.x)*(opposite.y-a.y);
    const auto edge_x=std::int64_t(a.x)+numerator/(b.y-a.y);
    return opposite.x<edge_x?-1:0;
}
std::uint32_t PixelOffset(std::uint32_t x,std::uint32_t y,std::uint32_t width,
                          std::uint32_t height) {
    const auto stored_y=height-1-y;
    std::uint32_t morton{};
    for (unsigned bit=0;bit<3;++bit) {
        morton|=((x>>bit)&1U)<<(2*bit);
        morton|=((stored_y>>bit)&1U)<<(2*bit+1);
    }
    return 4U*((stored_y&~7U)*width+(x&~7U)*8U+morton);
}

struct TevStage {
    std::array<std::uint8_t,2> rgb_sources{},alpha_sources{};
    std::uint8_t rgb_op{},alpha_op{};
    Color constant{};
};
Color Tev(const Color& primary,const Color& texture,const std::array<TevStage,6>& stages) {
    Color previous{};
    for (const auto& stage:stages) {
        const auto source=[&](unsigned id)->const Color& {
            return id==0?primary:(id==3?texture:(id==14?stage.constant:previous));
        };
        Color next{};
        for (unsigned channel=0;channel<4;++channel) {
            const auto& sources=channel==3?stage.alpha_sources:stage.rgb_sources;
            const auto op=channel==3?stage.alpha_op:stage.rgb_op;
            const auto a=source(sources[0])[channel];
            next[channel]=op?static_cast<std::uint8_t>(unsigned(a)*source(sources[1])[channel]/255U):a;
        }
        previous=next;
    }
    return previous;
}
unsigned BlendFactor(unsigned factor,const Color& source) {
    switch (factor) {
    case 0:return 0;
    case 1:return 255;
    case 6:return source[3];
    default:return 255-source[3]; // Factor7, checked during state validation.
    }
}

bool Overlaps(std::uint32_t a,std::uint32_t a_bytes,std::uint32_t b,std::uint32_t b_bytes) {
    return std::uint64_t(a)<std::uint64_t(b)+b_bytes && std::uint64_t(b)<std::uint64_t(a)+a_bytes;
}
bool DepthPass(unsigned function,std::uint32_t incoming,std::uint32_t stored) {
    switch (function) {
    case 0:return false;
    case 1:return true;
    case 2:return incoming==stored;
    case 3:return incoming!=stored;
    case 4:return incoming<stored;
    case 5:return incoming<=stored;
    case 6:return incoming>stored;
    case 7:return incoming>=stored;
    default:return false;
    }
}

struct Rasterizer {
    const PicaGpuRegisters& registers;
    const PicaUploadState& uploads;
    const PicaVertexShaderPlan& shader;
    const GpuVramBank& vram;
    const char*& error;
    GuestMemory* texture_memory{};
    PicaRasterPlan staged;
    PicaTexturePlan texture;
    std::array<TevStage,6> tev{};
    std::vector<Vertex> vertices;
    std::vector<Triangle> triangles;
    std::array<std::uint32_t,7> mappings{};
    unsigned output_count{},topology{},cull{},write_mask{};
    bool blend{},write_gate{};
    bool texture_enabled{},depth_test{},depth_write{};
    unsigned depth_function{};
    std::array<unsigned,4> factors{}; // RGB src/dst, alpha src/dst.
    float half_x{},half_y{};
    float depth_scale{},depth_offset{};
    std::int32_t corner_x{},corner_y{};
    std::uint32_t framebuffer_bytes{},vram_offset{};
    std::uint32_t depth_vram_offset{};
    std::uint32_t pixel_test_budget{kPicaRasterMaxPixelTests};

    bool Fail(const char* message) {error=message;return false;}
    std::uint32_t Reg(unsigned id) const {return registers[0x400U+id];}
    bool Field(unsigned id,std::uint32_t bits) const {return Known(uploads,id,bits);}

    bool Configuration() {
        if (!uploads.primitive_empty) return Fail("PICA rasterizer requires a reset primitive assembler");
        if (std::fegetround()!=FE_TONEAREST) return Fail("PICA rasterizer requires nearest host rounding");
        if (!Field(0x25E,0x300)||!Field(0x40,3)||!Field(0x47,1)||!Field(0x65,3))
            return Fail("PICA raster primitive/cull/clip/scissor state is unknown");
        topology=(Reg(0x25E)>>8)&3U;cull=Reg(0x40)&3U;
        if (uploads.topology!=topology) return Fail("PICA raster topology does not match assembler state");
        if (topology>1) return Fail("PICA rasterizer supports fresh triangle lists and strips only");
        if ((Reg(0x47)&1U)||(Reg(0x65)&3U))
            return Fail("PICA rasterizer does not support user clipping or scissor tests");
        if (shader.vertices.size()>kPicaVertexFetchMaxVertices ||
            (topology==0 && shader.vertices.size()%3U))
            return Fail("PICA raster vertex count exceeds bounds or leaves a partial triangle list");
        if (!Field(0x4F,7)) return Fail("PICA raster output semantic count is unknown");
        output_count=Reg(0x4F)&7U;
        if (shader.output_count>16 || output_count>shader.output_count)
            return Fail("PICA raster output mappings exceed the shaded attribute count");
        for (unsigned attribute=0;attribute<output_count;++attribute) {
            if (!Field(0x50+attribute,0x1F1F1F1FU))
                return Fail("PICA raster output component mapping is unknown");
            mappings[attribute]=Reg(0x50+attribute);
            for (unsigned component=0;component<4;++component) {
                const auto semantic=(mappings[attribute]>>(8*component))&31U;
                if (semantic>=24 && semantic!=31)
                    return Fail("PICA raster output component has an unsupported semantic");
            }
        }
        if (!Field(0x80,0x407)||!Field(0xE0,7))
            return Fail("PICA raster texture/procedural/fog enables are unknown");
        if ((Reg(0x80)&0x406U)||(Reg(0xE0)&7U))
            return Fail("PICA rasterizer supports texture0 only, with procedural textures and fog disabled");
        texture_enabled=(Reg(0x80)&1U)!=0;
        if (!Field(0x100,0x103)||!Field(0x104,1)||!Field(0x105,1)||
            !Field(0x107,0x1F01)||!Field(0x113,15)||!Field(0x115,3))
            return Fail("PICA raster fragment/depth/stencil/write state is unknown");
        if ((Reg(0x100)&3U)||(Reg(0x104)&1U)||(Reg(0x105)&1U))
            return Fail("PICA rasterizer does not support fragment modes, alpha or stencil tests");
        depth_test=(Reg(0x107)&1U)!=0;
        const auto depth_gate=Reg(0x115)&3U;
        if ((Reg(0x107)&0x1000U) && depth_gate!=0 && depth_gate!=2)
            return Fail("PICA raster depth-write permission is outside the supported mask");
        depth_write=(Reg(0x107)&0x1000U) && depth_gate==2;
        write_mask=(Reg(0x107)>>8)&15U;write_gate=(Reg(0x113)&15U)!=0;
        blend=(Reg(0x100)&0x100U)!=0;
        if (blend) {
            if (!Field(0x101,0xFFFF0707U)) return Fail("PICA raster blend equation/factors are unknown");
            if (Reg(0x101)&0x707U) return Fail("PICA rasterizer supports only additive alpha blending");
            for (unsigned index=0;index<4;++index) {
                factors[index]=(Reg(0x101)>>(16+4*index))&15U;
                if (factors[index]!=0 && factors[index]!=1 && factors[index]!=6 && factors[index]!=7)
                    return Fail("PICA raster blend factor is outside the supported slice");
            }
        } else if (!Field(0x102,15) || (Reg(0x102)&15U)!=3)
            return Fail("PICA rasterizer supports only Copy when alpha blending is disabled");
        if (!Field(0x117,0x70000)||!Field(0x11D,0x0FFFFFFF)||!Field(0x11E,0x013FF7FFU))
            return Fail("PICA raster color framebuffer state is unknown");
        if ((Reg(0x117)&0x70000U)||!(Reg(0x11E)&0x01000000U))
            return Fail("PICA rasterizer requires RGBA8 with the supported framebuffer orientation");
        staged.width=Reg(0x11E)&0x7FFU;staged.height=((Reg(0x11E)>>12)&0x3FFU)+1;
        const auto bytes=std::uint64_t(staged.width)*staged.height*4;
        const auto address=std::uint64_t(Reg(0x11D)&0x0FFFFFFFU)*8;
        if (!staged.width || (staged.width&7U)||(staged.height&7U)||
            bytes>kPicaRasterMaxFramebufferBytes || address<kGpuVramPhysicalBase ||
            address+bytes>std::uint64_t(kGpuVramPhysicalBase)+kGpuVramBytes)
            return Fail("PICA raster framebuffer exceeds supported tiled device VRAM bounds");
        staged.framebuffer_address=static_cast<std::uint32_t>(address);
        framebuffer_bytes=static_cast<std::uint32_t>(bytes);vram_offset=staged.framebuffer_address-kGpuVramPhysicalBase;
        if (!Field(0x41,0xFFFFFF)||!Field(0x43,0xFFFFFF)||!Field(0x68,0x03FF03FFU))
            return Fail("PICA raster viewport is unknown");
        half_x=Float24(Reg(0x41));half_y=Float24(Reg(0x43));
        corner_x=static_cast<std::int32_t>(Reg(0x68)&1023U);
        corner_y=static_cast<std::int32_t>((Reg(0x68)>>16)&1023U);
        if (corner_x&512) corner_x-=1024;
        if (corner_y&512) corner_y-=1024;
        Arithmetic math;
        const auto end_x=math.Add(math.Mul(half_x,2),static_cast<float>(corner_x));
        const auto end_y=math.Add(math.Mul(half_y,2),static_cast<float>(corner_y));
        if (!math.valid || half_x<=0 || half_y<=0 || corner_x<0 || corner_y<0 ||
            end_x>4095.9375f || end_y>4095.9375f)
            return Fail("PICA raster viewport is outside the supported finite fixed-point range");
        if (!TevConfiguration() || !DepthConfiguration()) return false;
        if (texture_enabled) {
            if (!StagePicaTexture0(registers,uploads,texture_memory,vram,texture,error)) return false;
            if (Overlaps(texture.read.physical_address,texture.read.bytes,
                         staged.framebuffer_address,framebuffer_bytes) ||
                ((depth_test||depth_write) && Overlaps(texture.read.physical_address,texture.read.bytes,
                         staged.depthbuffer_address,framebuffer_bytes)))
                return Fail("PICA texture source overlaps a color or depth framebuffer");
            staged.texture_reads.push_back(texture.read);
        }
        return true;
    }

    bool DepthConfiguration() {
        if (!depth_test && !depth_write) return true;
        if (!Field(0x116,3)||!Field(0x11C,0x0FFFFFFFU)||
            !Field(0x4D,0xFFFFFFU)||!Field(0x4E,0xFFFFFFU)||!Field(0x6D,1))
            return Fail("PICA raster depth format, buffer or viewport state is unknown");
        if ((Reg(0x116)&3U)!=3 || !(Reg(0x6D)&1U))
            return Fail("PICA raster depth supports D24S8 Z buffering only");
        if (depth_test) {
            if (!Field(0x107,0x70U)||!Field(0x114,3))
                return Fail("PICA raster depth comparison or read permission is unknown");
            if ((Reg(0x114)&3U)!=2)
                return Fail("PICA raster depth-read permission is outside the supported mask");
            depth_function=(Reg(0x107)>>4)&7U;
        }
        depth_scale=Float24(Reg(0x4D));depth_offset=Float24(Reg(0x4E));
        if (!NormalOrZero(depth_scale)||!NormalOrZero(depth_offset))
            return Fail("PICA raster depth viewport uses unsupported numeric state");
        const auto address=std::uint64_t(Reg(0x11C)&0x0FFFFFFFU)*8;
        if (address<kGpuVramPhysicalBase ||
            address+framebuffer_bytes>std::uint64_t(kGpuVramPhysicalBase)+kGpuVramBytes)
            return Fail("PICA raster depth framebuffer exceeds device VRAM");
        staged.depthbuffer_address=static_cast<std::uint32_t>(address);
        if (Overlaps(staged.depthbuffer_address,framebuffer_bytes,
                     staged.framebuffer_address,framebuffer_bytes))
            return Fail("PICA raster color and depth framebuffers overlap");
        depth_vram_offset=staged.depthbuffer_address-kGpuVramPhysicalBase;
        return true;
    }

    bool TevConfiguration() {
        constexpr std::array<unsigned,6> bases{0xC0,0xC8,0xD0,0xD8,0xF0,0xF8};
        for (unsigned index=0;index<6;++index) {
            const auto base=bases[index];auto& stage=tev[index];
            if (!Field(base+2,0x000F000F)||!Field(base+4,0x00030003))
                return Fail("PICA raster TEV operation or scale is unknown");
            stage.rgb_op=Reg(base+2)&15U;stage.alpha_op=(Reg(base+2)>>16)&15U;
            if (stage.rgb_op>1 || stage.alpha_op>1 || (Reg(base+4)&0x00030003U))
                return Fail("PICA raster TEV supports unscaled Replace and Modulate only");
            const auto constant=Reg(base+3);
            for (unsigned channel=0;channel<4;++channel) stage.constant[channel]=(constant>>(channel*8))&255U;
            for (unsigned alpha=0;alpha<2;++alpha) {
                const auto operation=alpha?stage.alpha_op:stage.rgb_op;
                auto& sources=alpha?stage.alpha_sources:stage.rgb_sources;
                for (unsigned operand=0;operand<=operation;++operand) {
                    const auto source_shift=(alpha?16U:0U)+operand*4U;
                    const auto modifier_shift=alpha?12U+operand*4U:operand*4U;
                    const auto modifier_mask=(alpha?7U:15U)<<modifier_shift;
                    if (!Field(base,15U<<source_shift)||!Field(base+1,modifier_mask))
                        return Fail("PICA raster TEV consumed operand is unknown");
                    if (Reg(base+1)&modifier_mask)
                        return Fail("PICA raster TEV operand modifiers are unsupported");
                    auto source=(Reg(base)>>source_shift)&15U;
                    // First-stage RGB Previous operands1/2 route through source3
                    // in the reference. Alpha Previous starts with zero.
                    if (index==0 && !alpha && source==15) {
                        if (!Field(base,0xF00)) return Fail("PICA raster initial TEV source is unknown");
                        source=(Reg(base)>>8)&15U;
                    }
                    if (source!=0 && source!=3 && source!=14 && source!=15)
                        return Fail("PICA raster TEV source requires an unsupported texture, light or buffer");
                    if (source==3 && !texture_enabled)
                        return Fail("PICA raster TEV requires enabled texture0");
                    if (source==14 && !Field(base+3,alpha?0xFF000000U:0x00FFFFFFU))
                        return Fail("PICA raster TEV constant component is unknown");
                    sources[operand]=static_cast<std::uint8_t>(source);
                }
            }
        }
        return true;
    }

    bool Transform() {
        vertices.reserve(shader.vertices.size());
        for (const auto& shaded:shader.vertices) {
            // Unmapped output semantics have the reference's hardware-tested
            // value1. A mapped but unwritten shader component remains unknown.
            std::array<float,14> slots;slots.fill(1);
            std::array<bool,14> known;known.fill(true);
            for (unsigned attribute=0;attribute<output_count;++attribute)
                for (unsigned lane=0;lane<4;++lane) {
                    const auto semantic=(mappings[attribute]>>(lane*8))&31U;
                    if (semantic>=14) continue; // Only ordinary texture0 UV is consumed.
                    slots[semantic]=std::bit_cast<float>(shaded.attributes[attribute][lane]);
                    known[semantic]=(shaded.attribute_components[attribute]&(1U<<lane))!=0;
                }
            for (unsigned semantic: {0U,1U,2U,3U,8U,9U,10U,11U})
                if (!known[semantic] || !NormalOrZero(slots[semantic]))
                    return Fail("PICA raster position/color uses an unknown or unsupported numeric component");
            const auto x=slots[0],y=slots[1],z=slots[2],w=slots[3];
            if (w<=0 || x < -w || x>w || y < -w || y>w || z < -w || z>0)
                return Fail("PICA raster clipping is required outside the supported homogeneous volume");
            Arithmetic math;Vertex vertex;vertex.inverse_w=math.Divide(1,w);
            if (depth_test||depth_write) vertex.z_over_w=math.Mul(z,vertex.inverse_w);
            if (texture_enabled) {
                for (unsigned component=0;component<2;++component) {
                    if (!known[12+component] || !NormalOrZero(slots[12+component]))
                        return Fail("PICA raster texture coordinate is unknown or unsupported");
                    vertex.texture_over_w[component]=math.Mul(slots[12+component],vertex.inverse_w);
                }
            }
            const auto screen_x=math.Add(math.Mul(math.Add(math.Mul(x,vertex.inverse_w),1),half_x),static_cast<float>(corner_x));
            const auto screen_y=math.Add(math.Mul(math.Add(math.Mul(y,vertex.inverse_w),1),half_y),static_cast<float>(corner_y));
            const auto fixed_x=math.Mul(screen_x,16),fixed_y=math.Mul(screen_y,16);
            if (!math.valid || fixed_x<0 || fixed_y<0 || fixed_x>staged.width*16U || fixed_y>staged.height*16U)
                return Fail("PICA raster transformed position exceeds the supported framebuffer");
            vertex.x=static_cast<std::int32_t>(std::round(fixed_x));
            vertex.y=static_cast<std::int32_t>(std::round(fixed_y));
            for (unsigned channel=0;channel<4;++channel)
                vertex.color_over_w[channel]=math.Mul(std::min(1.0f,std::fabs(slots[8+channel])),vertex.inverse_w);
            if (!math.valid) return Fail("PICA raster perspective conversion exceeds supported finite arithmetic");
            vertices.push_back(vertex);
        }
        return true;
    }

    bool Assemble() {
        const auto append=[&](std::array<std::size_t,3> indices)->bool {
            ++staged.triangles;
            auto area=Edge(vertices[indices[0]],vertices[indices[1]],vertices[indices[2]].x,vertices[indices[2]].y);
            if (!area || (cull==1 && area>0) || (cull==2 && area<0)) return true;
            if (area<0) std::swap(indices[1],indices[2]);
            Triangle triangle;triangle.vertex=indices;
            std::int32_t min_x=vertices[indices[0]].x,max_x=min_x,min_y=vertices[indices[0]].y,max_y=min_y;
            for (unsigned index=0;index<3;++index) {
                const auto& a=vertices[indices[index]];
                min_x=std::min(min_x,a.x);max_x=std::max(max_x,a.x);
                min_y=std::min(min_y,a.y);max_y=std::max(max_y,a.y);
                triangle.bias[index]=EdgeBias(a,vertices[indices[(index+1)%3]],vertices[indices[(index+2)%3]]);
            }
            triangle.left=static_cast<unsigned>(min_x)/16;triangle.right=(static_cast<unsigned>(max_x)+15)/16;
            triangle.bottom=static_cast<unsigned>(min_y)/16;triangle.top=(static_cast<unsigned>(max_y)+15)/16;
            const auto tests=std::uint64_t(triangle.right-triangle.left)*(triangle.top-triangle.bottom);
            if (tests+staged.pixels_tested>pixel_test_budget)
                return Fail("PICA rasterizer exceeds the remaining pixel-test budget");
            staged.pixels_tested+=static_cast<std::uint32_t>(tests);triangles.push_back(triangle);
            return true;
        };
        if (topology==0) {
            for (std::size_t i=0;i<vertices.size();i+=3) if (!append({i,i+1,i+2})) return false;
        } else {
            for (std::size_t i=2;i<vertices.size();++i)
                if (!append((i&1U)?std::array<std::size_t,3>{i-1,i-2,i}:
                                   std::array<std::size_t,3>{i-2,i-1,i})) return false;
        }
        return true;
    }

    bool Pixels() {
        PicaRasterPatch patch;patch.offset=vram_offset;
        const auto source=vram.bytes().subspan(vram_offset,framebuffer_bytes);
        patch.bytes.assign(source.begin(),source.end());
        PicaRasterPatch depth_patch;
        if (depth_test||depth_write) {
            depth_patch.offset=depth_vram_offset;
            const auto depth_source=vram.bytes().subspan(depth_vram_offset,framebuffer_bytes);
            depth_patch.bytes.assign(depth_source.begin(),depth_source.end());
        }
        for (const auto& triangle:triangles) {
            const auto& a=vertices[triangle.vertex[0]];const auto& b=vertices[triangle.vertex[1]];
            const auto& c=vertices[triangle.vertex[2]];
            const std::array<float,3> inverse_w{a.inverse_w,b.inverse_w,c.inverse_w};
            for (auto y=triangle.bottom;y<triangle.top;++y) for (auto x=triangle.left;x<triangle.right;++x) {
                const auto sample_x=static_cast<std::int32_t>(x*16+8),sample_y=static_cast<std::int32_t>(y*16+8);
                const std::array<std::int64_t,3> weights{
                    Edge(b,c,sample_x,sample_y)+triangle.bias[0],
                    Edge(c,a,sample_x,sample_y)+triangle.bias[1],
                    Edge(a,b,sample_x,sample_y)+triangle.bias[2]};
                if (weights[0]<0 || weights[1]<0 || weights[2]<0) continue;
                ++staged.pixels_covered;
                std::array<float,3> barycentric{};
                for (unsigned i=0;i<3;++i) {volatile float rounded=static_cast<float>(weights[i]);barycentric[i]=rounded;}
                Arithmetic math;const auto reciprocal=math.Divide(1,math.Dot(inverse_w,barycentric));
                Color primary{};
                for (unsigned channel=0;channel<4;++channel) {
                    const std::array<float,3> values{a.color_over_w[channel],b.color_over_w[channel],c.color_over_w[channel]};
                    const auto interpolated=math.Mul(math.Dot(values,barycentric),reciprocal);
                    const auto scaled=math.Mul(interpolated,255);
                    if (!math.valid) return Fail("PICA raster interpolation exceeds supported finite arithmetic");
                    const auto quantized=std::round(scaled);
                    if (quantized<0 || quantized>255)
                        return Fail("PICA raster interpolated primary color is outside the byte range");
                    primary[channel]=static_cast<std::uint8_t>(quantized);
                }
                Color sampled{};
                if (texture_enabled) {
                    std::array<float,2> coordinate{};
                    for (unsigned component=0;component<2;++component) {
                        const std::array<float,3> values{a.texture_over_w[component],
                            b.texture_over_w[component],c.texture_over_w[component]};
                        coordinate[component]=math.Mul(math.Dot(values,barycentric),reciprocal);
                    }
                    if (!math.valid) return Fail("PICA raster texture interpolation exceeds finite arithmetic");
                    if (!SamplePicaTexture(texture,coordinate[0],coordinate[1],sampled,error)) return false;
                }
                const auto combined=Tev(primary,sampled,tev);
                const auto offset=PixelOffset(x,y,staged.width,staged.height);
                if (depth_test||depth_write) {
                    const std::array<float,3> z{a.z_over_w,b.z_over_w,c.z_over_w};
                    volatile float rounded_sum=static_cast<float>(weights[0]+weights[1]+weights[2]);
                    const auto interpolated_z=math.Divide(math.Dot(z,barycentric),rounded_sum);
                    const auto viewport_z=math.Add(math.Mul(interpolated_z,depth_scale),depth_offset);
                    if (!math.valid) return Fail("PICA raster depth interpolation exceeds finite arithmetic");
                    const auto scaled=math.Mul(std::clamp(viewport_z,0.0f,1.0f),16777215.0f);
                    if (!math.valid || scaled<0 || scaled>16777215.0f)
                        return Fail("PICA raster depth conversion exceeds the 24-bit range");
                    const auto incoming=static_cast<std::uint32_t>(scaled);
                    const auto stored=std::uint32_t(depth_patch.bytes[offset]) |
                        (std::uint32_t(depth_patch.bytes[offset+1])<<8) |
                        (std::uint32_t(depth_patch.bytes[offset+2])<<16);
                    if (depth_test && !DepthPass(depth_function,incoming,stored)) continue;
                    if (depth_write) {
                        for (unsigned byte=0;byte<3;++byte)
                            depth_patch.bytes[offset+byte]=static_cast<std::uint8_t>(incoming>>(byte*8));
                        // D24S8 byte3 is stencil. Stencil testing/writes remain disabled.
                        ++staged.depth_pixels_written;
                    }
                }
                Color destination{};for (unsigned channel=0;channel<4;++channel) destination[channel]=patch.bytes[offset+3-channel];
                if (write_gate && write_mask) {
                    for (unsigned channel=0;channel<4;++channel) {
                        if (!(write_mask&(1U<<channel))) continue;
                        unsigned value=combined[channel];
                        if (blend) {
                            const auto factor=channel==3?2U:0U;
                            value=(unsigned(combined[channel])*BlendFactor(factors[factor],combined)+
                                   unsigned(destination[channel])*BlendFactor(factors[factor+1],combined))/255U;
                            value=std::min(value,255U);
                        }
                        patch.bytes[offset+3-channel]=static_cast<std::uint8_t>(value);
                    }
                    ++staged.pixels_written;
                }
            }
        }
        if (staged.pixels_written) staged.patches.push_back(std::move(patch));
        if (staged.depth_pixels_written) staged.patches.push_back(std::move(depth_patch));
        return true;
    }
};
} // namespace

bool StagePicaRasterizer(const PicaGpuRegisters& registers,const PicaUploadState& uploads,
                         const PicaVertexShaderPlan& shader,const GpuVramBank& vram,
                         PicaRasterPlan& plan,const char*& error,std::uint32_t pixel_test_budget,
                         GuestMemory* texture_memory) {
    error=nullptr;
    Rasterizer raster{registers,uploads,shader,vram,error};
    raster.texture_memory=texture_memory;
    raster.pixel_test_budget=std::min(pixel_test_budget,kPicaRasterMaxPixelTests);
    if (!raster.Configuration() || !raster.Transform() || !raster.Assemble() || !raster.Pixels()) return false;
    plan=std::move(raster.staged);
    return true;
}
} // namespace lego::ctr
