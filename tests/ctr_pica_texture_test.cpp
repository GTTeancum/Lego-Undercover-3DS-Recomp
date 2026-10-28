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
    for (const auto format:{1U,13U,15U}) {f.Set(0x8E,format);f.Stop();}
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
    f.plan.width=12;RejectSample(f.plan,0.5f,0.5f);f.plan=saved;
    const auto rounding=std::fegetround();CHECK(std::fesetround(FE_DOWNWARD)==0);
    RejectSample(f.plan,0.5f,0.5f);CHECK(std::fesetround(rounding)==0);
}
} // namespace

int main() {
    MortonNearestAndClamp();LinearGoldensAndPureSampling();PhysicalSourceEquivalenceAndLimits();
    UnavailableSourcesStayAtomic();KnownRegistersAndUnsupportedConfigurations();InvalidSamplesStayAtomic();
    if (failures)return EXIT_FAILURE;
    std::cout<<"PASS: authored texture Morton/ABGR, nearest/linear clamp, immutable physical reads and atomic bounds\n";
}
