#include "services/pica_rasterizer.h"
#include "services/gsp_display_transfer.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cfenv>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

using namespace lego::ctr;
namespace {
int failures{};
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL " << __LINE__ << ": " #x "\n"; ++failures; } } while (0)
using Color = std::array<std::uint8_t,4>;
using FloatColor = std::array<float,4>;
using Pixel = std::array<unsigned,2>;
constexpr Color kDestination{20,40,60,91};
constexpr FloatColor kTranslucent{1.0f,0.25f,0.75f,0.25f};
constexpr std::array<unsigned,6> kTevBases{0xC0,0xC8,0xD0,0xD8,0xF0,0xF8};

// All fixtures are authored, ROM-free geometry and colors. Golden coverage,
// channel bytes, and interpolation results follow the explicitly bounded
// reference model, not a claim about hardware float24 precision:
// azahar-emu/azahar@86a9f9236ae42bb5a2b995dbc933d599d8ea07ac,
// src/video_core/renderer_software/{sw_clipper,sw_rasterizer,sw_framebuffer,
// sw_texturing}.cpp and pica/primitive_assembly.cpp.
// Expected frames do not call production geometry, color, or address helpers.
std::uint32_t Bits(float value) { return std::bit_cast<std::uint32_t>(value); }
std::uint32_t ExactFloat24(float value) {
    const auto bits=Bits(value);
    return ((bits>>8)&0x800000U)|((((bits>>23)&255U)-64U)<<16)|((bits>>7)&0xFFFFU);
}

// Independent explicit Morton digit tables. RGBA8 is stored A,B,G,R; screen
// y=0 addresses the last framebuffer row. The cross-tile test also checks four
// literal byte offsets so this helper cannot alone define the expected layout.
std::size_t ByteOffset(unsigned width,unsigned height,unsigned x,unsigned y) {
    constexpr std::array<unsigned,8> x_digits{0,1,4,5,16,17,20,21};
    constexpr std::array<unsigned,8> y_digits{0,2,8,10,32,34,40,42};
    const auto memory_y=height-1-y;
    return 4U*((memory_y/8)*width*8+(x/8)*64+x_digits[x%8]+y_digits[memory_y%8]);
}
void StoreColor(std::vector<std::uint8_t>& bytes,std::size_t offset,Color rgba) {
    if (offset+4>bytes.size()) { CHECK(false); return; }
    for (unsigned channel=0;channel<4;++channel) bytes[offset+channel]=rgba[3-channel];
}

struct Fixture {
    unsigned width,height;
    std::uint32_t offset=0x1000;
    PicaGpuRegisters regs{};
    PicaUploadState uploads{};
    PicaVertexShaderPlan shader;
    std::shared_ptr<GpuVramBank> bank=GpuVramBank::ReferenceZero();
    PicaRasterPlan plan;
    const char* error{};

    explicit Fixture(unsigned w=8,unsigned h=8):width(w),height(h) {
        // Unit-level register state is deliberately explicit. Decoder/queue
        // tests separately establish these known bytes through real packets.
        uploads.register_known_bytes.fill(15); uploads.registers_written.set();
        uploads.primitive_empty=true;
        Set(0x40,0); Set(0x41,ExactFloat24(float(w)/2)); Set(0x43,ExactFloat24(float(h)/2));
        Set(0x47,0); Set(0x4D,0xBF0000); Set(0x4E,0); Set(0x4F,2);
        Set(0x50,0x03020100); Set(0x51,0x0B0A0908);
        Set(0x65,0); Set(0x68,0); Set(0x6D,1); Set(0x80,0); Set(0xE0,0);
        Set(0x100,0x100); Set(0x101,0x76760000); Set(0x102,3);
        Set(0x104,0); Set(0x105,0); Set(0x107,0x700);
        Set(0x113,1); Set(0x115,0); Set(0x116,3); Set(0x117,0);
        Set(0x11D,(kGpuVramPhysicalBase+offset)/8); Set(0x11E,w|((h-1)<<12)|(1U<<24));
        Set(0x1C6,1); Topology(0);
        for (unsigned stage=0;stage<kTevBases.size();++stage) {
            const auto base=kTevBases[stage];
            Set(base,stage?0xF000F:0); Set(base+1,0); Set(base+2,0); Set(base+3,0); Set(base+4,0);
        }
        shader.output_count=2;
        std::vector<std::uint8_t> initial(w*h*4+64,0xA5);
        for (unsigned i=0;i<w*h;++i) StoreColor(initial,32+4*i,kDestination);
        CHECK(bank->Write(offset-32,initial));
    }
    void Set(unsigned id,std::uint32_t value) { regs[0x400+id]=value; }
    std::uint32_t Get(unsigned id) const { return regs[0x400+id]; }
    void Topology(unsigned value) { Set(0x25E,value<<8); uploads.topology=value; }
    void Vertex(float x,float y,FloatColor color=kTranslucent,float clip_w=1) {
        PicaShadedVertex vertex;
        vertex.source_index=static_cast<std::uint32_t>(shader.vertices.size());
        const std::array<float,4> position{
            (x/(float(width)/2)-1)*clip_w,(y/(float(height)/2)-1)*clip_w,-0.5f*clip_w,clip_w};
        for (unsigned c=0;c<4;++c) {
            vertex.attributes[0][c]=Bits(position[c]); vertex.attributes[1][c]=Bits(color[c]);
        }
        vertex.attribute_components[0]=15; vertex.attribute_components[1]=15;
        vertex.outputs[0]=vertex.attributes[0]; vertex.outputs[1]=vertex.attributes[1];
        vertex.output_components[0]=15; vertex.output_components[1]=15;
        shader.vertices.push_back(vertex);
    }
    void Triangle(FloatColor color=kTranslucent) {
        Vertex(1,1,color); Vertex(5,1,color); Vertex(1,5,color);
    }
    void OnePixel(FloatColor a,FloatColor b,FloatColor c,float wa=1,float wb=1,float wc=1) {
        Vertex(1,1,a,wa); Vertex(3,1,b,wb); Vertex(1,3,c,wc);
    }
    std::vector<std::uint8_t> Vram() const { const auto bytes=bank->bytes(); return {bytes.begin(),bytes.end()}; }
    std::vector<std::uint8_t> Frame() const {
        const auto bytes=bank->bytes().subspan(offset,width*height*4); return {bytes.begin(),bytes.end()};
    }
    bool Run(std::uint32_t budget=kPicaRasterMaxPixelTests) {
        const auto old_regs=regs; const auto old_uploads=uploads; const auto old_shader=shader;
        const auto old_vram=Vram(); error=nullptr;
        const bool result=StagePicaRasterizer(regs,uploads,shader,*bank,plan,error,budget);
        CHECK(regs==old_regs && uploads==old_uploads && shader==old_shader && Vram()==old_vram);
        return result;
    }
    bool Success() {
        const bool result=Run(); CHECK(result && !error);
        if (!result) { std::cerr << "Raster stop: " << (error?error:"no error") << '\n'; return false; }
        CHECK(plan.framebuffer_address==kGpuVramPhysicalBase+offset && plan.width==width && plan.height==height);
        return true;
    }
    void Stop(std::uint32_t budget=kPicaRasterMaxPixelTests) {
        const auto before=plan; CHECK(!Run(budget)); CHECK(error); CHECK(plan==before);
    }
    std::vector<std::uint8_t> StagedFrame() const {
        auto bytes=Frame();
        for (const auto& patch:plan.patches) {
            const std::uint64_t end=std::uint64_t(patch.offset)+patch.bytes.size();
            if (end<=offset || patch.offset>=std::uint64_t(offset)+bytes.size()) continue;
            if (patch.offset<offset || end>std::uint64_t(offset)+bytes.size()) { CHECK(false); continue; }
            std::copy(patch.bytes.begin(),patch.bytes.end(),bytes.begin()+(patch.offset-offset));
        }
        return bytes;
    }
    void Expect(const std::vector<Pixel>& covered,Color color) const {
        auto expected=Frame();
        for (const auto& pixel:covered) StoreColor(expected,ByteOffset(width,height,pixel[0],pixel[1]),color);
        CHECK(StagedFrame()==expected);
    }
};

void TriangleCoverageAndSharedEdge() {
    const std::vector<Pixel> six{{1,1},{2,1},{3,1},{1,2},{2,2},{1,3}};
    // A source alpha of64 produces (78,46,92); alpha91 is masked off.
    // A repeated hit on the shared edge would blend again and change the result.
    Fixture triangle; triangle.Triangle(); if (!triangle.Success()) return;
    CHECK(triangle.plan.triangles==1 && triangle.plan.pixels_covered==6 && triangle.plan.pixels_written==6);
    triangle.Expect(six,{78,46,92,91});

    Fixture strip; strip.Topology(1); strip.Triangle(); strip.Vertex(5,5);
    if (!strip.Success()) return;
    CHECK(strip.plan.triangles==2 && strip.plan.pixels_covered==16 && strip.plan.pixels_written==16);
    std::vector<Pixel> square;
    for (unsigned y=1;y<5;++y) for (unsigned x=1;x<5;++x) square.push_back({x,y});
    strip.Expect(square,{78,46,92,91});
}

void TevBlendAndChannelMasks() {
    Fixture f; const FloatColor primary{0.75f,0.25f,1,1}; f.OnePixel(primary,primary,primary);
    // Primary(191,64,255,255) modulate Constant(128,64,192,128)
    // is(95,16,192,128), using floor(product/255) in both TEV lanes.
    f.Set(0xC0,0x00E000E0); f.Set(0xC2,0x00010001); f.Set(0xC3,0x80C04080);
    if (!f.Success()) return;
    f.Expect({{1,1}},{57,27,126,91});
    f.Set(0x107,0xF00); if (!f.Success()) return;
    f.Expect({{1,1}},{57,27,126,109});
    f.Set(0x107,0x500); if (!f.Success()) return;
    f.Expect({{1,1}},{57,40,126,91});
    // LogicCopy bypasses blending; RGB and alpha are still independently masked.
    f.Set(0x100,0); f.Set(0x102,3); f.Set(0x107,0xF00);
    if (!f.Success()) return;
    f.Expect({{1,1}},{95,16,192,128});
    f.Set(0x107,0); if (!f.Success()) return;
    f.Expect({},kDestination);
    f.Set(0x107,0xF00); f.Set(0x113,0); if (!f.Success()) return;
    f.Expect({},kDestination); CHECK(f.plan.pixels_written==0);
}

void BiasedInterpolationAndPerspective() {
    constexpr FloatColor red{1,0,0,1},green{0,1,0,1},blue{0,0,1,1};
    Fixture affine; affine.Set(0x107,0xF00); affine.OnePixel(red,green,blue);
    if (!affine.Success()) return;
    CHECK(affine.plan.pixels_covered==1);
    // At(1.5,1.5), the reference's biased integer weights are511,256,256.
    // Rounding255*(511,256,256)/1023 gives127,64,64. Unbiased weights give128 red.
    affine.Expect({{1,1}},{127,64,64,255});
    Fixture perspective; perspective.Set(0x107,0xF00);
    perspective.OnePixel(red,green,blue,1,2,4);
    if (!perspective.Success()) return;
    // Perspective weights are511,128,64, with denominator703.
    perspective.Expect({{1,1}},{185,46,23,255});
    Fixture clamped; clamped.Set(0x107,0xF00);
    const FloatColor signed_color{-0.75f,1.5f,-0.25f,1};
    clamped.OnePixel(signed_color,signed_color,signed_color);
    if (!clamped.Success()) return;
    clamped.Expect({{1,1}},{191,255,64,255}); // Per-vertex abs then saturation.
}

void TiledAddressesAndViewportOffset() {
    constexpr FloatColor color{0.25f,0.75f,1,1};
    Fixture f(16,16); f.Set(0x107,0xF00); f.Topology(1);
    f.Vertex(7,7,color); f.Vertex(9,7,color); f.Vertex(7,9,color); f.Vertex(9,9,color);
    if (!f.Success()) return;
    CHECK(f.plan.pixels_covered==4);
    auto expected=f.Frame();
    // Four sides of both8x8 tile boundaries, with framebuffer y inverted:
    // screen(7,8),(8,8),(7,7),(8,7), respectively.
    for (auto byte:{252U,424U,596U,768U}) StoreColor(expected,byte,{64,191,255,255});
    CHECK(f.StagedFrame()==expected);

    Fixture shifted; shifted.Set(0x107,0xF00); shifted.Set(0x68,0x00010001);
    shifted.Set(0x41,ExactFloat24(2)); shifted.Set(0x43,ExactFloat24(2));
    // A smaller4x4 viewport remains inside the8x8 framebuffer after translation.
    shifted.Vertex(0,0,color); shifted.Vertex(4,0,color); shifted.Vertex(0,4,color);
    if (!shifted.Success()) return;
    shifted.Expect({{1,1}},{64,191,255,255});
}

void WindingStripAndDegenerates() {
    Fixture f; f.Triangle();
    for (unsigned cull:{0U,2U,3U}) {
        f.Set(0x40,cull); if (!f.Success()) return;
        CHECK(f.plan.pixels_covered==6);
    }
    f.Set(0x40,1); if (!f.Success()) return;
    CHECK(f.plan.pixels_covered==0); f.Expect({},kDestination);
    std::swap(f.shader.vertices[1],f.shader.vertices[2]);
    if (!f.Success()) return; CHECK(f.plan.pixels_covered==6);
    f.Set(0x40,2); if (!f.Success()) return;
    CHECK(f.plan.pixels_covered==0); f.Expect({},kDestination);

    Fixture strip; strip.Topology(1); strip.Set(0x40,2); strip.Triangle(); strip.Vertex(5,5);
    if (!strip.Success()) return;
    CHECK(strip.plan.pixels_covered==16); // The second strip triangle retains winding.
    strip.Set(0x40,1); if (!strip.Success()) return;
    CHECK(strip.plan.pixels_covered==0);
    Fixture degenerate; degenerate.Vertex(1,1); degenerate.Vertex(3,3); degenerate.Vertex(5,5);
    if (!degenerate.Success()) return;
    CHECK(degenerate.plan.pixels_covered==0); degenerate.Expect({},kDestination);
}

void KnownStateAndUnsupportedModes() {
    Fixture f; f.Triangle(); if (!f.Success()) return;
    // Preserve a nonempty prior output through every failure, including known
    // but unsupported state. No source register, upload, shader, or VRAM mutates.
    const std::array<std::pair<unsigned,std::uint32_t>,16> modes{{
        {0x47,1},{0x65,1},{0x80,1},{0x80,1U<<10},{0xE0,5},
        {0x100,0x101},{0x104,1},{0x105,1},{0x107,0x701},
        {0x117,1U<<16},{0xC0,3},
        {0xC1,1},{0xC2,2},{0xC4,1},{0x101,0x76760001},{0x101,0x76720000}
    }};
    for (const auto& [id,value]:modes) { const auto old=f.Get(id); f.Set(id,value); f.Stop(); f.Set(id,old); }
    f.Topology(2); f.Stop(); f.Topology(0);
    for (unsigned id:{0x11DU,0x4FU,0x50U,0xC2U}) {
        const auto old=f.uploads.register_known_bytes[id]; f.uploads.register_known_bytes[id]=0;
        f.Stop(); f.uploads.register_known_bytes[id]=old;
    }
    f.uploads.primitive_empty=false; f.Stop(); f.uploads.primitive_empty=true;
    f.shader.vertices[0].attribute_components[0]&=~8U; f.Stop();
    f.shader.vertices[0].attribute_components[0]=15;
    f.shader.vertices[1].attribute_components[1]&=~2U; f.Stop();
    f.shader.vertices[1].attribute_components[1]=15;
    f.shader.vertices.push_back(f.shader.vertices.back()); f.Stop(); f.shader.vertices.pop_back();
    if (!f.Success()) return;
}

void InvalidSpansNumericInputsAndLateFailure() {
    Fixture f; f.Triangle(); if (!f.Success()) return;
    const auto framebuffer=f.Get(0x11E),address=f.Get(0x11D);
    f.Set(0x11E,(framebuffer&~0x7FFU)|7); f.Stop();
    f.Set(0x11E,2040|(1023U<<12)|(1U<<24)); f.Stop(); // Framebuffer work/storage cap.
    f.Set(0x11E,framebuffer);
    f.Set(0x11D,(kGpuVramPhysicalBase+kGpuVramBytes-128)/8); f.Stop();
    f.Set(0x11D,address);
    const auto position=f.shader.vertices[0].attributes[0];
    for (const auto [component,bits]:std::array<std::pair<unsigned,std::uint32_t>,4>{{
            {0,Bits(2)},{2,Bits(0.5f)},{3,0},{0,0x7FC12345}}}) {
        f.shader.vertices[0].attributes[0][component]=bits; f.Stop();
        f.shader.vertices[0].attributes[0]=position;
    }
    const auto color=f.shader.vertices[0].attributes[1];
    f.shader.vertices[0].attributes[1][0]=0x7F800000; f.Stop();
    f.shader.vertices[0].attributes[1]=color;
    const auto old_rounding=std::fegetround();
    CHECK(std::fesetround(FE_UPWARD)==0); f.Stop(); CHECK(std::fesetround(old_rounding)==0);
    // A valid first triangle must never leak its staged pixels when a later
    // triangle has a nonfinite consumed coordinate.
    const auto first=f.shader.vertices;
    f.shader.vertices.insert(f.shader.vertices.end(),first.begin(),first.end());
    f.shader.vertices.back().attributes[0][0]=0x7FC12345; f.Stop();
    f.shader.vertices=first;
    // All transformed coordinates and colors are finite and normal, but the
    // second triangle's inverse-w weighted barycentric sum overflows binary32.
    // Failure occurs after the valid first triangle has already staged pixels.
    const float tiny_w=std::bit_cast<float>(0x03800000U); // 2^-120.
    f.Vertex(1,1,{1,0,0,1},tiny_w); f.Vertex(5,1,{1,0,0,1},tiny_w); f.Vertex(1,5,{1,0,0,1},tiny_w);
    f.Stop();
    CHECK(f.error && std::string_view(f.error).find("interpolation")!=std::string_view::npos);
}

void AggregateRasterWorkBound() {
    Fixture remaining; remaining.Triangle(); if (!remaining.Success()) return;
    remaining.Stop(15); // A4x4 bounding box cannot consume a15-pixel queue remainder.
    CHECK(remaining.error && std::string_view(remaining.error).find("budget")!=std::string_view::npos);
    CHECK(remaining.Run(16)); CHECK(remaining.plan.pixels_tested==16);
    Fixture f(128,128); f.Set(0x107,0xF00);
    f.Vertex(0,0,{1,0,0,1}); f.Vertex(128,0,{1,0,0,1}); f.Vertex(0,128,{1,0,0,1});
    const auto triangle=f.shader.vertices;
    const auto triangles=kPicaRasterMaxPixelTests/(128U*128U)+1;
    for (unsigned i=1;i<triangles;++i) f.shader.vertices.insert(f.shader.vertices.end(),triangle.begin(),triangle.end());
    // Every triangle and framebuffer fits individually. Their combined candidate
    // pixel count exceeds the explicit host-work limit, without integer overflow.
    f.plan.pixels_written=123; f.plan.patches.push_back({17,{3,1,4}});
    f.Stop();
    CHECK(f.error && (std::string_view(f.error).find("work")!=std::string_view::npos ||
                      std::string_view(f.error).find("pixel")!=std::string_view::npos));
}

constexpr unsigned kTextureOffset=0x3000,kDepthOffset=0x5000;

void TextureFixture(Fixture& f) {
    f.Set(0x80,1); f.Set(0x82,0x00080008); f.Set(0x83,6); f.Set(0x84,0);
    f.Set(0x85,(kGpuVramPhysicalBase+kTextureOffset)/8); f.Set(0x8E,0);
    f.Set(0x4F,3); f.Set(0x52,0x1F1F0D0C); f.shader.output_count=3;
    f.Set(0xC0,0x00030003); f.Set(0xC2,0);
    std::vector<std::uint8_t> image(256);
    for (unsigned y=0;y<8;++y) for (unsigned x=0;x<8;++x)
        StoreColor(image,ByteOffset(8,8,x,y),{static_cast<std::uint8_t>(32*x),48,96,255});
    CHECK(f.bank->Write(kTextureOffset,image));
    for (auto& vertex:f.shader.vertices) {
        vertex.attributes[2]={Bits(0.5f),Bits(0.5f),0,0};
        vertex.attribute_components[2]=3;
        vertex.outputs[2]=vertex.attributes[2]; vertex.output_components[2]=3;
    }
}

void TexturePerspectiveTevAndFeedback() {
    constexpr FloatColor white{1,1,1,1};
    Fixture f; f.OnePixel(white,white,white,1,2,4); TextureFixture(f);
    f.Set(0x100,0); f.Set(0x107,0xF00);
    for (unsigned vertex=0;vertex<3;++vertex)
        f.shader.vertices[vertex].attributes[2][0]=Bits(vertex==1?1.0f:0.0f);
    if (!f.Success()) return;
    // At the sole covered center, perspective U=128/703. Bilinear sampling the
    // authored red ramp yields31; affine UV interpolation would yield48.
    f.Expect({{1,1}},{31,48,96,255});
    CHECK(f.plan.texture_reads.size()==1);
    CHECK((f.plan.texture_reads[0]==PicaTextureReadSpan{kGpuVramPhysicalBase+kTextureOffset,256}));
    f.shader.vertices[0].attribute_components[2]&=~2U; f.Stop();
    f.shader.vertices[0].attribute_components[2]=3;
    f.Set(0x85,(kGpuVramPhysicalBase+f.offset)/8); f.Stop();
    f.Set(0x85,(kGpuVramPhysicalBase+kTextureOffset)/8);

    // Nontrivial texture alpha participates in both TEV lanes, blending and the
    // destination alpha mask. Every texel is authored RGBA(128,64,192,128).
    std::vector<std::uint8_t> image(256);
    for (unsigned i=0;i<64;++i) StoreColor(image,4*i,{128,64,192,128});
    CHECK(f.bank->Write(kTextureOffset,image));
    for (auto& vertex:f.shader.vertices) vertex.attributes[1]={Bits(0.75f),Bits(0.25f),Bits(1),Bits(1)};
    f.Set(0xC8,0x000F000F); f.Set(0xCA,0x00010001);
    f.Set(0x100,0x100); f.Set(0x107,0x700);
    if (!f.Success()) return;
    f.Expect({{1,1}},{57,27,126,91});
}

std::vector<std::uint8_t> DepthFixture(Fixture& f,std::uint32_t value=0xFFFFFFU) {
    f.Set(0x107,0x1751); f.Set(0x114,2); f.Set(0x115,2); f.Set(0x116,3);
    f.Set(0x11C,(kGpuVramPhysicalBase+kDepthOffset)/8);
    std::vector<std::uint8_t> bytes(f.width*f.height*4);
    for (unsigned i=0;i<f.width*f.height;++i) {
        for (unsigned b=0;b<3;++b) bytes[4*i+b]=static_cast<std::uint8_t>(value>>(8*b));
        bytes[4*i+3]=static_cast<std::uint8_t>(i^0xA5U);
    }
    CHECK(f.bank->Write(kDepthOffset,bytes)); return bytes;
}
std::vector<std::uint8_t> StagedDepth(const Fixture& f) {
    const auto source=f.bank->bytes().subspan(kDepthOffset,f.width*f.height*4);
    std::vector<std::uint8_t> result(source.begin(),source.end());
    for (const auto& patch:f.plan.patches)
        if (patch.offset==kDepthOffset) { CHECK(patch.bytes.size()==result.size()); result=patch.bytes; }
    return result;
}

void DepthWritesComparisonsAndStencil() {
    const std::vector<Pixel> six{{1,1},{2,1},{3,1},{1,2},{2,2},{1,3}};
    Fixture f; f.Triangle(); auto expected=DepthFixture(f);
    if (!f.Success()) return;
    CHECK(f.plan.depthbuffer_address==kGpuVramPhysicalBase+kDepthOffset);
    CHECK(f.plan.depth_pixels_written==6 && f.plan.patches.size()==2);
    for (const auto& p:six) {
        const auto offset=ByteOffset(8,8,p[0],p[1]);
        expected[offset]=255;expected[offset+1]=255;expected[offset+2]=127;
    }
    CHECK(StagedDepth(f)==expected); // Uncovered depth and every stencil byte survive.
    f.Expect(six,{78,46,92,91});

    constexpr std::array<std::uint32_t,3> depths{0x7FFFFE,0x7FFFFF,0x800000};
    constexpr std::array<std::array<bool,8>,3> passes{{
        {false,true,false,true,false,false,true,true},
        {false,true,true,false,false,true,false,true},
        {false,true,false,true,true,true,false,false}}};
    for (unsigned relation=0;relation<3;++relation) {
        const auto unchanged=DepthFixture(f,depths[relation]);
        for (unsigned comparison=0;comparison<8;++comparison) {
            f.Set(0x107,0x701|(comparison<<4)); // Compare without writing depth.
            if (!f.Success()) return;
            CHECK(f.plan.pixels_written==(passes[relation][comparison]?6U:0U));
            CHECK(f.plan.depth_pixels_written==0 && StagedDepth(f)==unchanged);
        }
    }
    const auto old=DepthFixture(f);
    f.Set(0x115,0); if (!f.Success()) return;
    CHECK(f.plan.pixels_written==6 && f.plan.depth_pixels_written==0 && StagedDepth(f)==old);
    f.Set(0x115,2); f.Set(0x107,0x1000); f.Set(0x113,0); f.Set(0x114,0);
    if (!f.Success()) return;
    CHECK(f.plan.pixels_written==0 && f.plan.depth_pixels_written==6 && f.plan.patches.size()==1);
    CHECK(StagedDepth(f)==expected); // Depth writes also work without testing/color writes.
}

void DepthInterpolationAndRejection() {
    constexpr FloatColor white{1,1,1,1};
    Fixture f; f.OnePixel(white,white,white,1,2,4); auto expected=DepthFixture(f);
    f.shader.vertices[0].attributes[0][2]=Bits(-0.25f);
    f.shader.vertices[1].attributes[0][2]=Bits(-1.0f);
    f.shader.vertices[2].attributes[0][2]=Bits(-3.0f);
    if (!f.Success()) return;
    // Z/w is affine: weights(511,256,256) applied to(.25,.5,.75), divided by1023.
    // Explicit binary32 viewport/quantization produces0x700C02. Perspective
    // interpolation of Z/w would give a different result when clip W differs.
    const auto offset=ByteOffset(8,8,1,1);
    expected[offset]=0x02; expected[offset+1]=0x0C; expected[offset+2]=0x70;
    CHECK(StagedDepth(f)==expected && f.plan.depth_pixels_written==1);
    for (const auto [id,value]:std::array<std::pair<unsigned,std::uint32_t>,6>{{
            {0x116,2},{0x6D,0},{0x114,1},{0x115,1},
            {0x11C,(kGpuVramPhysicalBase+f.offset)/8},{0x4D,0x7F0000}}}) {
        const auto before=f.Get(id); f.Set(id,value); f.Stop(); f.Set(id,before);
    }
    for (unsigned id:{0x116U,0x114U,0x11CU,0x4DU}) {
        const auto known=f.uploads.register_known_bytes[id]; f.uploads.register_known_bytes[id]=0;
        f.Stop(); f.uploads.register_known_bytes[id]=known;
    }
    // A texture may not sample the depth buffer while the draw writes it.
    TextureFixture(f); f.Set(0x85,(kGpuVramPhysicalBase+kDepthOffset)/8); f.Stop();
}
} // namespace

int main() {
    TriangleCoverageAndSharedEdge(); TevBlendAndChannelMasks(); BiasedInterpolationAndPerspective();
    TiledAddressesAndViewportOffset(); WindingStripAndDegenerates(); KnownStateAndUnsupportedModes();
    InvalidSpansNumericInputsAndLateFailure(); AggregateRasterWorkBound();
    TexturePerspectiveTevAndFeedback(); DepthWritesComparisonsAndStencil(); DepthInterpolationAndRejection();
    if (failures) return EXIT_FAILURE;
    std::cout << "PASS: authored coverage, perspective color/UV, texture TEV/blending, D24S8 comparisons/stencil preservation, tiled bytes and atomic bounds\n";
    return EXIT_SUCCESS;
}
