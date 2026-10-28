#include "services/pica_texture.h"
#include "services/gsp_display_transfer.h"
#include "runtime/ctr_memory.h"
#include <algorithm>
#include <array>
#include <cfenv>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <tuple>
#include <utility>
#include <vector>

namespace { bool fail_allocation{}; }
void* operator new(std::size_t size) {
    if (fail_allocation) throw std::bad_alloc();
    if (auto* p=std::malloc(size?size:1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}

using namespace lego::ctr;
namespace {
int failures{};
#define CHECK(x) do {if (!(x)) {std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}} while (0)
using Color=std::array<std::uint8_t,4>;
constexpr auto kRw=MemoryPermission::Read|MemoryPermission::Write;
constexpr std::uint32_t kOffset=0x2000,kPhysical=kGpuVramPhysicalBase+kOffset;
constexpr std::uint32_t kPrivateVa=0x14001000,kPrivatePhysical=0x20001000;
constexpr Color kA{10,30,50,70},kB{110,50,90,130},kC{30,130,150,210},kD{250,230,190,10};
constexpr Color kLow{1,2,3,4},kHighX{231,117,59,193},kHighY{9,17,33,65},kHighBoth{13,37,81,155};

void Store(std::vector<std::uint8_t>& bytes,unsigned offset,Color rgba) {
    CHECK(offset+4<=bytes.size());
    if (offset+4>bytes.size())return;
    for (unsigned c=0;c<4;++c)bytes[offset+c]=rgba[3-c];
}
std::vector<std::uint8_t> AuthoredTexture() {
    std::vector<std::uint8_t> bytes(16*16*4,0xCD);
    // Literal offsets independently specify ABGR texels in four 8x8 tiles.
    // Logical (7,7),(8,7),(7,8),(8,8) straddle both tile axes after Y inversion.
    Store(bytes,596,kA);Store(bytes,768,kB);Store(bytes,252,kC);Store(bytes,424,kD);
    // Logical corners (0,0),(15,0),(0,15),(15,15).
    Store(bytes,680,kLow);Store(bytes,1020,kHighX);Store(bytes,0,kHighY);Store(bytes,340,kHighBoth);
    // Adjacent Morton positions at logical (1,0),(7,0),(8,0),(0,7),(0,8).
    Store(bytes,684,{17,29,43,61});Store(bytes,764,{71,83,97,109});
    Store(bytes,936,{127,139,151,163});Store(bytes,512,{173,181,193,211});
    Store(bytes,168,{223,229,233,239});
    return bytes;
}
std::vector<std::uint8_t> AuthoredIa8Texture() {
    std::vector<std::uint8_t> bytes(512,0xCD);
    const auto put=[&](unsigned offset,std::uint8_t intensity,std::uint8_t alpha) {
        bytes[offset]=alpha;bytes[offset+1]=intensity;
    };
    // Literal two-byte offsets for logical (7,7),(8,7),(7,8),(8,8).
    // All four 8x8 tiles are involved; alpha and intensity deliberately differ.
    put(298,10,20);put(384,110,100);put(126,30,200);put(212,250,40);
    // Logical (0,0),(1,0),(0,7),(0,8),(15,15), including transparent nonzero I.
    put(340,17,231);put(342,31,0);put(256,177,33);put(84,229,119);put(170,67,149);
    return bytes;
}

void StoreLittle64(std::vector<std::uint8_t>& bytes,unsigned offset,std::uint64_t value) {
    CHECK(offset+8<=bytes.size());
    if (offset+8>bytes.size())return;
    for (unsigned i=0;i<8;++i)bytes[offset+i]=static_cast<std::uint8_t>(value>>(8*i));
}
std::vector<std::uint8_t> AuthoredEtcBlock(bool alpha,std::uint64_t rgb,
                                        std::uint64_t a=0xFEDCBA9876543210ULL) {
    // One8x8 tile, with four identical hand-authored4x4 blocks. PICA stores
    // little-endian alpha before RGB; no production encoder/decoder is used.
    std::vector<std::uint8_t> bytes(alpha?64:32);
    for (unsigned offset=0;offset<bytes.size();offset+=alpha?16:8) {
        if (alpha)StoreLittle64(bytes,offset,a);
        StoreLittle64(bytes,offset+(alpha?8:0),rgb);
    }
    return bytes;
}
std::vector<std::uint8_t> AuthoredEtcSeams(bool alpha) {
    std::vector<std::uint8_t> bytes(alpha?256:128);
    if (alpha)for (unsigned offset=0;offset<bytes.size();offset+=16)
        StoreLittle64(bytes,offset,0xFFFFFFFFFFFFFFFFULL);
    // Literal block offsets for logical(7,7),(8,7),(7,8),(8,8), spanning
    // four8x8 tiles after Y inversion. Both partitions use the same authored
    // base; modifier+2 gives RGB19,53,87,121. Alpha is17,85,153,221.
    constexpr std::array<unsigned,4> with_alpha{144,192,48,96},opaque{72,96,24,48};
    constexpr std::array<std::uint64_t,4> colors{
        0x1111110000000000ULL,0x3333330000000000ULL,
        0x5555550000000000ULL,0x7777770000000000ULL};
    constexpr std::array<std::uint64_t,4> alphas{
        0x1111111111111111ULL,0x5555555555555555ULL,
        0x9999999999999999ULL,0xDDDDDDDDDDDDDDDDULL};
    for (unsigned i=0;i<4;++i) {
        const auto offset=alpha?with_alpha[i]:opaque[i];
        if (alpha)StoreLittle64(bytes,offset,alphas[i]);
        StoreLittle64(bytes,offset+(alpha?8:0),colors[i]);
    }
    return bytes;
}

struct Fixture {
    GuestMemory memory;
    PicaGpuRegisters regs{};
    PicaUploadState uploads{};
    std::shared_ptr<GpuVramBank> bank=GpuVramBank::ReferenceZero();
    PicaTexturePlan plan;
    const char* error{};
    Fixture() {
        Set(0x82,0x00100010);Set(0x83,6,11);Set(0x84,0);Set(0x85,kPhysical/8);Set(0x8E,0,1);
        CHECK(bank->Write(kOffset,AuthoredTexture()));
        plan.width=99;plan.height=77;plan.read={17,3};plan.bytes={3,1,4};
    }
    void Set(unsigned id,std::uint32_t value,unsigned mask=15) {
        // The normal command decoder establishes byte-level provenance.
        const auto header=id|(mask<<16);std::array<std::uint8_t,8> list{};
        for (unsigned b=0;b<4;++b) {
            list[b]=static_cast<std::uint8_t>(value>>(8*b));
            list[4+b]=static_cast<std::uint8_t>(header>>(8*b));
        }
        PicaListPlan staged;CHECK(StagePicaStartupList(list,regs,uploads,staged));
        regs=staged.registers;uploads=staged.uploads;
    }
    bool Run(GuestMemory* source=nullptr) {
        const auto before_regs=regs;const auto before_uploads=uploads;
        const std::vector<std::uint8_t> before_vram(bank->bytes().begin(),bank->bytes().end());
        const bool ok=StagePicaTexture0(regs,uploads,source,*bank,plan,error);
        CHECK(regs==before_regs && uploads==before_uploads);
        CHECK(std::equal(before_vram.begin(),before_vram.end(),bank->bytes().begin()));
        return ok;
    }
    bool Good(GuestMemory* source=nullptr) {
        const auto ok=Run(source);CHECK(ok && !error);
        if (!ok)std::cerr<<"Texture stage: "<<(error?error:"missing error")<<'\n';
        return ok;
    }
    void Stop(GuestMemory* source=nullptr) {
        const auto before=plan;CHECK(!Run(source));CHECK(error && plan==before);
    }
};
void Sample(const PicaTexturePlan& plan,float u,float v,Color expected) {
    const auto before=plan;Color color{0xF1,0xE2,0xD3,0xC4};const char* error="old";
    CHECK(SamplePicaTexture(plan,u,v,color,error));CHECK(!error && color==expected && plan==before);
}
void RejectSample(const PicaTexturePlan& plan,float u,float v) {
    const auto before=plan;Color color{0xF1,0xE2,0xD3,0xC4};const auto saved=color;const char* error=nullptr;
    CHECK(!SamplePicaTexture(plan,u,v,color,error));CHECK(error && color==saved && plan==before);
}

void MortonNearestAndClamp() {
    Fixture f;f.Set(0x83,0,11);if (!f.Good())return;
    CHECK(f.plan.width==16 && f.plan.height==16 && f.plan.filter==PicaTextureFilter::Nearest);
    CHECK(f.plan.format==PicaTextureFormat::RGBA8);
    CHECK((f.plan.read==PicaTextureReadSpan{kPhysical,1024}));
    CHECK(f.plan.bytes==AuthoredTexture());
    for (auto [u,v,color]:std::array<std::tuple<float,float,Color>,9>{{
        {7.5f/16,7.5f/16,kA},{8.5f/16,7.5f/16,kB},
        {7.5f/16,8.5f/16,kC},{0.5f,0.5f,kD},
        {1.5f/16,0.5f/16,{17,29,43,61}},
        {7.5f/16,0.5f/16,{71,83,97,109}},
        {8.5f/16,0.5f/16,{127,139,151,163}},
        {0.5f/16,7.5f/16,{173,181,193,211}},
        {0.5f/16,8.5f/16,{223,229,233,239}}}})Sample(f.plan,u,v,color);
    Sample(f.plan,0,0,kLow);Sample(f.plan,1,0,kHighX);
    Sample(f.plan,0,1,kHighY);Sample(f.plan,1,1,kHighBoth);
    Sample(f.plan,-4,-2,kLow);Sample(f.plan,4,2,kHighBoth);
}

void LinearGoldensAndPureSampling() {
    Fixture f;if (!f.Good())return;
    CHECK(f.plan.filter==PicaTextureFilter::Linear);
    Sample(f.plan,7.5f/16,7.5f/16,kA); // Exact texel center.
    Sample(f.plan,0.5f,7.5f/16,{60,40,70,100}); // Two horizontal taps.
    Sample(f.plan,0.5f,0.5f,{100,110,120,105}); // Four tile-corner taps.
    Sample(f.plan,7.75f/16,8.25f/16,{73,125,135,141}); // Weights1/4,3/4 and explicit half-up rounding.
    Sample(f.plan,0,0,kLow);Sample(f.plan,1,0,kHighX);
    Sample(f.plan,0,1,kHighY);Sample(f.plan,1,1,kHighBoth);
    Sample(f.plan,-0.25f,1.25f,kHighY);
    Sample(f.plan,-std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),kHighY);

    const auto saved=f.plan;Color output{};const char* error=nullptr;
    fail_allocation=true;
    const bool ok=SamplePicaTexture(f.plan,0.5f,0.5f,output,error);
    fail_allocation=false;
    CHECK(ok && !error && output==Color({100,110,120,105}) && f.plan==saved);
    // The source snapshot stays authoritative after the live texture is changed.
    CHECK(f.bank->Write(kOffset,std::vector<std::uint8_t>(1024,0)));
    Sample(f.plan,0.5f,0.5f,{100,110,120,105});
}

void PhysicalSourceEquivalenceAndLimits() {
    Fixture f;if (!f.Good())return;const auto device_plan=f.plan;
    CHECK(f.memory.Map(kPrivateVa,1024,kRw));CHECK(f.memory.LoadBytes(kPrivateVa,AuthoredTexture()));
    f.Set(0x85,kPrivatePhysical/8);if (!f.Good(&f.memory))return;
    CHECK((f.plan.read==PicaTextureReadSpan{kPrivatePhysical,1024}));
    CHECK(f.plan.width==device_plan.width && f.plan.height==device_plan.height &&
          f.plan.filter==device_plan.filter && f.plan.bytes==device_plan.bytes);
    Sample(f.plan,0.5f,0.5f,{100,110,120,105});
    for (unsigned i=0;i<1024;++i) {std::uint8_t value{};CHECK(f.memory.Read8(kPrivateVa+i,&value));CHECK(value==f.plan.bytes[i]);}
    CHECK(f.memory.ZeroBytes(kPrivateVa,1024));Sample(f.plan,0.5f,0.5f,{100,110,120,105});

    f.Set(0x85,kPhysical/8);
    CHECK(f.memory.Map(kGpuVramVirtualBase+kOffset,1024,kRw));
    CHECK(f.memory.ZeroBytes(kGpuVramVirtualBase+kOffset,1024));
    if (f.Good(&f.memory))Sample(f.plan,0.5f,0.5f,{100,110,120,105});
    // The maximum allowed extent stages exactly4MiB; an8x8 extent also works.
    f.Set(0x82,0x04000400);CHECK(f.Good());CHECK(f.plan.bytes.size()==kPicaTextureMaxBytes);
    f.Set(0x82,0x00080008);CHECK(f.Good());CHECK(f.plan.bytes.size()==256);
    f.Set(0x82,0x00100010);f.Set(0x85,(kGpuVramPhysicalBase+kGpuVramBytes-512)/8);f.Stop();
    f.Set(0x85,0x27FFFF00U/8);f.Stop(&f.memory);
    f.Set(0x85,0x08000000U/8);f.Stop(&f.memory);
}

void Ia8DecodeAndFiltering() {
    Fixture f;f.Set(0x8E,5,1);f.Set(0x83,0,11);
    CHECK(f.bank->Write(kOffset,AuthoredIa8Texture()));if (!f.Good())return;
    CHECK(f.plan.format==PicaTextureFormat::IA8 && f.plan.bytes==AuthoredIa8Texture());
    CHECK((f.plan.read==PicaTextureReadSpan{kPhysical,512}));
    for (auto [u,v,color]:std::array<std::tuple<float,float,Color>,8>{{
        {7.5f/16,7.5f/16,{10,10,10,20}},{8.5f/16,7.5f/16,{110,110,110,100}},
        {7.5f/16,8.5f/16,{30,30,30,200}},{0.5f,0.5f,{250,250,250,40}},
        {1.5f/16,0.5f/16,{31,31,31,0}},{0.5f/16,7.5f/16,{177,177,177,33}},
        {0.5f/16,8.5f/16,{229,229,229,119}},{1,1,{67,67,67,149}}}})Sample(f.plan,u,v,color);
    Sample(f.plan,0,0,{17,17,17,231});
    f.Set(0x83,6,11);if (!f.Good())return;
    Sample(f.plan,0.5f,0.5f,{100,100,100,90});
    Sample(f.plan,7.75f/16,8.25f/16,{73,73,73,130}); // Independent I/A interpolation.
    Sample(f.plan,1.5f/16,0.5f/16,{31,31,31,0}); // Intensity is not premultiplied by alpha.
}

void Ia8EncodedExtentAndFailures() {
    Fixture f;f.Set(0x8E,5,1);const auto texture=AuthoredIa8Texture();
    CHECK(f.memory.Map(kPrivateVa,512,kRw));CHECK(f.memory.LoadBytes(kPrivateVa,texture));
    CHECK(!f.memory.IsMapped(kPrivateVa+512));f.Set(0x85,kPrivatePhysical/8);
    if (!f.Good(&f.memory))return;
    CHECK((f.plan.read==PicaTextureReadSpan{kPrivatePhysical,512}) && f.plan.bytes==texture);
    Sample(f.plan,0.5f,0.5f,{100,100,100,90});
    GuestMemory short_source;CHECK(short_source.Map(kPrivateVa,511,kRw));f.Stop(&short_source);

    const auto end_address=kGpuVramPhysicalBase+kGpuVramBytes-512;
    CHECK(f.bank->Write(kGpuVramBytes-512,texture));f.Set(0x85,end_address/8);
    if (!f.Good())return;
    CHECK((f.plan.read==PicaTextureReadSpan{end_address,512}) && f.plan.bytes==texture);
    Sample(f.plan,0.5f,0.5f,{100,100,100,90});
    f.Set(0x85,(end_address+8)/8);f.Stop(); // Eight bytes beyond exact device end.
    const auto saved=f.plan;
    f.plan.bytes.pop_back();RejectSample(f.plan,0.5f,0.5f);f.plan=saved;
    f.plan.format=PicaTextureFormat::RGBA8;RejectSample(f.plan,0.5f,0.5f);f.plan=saved;
    f.plan.read.bytes=1024;RejectSample(f.plan,0.5f,0.5f);f.plan=saved;
}

void EtcWordsModesAndAlpha() {
    Fixture f;f.Set(0x82,0x00080008);f.Set(0x83,0,11);f.Set(0x8E,13,1);
    // Individual bases(17,34,51)/(68,85,102), tables0/1. Four authored
    // selectors exercise +2,+8,-2,-8. These words contain no game data.
    CHECK(f.bank->Write(kOffset,AuthoredEtcBlock(true,0x14253604000C000AULL)));
    if (!f.Good())return;
    CHECK(f.plan.format==PicaTextureFormat::ETC1A4 && f.plan.bytes.size()==64);
    Sample(f.plan,0.5f/8,7.5f/8,{19,36,53,0});
    Sample(f.plan,0.5f/8,6.5f/8,{25,42,59,17});
    Sample(f.plan,0.5f/8,5.5f/8,{15,32,49,34});
    Sample(f.plan,0.5f/8,4.5f/8,{9,26,43,51});
    Sample(f.plan,2.5f/8,7.5f/8,{73,90,107,136});

    // Flip changes the partition, never the selector or alpha index.
    CHECK(f.bank->Write(kOffset,AuthoredEtcBlock(true,0x14253605000C000AULL)));
    if (!f.Good())return;
    Sample(f.plan,0.5f/8,5.5f/8,{63,80,97,34});
    Sample(f.plan,2.5f/8,7.5f/8,{19,36,53,136});

    // Differential bases(10,12,14), deltas(-4,+3,-1): expand5bits only
    // after adding. Both differential flip values have literal RGB goldens.
    CHECK(f.bank->Write(kOffset,AuthoredEtcBlock(true,0x5463770600000000ULL)));
    if (!f.Good())return;
    Sample(f.plan,0.5f/8,7.5f/8,{84,101,117,0});
    Sample(f.plan,2.5f/8,7.5f/8,{54,128,112,136});
    CHECK(f.bank->Write(kOffset,AuthoredEtcBlock(true,0x5463770700000000ULL)));
    if (!f.Good())return;
    Sample(f.plan,0.5f/8,5.5f/8,{54,128,112,34});

    // Table7 large modifiers(-183,+183) saturate at both byte endpoints.
    CHECK(f.bank->Write(kOffset,AuthoredEtcBlock(true,0x0F0F0FFC00010101ULL)));
    if (!f.Good())return;
    Sample(f.plan,0.5f/8,7.5f/8,{0,0,0,0});
    Sample(f.plan,2.5f/8,7.5f/8,{255,255,255,136});
    f.Set(0x8E,12,1);
    CHECK(f.bank->Write(kOffset,AuthoredEtcBlock(false,0x14253604000C000AULL)));
    if (!f.Good())return;
    CHECK(f.plan.format==PicaTextureFormat::ETC1 && f.plan.bytes.size()==32);
    Sample(f.plan,0.5f/8,7.5f/8,{19,36,53,255});
    Sample(f.plan,0.5f/8,4.5f/8,{9,26,43,255});
}

void EtcTilesFiltersAndEncodedSpans() {
    for (const unsigned format:{12U,13U}) {
        const bool alpha=format==13;const auto texture=AuthoredEtcSeams(alpha);
        const unsigned count=alpha?256:128;
        Fixture f;f.Set(0x8E,format,1);f.Set(0x83,0,11);
        CHECK(f.bank->Write(kOffset,texture));if (!f.Good())return;
        CHECK(f.plan.bytes==texture && f.plan.read.bytes==count);
        Sample(f.plan,7.5f/16,7.5f/16,{19,19,19,static_cast<std::uint8_t>(alpha?17:255)});
        Sample(f.plan,8.5f/16,7.5f/16,{53,53,53,static_cast<std::uint8_t>(alpha?85:255)});
        Sample(f.plan,7.5f/16,8.5f/16,{87,87,87,static_cast<std::uint8_t>(alpha?153:255)});
        Sample(f.plan,8.5f/16,8.5f/16,{121,121,121,static_cast<std::uint8_t>(alpha?221:255)});
        f.Set(0x83,6,11);if (!f.Good())return;
        Sample(f.plan,0.5f,7.5f/16,{36,36,36,static_cast<std::uint8_t>(alpha?51:255)});
        Sample(f.plan,0.5f,0.5f,{70,70,70,static_cast<std::uint8_t>(alpha?119:255)});
        Sample(f.plan,7.75f/16,8.25f/16,{79,79,79,static_cast<std::uint8_t>(alpha?136:255)});
        Sample(f.plan,-4,-2,{2,2,2,255});Sample(f.plan,4,2,{2,2,2,255});

        // Exact encoded backing must suffice: ETC1 occupies half a byte per
        // texel, ETC1A4 one byte, despite producing four-channel samples.
        CHECK(f.memory.Map(kPrivateVa,count,kRw));CHECK(f.memory.LoadBytes(kPrivateVa,texture));
        CHECK(!f.memory.IsMapped(kPrivateVa+count));f.Set(0x85,kPrivatePhysical/8);
        if (!f.Good(&f.memory))return;
        CHECK((f.plan.read==PicaTextureReadSpan{kPrivatePhysical,count}) && f.plan.bytes==texture);
        GuestMemory short_source;CHECK(short_source.Map(kPrivateVa,count-1,kRw));f.Stop(&short_source);
        const auto end_address=kGpuVramPhysicalBase+kGpuVramBytes-count;
        CHECK(f.bank->Write(kGpuVramBytes-count,texture));f.Set(0x85,end_address/8);
        if (!f.Good())return;
        CHECK(f.plan.bytes==texture && f.plan.read.bytes==count);
        f.Set(0x85,(end_address+8)/8);f.Stop();
        const auto saved=f.plan;
        f.plan.bytes.pop_back();RejectSample(f.plan,0.5f,0.5f);f.plan=saved;
        ++f.plan.read.bytes;RejectSample(f.plan,0.5f,0.5f);f.plan=saved;
        f.plan.format=alpha?PicaTextureFormat::ETC1:PicaTextureFormat::ETC1A4;
        RejectSample(f.plan,0.5f,0.5f);
    }
}

void EtcUndefinedDifferentialIsAtomic() {
    // Underflow/overflow in each of R,G,B. Every sample is in the FIRST
    // partition: an invalid unused second endpoint still invalidates the block.
    constexpr std::array<std::uint64_t,6> invalid{
        0x0700000200000000ULL,0xF900000200000000ULL,
        0x0007000200000000ULL,0x00F9000200000000ULL,
        0x0000070200000000ULL,0x0000F90200000000ULL};
    for (const unsigned format:{12U,13U}) {
        Fixture f;f.Set(0x82,0x00080008);f.Set(0x83,0,11);f.Set(0x8E,format,1);
        for (const auto word:invalid) {
            CHECK(f.bank->Write(kOffset,AuthoredEtcBlock(format==13,word)));
            if (!f.Good())return; // Staging retains encoded storage; taps validate blocks.
            RejectSample(f.plan,0.5f/8,7.5f/8);
        }
    }
    Fixture f;f.Set(0x82,0x00080008);f.Set(0x83,0,11);f.Set(0x8E,13,1);
    auto mixed=AuthoredEtcBlock(true,0x1111110000000000ULL,0xFFFFFFFFFFFFFFFFULL);
    StoreLittle64(mixed,24,invalid[0]); // Second4x4 block, after its alpha word.
    CHECK(f.bank->Write(kOffset,mixed));if (!f.Good())return;
    Sample(f.plan,0.5f/8,7.5f/8,{19,19,19,255}); // Unvisited invalid block has no effect.
    f.Set(0x83,6,11);if (!f.Good())return;
    RejectSample(f.plan,0.5f,7.5f/8); // First tap valid, second invalid: no partial color.
}

struct ReadProbe final : DeviceMemory {
    mutable unsigned probes{},reads{};
    std::uint32_t size() const noexcept override {return 0x1000;}
    bool CanRead(std::uint32_t,std::uint32_t) const noexcept override {++probes;return true;}
    bool CanWrite(std::uint32_t,std::uint32_t) const noexcept override {return false;}
    bool Read(std::uint32_t,std::span<std::uint8_t>) const noexcept override {++reads;return true;}
    bool Write(std::uint32_t,std::span<const std::uint8_t>) noexcept override {return false;}
    std::uint64_t Epoch(std::uint32_t) const noexcept override {return 0;}
};
void UnavailableSourcesStayAtomic() {
    Fixture f;f.Set(0x85,kPrivatePhysical/8);f.Stop();f.Stop(&f.memory);
    GuestMemory short_source;CHECK(short_source.Map(kPrivateVa,1020,kRw));f.Stop(&short_source);
    GuestMemory pieces;CHECK(pieces.Map(kPrivateVa,512,kRw));CHECK(pieces.Map(kPrivateVa+512,512,kRw));f.Stop(&pieces);
    GuestMemory denied;CHECK(denied.Map(kPrivateVa,1024,MemoryPermission::Write));f.Stop(&denied);
    GuestMemory shared;auto page=std::make_shared<ServiceSharedMemoryObject>();
    CHECK(shared.MapSharedServicePage(kPrivateVa,page,kRw));f.Stop(&shared);
    GuestMemory callbacks;auto device=std::make_shared<ReadProbe>();
    CHECK(callbacks.MapDeviceMemory(kPrivateVa,device,MemoryPermission::Read));f.Stop(&callbacks);
    CHECK(device->probes==0 && device->reads==0);
}

void KnownRegistersAndUnsupportedConfigurations() {
    Fixture f;
    // Configuration need not know border color or unconsumed sampler byte2.
    CHECK(f.uploads.register_known_bytes[0x81]==0 && f.uploads.register_known_bytes[0x83]==11);
    CHECK(f.Good());
    for (auto [id,mask]:std::array<std::pair<unsigned,unsigned>,5>{{
        {0x82,15},{0x83,11},{0x84,15},{0x85,15},{0x8E,1}}}) {
        const auto before=f.uploads.register_known_bytes[id];
        for (unsigned byte=0;byte<4;++byte)if (mask&(1U<<byte)) {
            f.uploads.register_known_bytes[id]=static_cast<std::uint8_t>(before&~(1U<<byte));f.Stop();
        }
        f.uploads.register_known_bytes[id]=before;
    }
    for (const auto config: {2U,4U,6U|(1U<<8),6U|(1U<<12),6U|(1U<<24),6U|(1U<<28),6U|(3U<<28)}) {
        f.Set(0x83,config);f.Stop();
    }
    f.Set(0x83,6);
    for (const auto lod:{1U,1U<<16,1U<<24}) {f.Set(0x84,lod);f.Stop();}
    f.Set(0x84,0);
    for (const auto format:{1U,9U,15U}) {f.Set(0x8E,format);f.Stop();}
    f.Set(0x8E,0);
    for (const auto size:{0U,0x00040008U,0x000C0010U,0x0010000CU,0x06000400U}) {
        f.Set(0x82,size);f.Stop();
    }
}

void InvalidSamplesStayAtomic() {
    Fixture f;if (!f.Good())return;
    for (const auto value: {std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),
                           std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::denorm_min()}) {
        RejectSample(f.plan,value,0.5f);RejectSample(f.plan,0.5f,value);
    }
    const auto saved=f.plan;
    f.plan.bytes.pop_back();RejectSample(f.plan,0.5f,0.5f);f.plan=saved;
    --f.plan.read.bytes;RejectSample(f.plan,0.5f,0.5f);f.plan=saved;
    f.plan.filter=static_cast<PicaTextureFilter>(99);RejectSample(f.plan,0.5f,0.5f);f.plan=saved;
    f.plan.format=static_cast<PicaTextureFormat>(99);RejectSample(f.plan,0.5f,0.5f);f.plan=saved;
    f.plan.width=12;RejectSample(f.plan,0.5f,0.5f);f.plan=saved;
    const auto rounding=std::fegetround();CHECK(std::fesetround(FE_DOWNWARD)==0);
    RejectSample(f.plan,0.5f,0.5f);CHECK(std::fesetround(rounding)==0);
}
} // namespace

int main() {
    MortonNearestAndClamp();LinearGoldensAndPureSampling();PhysicalSourceEquivalenceAndLimits();
    Ia8DecodeAndFiltering();Ia8EncodedExtentAndFailures();
    EtcWordsModesAndAlpha();EtcTilesFiltersAndEncodedSpans();EtcUndefinedDifferentialIsAtomic();
    UnavailableSourcesStayAtomic();KnownRegistersAndUnsupportedConfigurations();InvalidSamplesStayAtomic();
    if (failures)return EXIT_FAILURE;
    std::cout<<"PASS: authored RGBA8/IA8 texture bytes, Morton/linear sampling, immutable physical reads and atomic bounds\n";
}
