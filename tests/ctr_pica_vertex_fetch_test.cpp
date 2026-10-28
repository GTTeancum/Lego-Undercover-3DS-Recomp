#include "services/pica_vertex_fetch.h"
#include "services/gsp_display_transfer.h"
#include "runtime/ctr_memory.h"
#include <array>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <vector>

using namespace lego::ctr;
namespace {
int failures{};
#define CHECK(x) do {if (!(x)) {std::cerr << "FAIL " << __LINE__ << ": " #x "\n"; ++failures;}} while (0)
constexpr std::uint32_t kVa=0x14000000, kPa=0x20000000;
constexpr auto kRw=MemoryPermission::Read|MemoryPermission::Write;
using Vector=std::array<std::uint32_t,4>;
constexpr Vector kDefault{0x3F800000,0x40000000,0x40400000,0x40800000};

struct ReadProbe final : DeviceMemory {
    mutable unsigned probes{},reads{};
    std::uint32_t size() const noexcept override {return 0x1000;}
    bool CanRead(std::uint32_t,std::uint32_t) const noexcept override {++probes; return true;}
    bool CanWrite(std::uint32_t,std::uint32_t) const noexcept override {return false;}
    bool Read(std::uint32_t,std::span<std::uint8_t> bytes) const noexcept override {
        ++reads; for (auto& byte:bytes) byte=0; return true;
    }
    bool Write(std::uint32_t,std::span<const std::uint8_t>) noexcept override {return false;}
    std::uint64_t Epoch(std::uint32_t) const noexcept override {return 0;}
};

struct Fixture {
    GuestMemory memory;
    PicaGpuRegisters regs{};
    PicaUploadState uploads{};
    PicaVertexFetchPlan plan;
    std::shared_ptr<GpuVramBank> vram;
    const char* error{};
    Fixture() {
        CHECK(memory.Map(kVa,0x1000,kRw));
        CHECK(memory.LoadBytes(kVa,std::vector<std::uint8_t>(0x1000,0xA5)));
    }
    // Exercise the production byte-mask provenance path, not a test-only claim
    // that an allocated register shadow is known.
    void Set(unsigned id,std::uint32_t value,unsigned mask=15) {
        std::array<std::uint8_t,8> list{};
        const auto header=id|(mask<<16);
        for (unsigned b=0;b<4;++b) {
            list[b]=static_cast<std::uint8_t>(value>>(8*b));
            list[4+b]=static_cast<std::uint8_t>(header>>(8*b));
        }
        PicaListPlan staged;
        CHECK(StagePicaStartupList(list,regs,uploads,staged));
        regs=staged.registers; uploads=staged.uploads;
    }
    void Configure(bool known_base=true) {
        if (known_base) Set(0x200,kPa>>3);
        // Attribute formats: signed byte3, unsigned byte2, short2, float2,
        // followed by one fixed attribute. Loader order includes padding ID12.
        Set(0x201,0x000F7658); Set(0x202,0x40100000);
        for (unsigned id=0x203;id<=0x226;++id) Set(id,0);
        Set(0x203,0x80); Set(0x204,0x000321C0); Set(0x205,0x50180000);
        Set(0x228,2); Set(0x229,0); Set(0x22A,1);
        Set(0x2B9,0xA0000004); Set(0x2BB,0x00002314); Set(0x2BC,0);
        uploads.default_attributes.floats[4]=kDefault;
        uploads.default_attributes.written.set(4);
        // First submitted source vertex is index1. Poisoned padding and index0
        // distinguish component alignment and stride from a packed byte stream.
        Bytes(0x98,{0x80,0xFF,0x7F,0xA5,0xA5,0xA5,0xA5,0xA5,
                    0,0xFF,0,0x80,0xFF,0x7F,0xA5,0xA5,
                    0,0,0xC0,0x3F,0,0,0,0x80});
        Bytes(0xB0,{1,0xFE,0,0xA5,0xA5,0xA5,0xA5,0xA5,
                    0x80,1,0xFE,0xFF,0,0,0xA5,0xA5,
                    0,0,0x80,0xBF,0,0,0,0x40});
    }
    void Bytes(std::uint32_t offset,std::initializer_list<std::uint8_t> bytes) {
        CHECK(memory.LoadBytes(kVa+offset,std::span<const std::uint8_t>(bytes.begin(),bytes.size())));
    }
    bool Run(std::uint32_t draw=0x22E) {
        return StagePicaVertexFetch(draw,regs,uploads,memory,vram.get(),plan,error);
    }
    void Stop(std::uint32_t draw=0x22E) {
        const auto before=plan; const auto before_regs=regs; const auto before_uploads=uploads;
        CHECK(!Run(draw)); CHECK(error);
        CHECK(plan==before && regs==before_regs && uploads==before_uploads);
    }
};

void FormatsLayoutAndInputMapping() {
    Fixture f; f.Configure(); const auto before_regs=f.regs; const auto before_uploads=f.uploads;
    CHECK(f.Run() && !f.error);
    CHECK(f.plan.attribute_count==5 && f.plan.input_count==5 && f.plan.vertices.size()==2);
    CHECK(f.regs==before_regs && f.uploads==before_uploads);
    if (f.plan.vertices.size()!=2) return;
    const std::array<std::array<Vector,5>,2> expected{{
        {{{0xC3000000,0xBF800000,0x42FE0000,0x3F800000},
          {0,0x437F0000,0,0x3F800000}, {0xC7000000,0x46FFFE00,0,0x3F800000},
          {0x3FC00000,0x80000000,0,0x3F800000}, kDefault}},
        {{{0x3F800000,0xC0000000,0,0x3F800000},
          {0x43000000,0x3F800000,0,0x3F800000}, {0xC0000000,0,0,0x3F800000},
          {0xBF800000,0x40000000,0,0x3F800000}, kDefault}}
    }};
    constexpr std::array<unsigned,5> input_register{4,1,3,2,0};
    for (unsigned v=0;v<f.plan.vertices.size();++v) {
        const auto& vertex=f.plan.vertices[v];
        CHECK(vertex.source_index==v+1);
        CHECK(vertex.attributes_written.count()==5 && vertex.inputs_written.count()==5);
        for (unsigned a=0;a<5;++a) {
            CHECK(vertex.attributes_written[a] && vertex.attributes[a]==expected[v][a]);
            CHECK(vertex.inputs_written[input_register[a]] && vertex.inputs[input_register[a]]==expected[v][a]);
        }
        CHECK(!vertex.attributes_written[15] && !vertex.inputs_written[15]);
    }
    const std::vector<PicaVertexReadSpan> reads{
        {kPa+0x98,3,1,0},{kPa+0xA0,2,1,1},{kPa+0xA2,4,1,2},{kPa+0xA8,8,1,3},
        {kPa+0xB0,3,2,0},{kPa+0xB8,2,2,1},{kPa+0xBA,4,2,2},{kPa+0xC0,8,2,3}};
    CHECK(f.plan.reads==reads);
    // Input fetch cannot authorize the still-unimplemented draw command.
    std::array<std::uint8_t,8> draw{1,0,0,0,0x2E,2,15,0}; PicaListPlan list_plan;
    CHECK(!StagePicaStartupList(draw,f.regs,f.uploads,list_plan));
    CHECK(list_plan.result.register_id==0x22E && list_plan.result.error);

    // A separate second loader has its own offset and stride. Re-homing just
    // the float attribute must produce the same shader inputs for both vertices.
    const auto vertices=f.plan.vertices;
    f.Set(0x205,0x40180000); f.Set(0x206,0x180);
    f.Set(0x207,3); f.Set(0x208,0x10080000);
    f.Bytes(0x188,{0,0,0xC0,0x3F,0,0,0,0x80,0,0,0x80,0xBF,0,0,0,0x40});
    CHECK(f.Run()); CHECK(f.plan.vertices==vertices);
    CHECK(f.plan.reads.size()==8);
    if (f.plan.reads.size()==8)
        CHECK(f.plan.reads[3].physical_address==kPa+0x188 && f.plan.reads[7].physical_address==kPa+0x190);
}

void SourceKindsAndKnownState() {
    Fixture f; f.Configure();
    f.Set(0x200,kGpuVramPhysicalBase>>3); f.Set(0x201,15); f.Set(0x202,0);
    f.Set(0x203,0x40); f.Set(0x204,0); f.Set(0x205,0x10100000);
    f.Set(0x228,1); f.Set(0x22A,0); f.Set(0x2B9,0xA0000000); f.Set(0x2BB,3);
    f.vram=GpuVramBank::ReferenceZero();
    const std::array<std::uint8_t,16> bytes{0,0,0x80,0x3F,0,0,0,0xC0,0,0,0x40,0x40,0,0,0x80,0x40};
    CHECK(f.vram->Write(0x40,bytes)); CHECK(f.Run());
    CHECK(f.plan.vertices.size()==1);
    if (f.plan.vertices.size()!=1) return;
    CHECK((f.plan.vertices[0].attributes[0]==Vector{0x3F800000,0xC0000000,0x40400000,0x40800000}));
    CHECK(f.plan.vertices[0].inputs[3]==f.plan.vertices[0].attributes[0]);
    CHECK(f.plan.reads.size()==1 && f.plan.reads[0].physical_address==kGpuVramPhysicalBase+0x40);
    f.vram.reset(); f.Stop(); // No implicit zero-filled bank may replace the supplied source.

    Fixture partial; partial.Configure(false);
    partial.Set(0x200,kPa>>3,8); // Numerically valid shadow; lower bytes remain unknown.
    partial.Stop(); partial.Set(0x200,kPa>>3,0); partial.Stop();
    partial.Set(0x200,kPa>>3,7); CHECK(partial.Run());
    partial.uploads.default_attributes.written.reset(4); partial.Stop();
}

void RejectionPreservesPreparedVertices() {
    Fixture f; f.Configure(); CHECK(f.Run());
    f.Stop(0x22F);
    f.Set(0x229,2); f.Stop(); f.Set(0x229,0);
    f.Set(0x2B9,0x08000004); f.Stop();
    f.Set(0x2B9,0xA0000104); f.Stop(); f.Set(0x2B9,0xA0000004);
    f.Set(0x228,kPicaVertexFetchMaxVertices+1); f.Stop(); f.Set(0x228,2);
    f.Set(0x22A,0xFFFFFFFF); f.Stop(); f.Set(0x22A,1);
    f.Set(0x200,0x1FFFFFFE); f.Stop(); f.Set(0x200,kPa>>3);
    f.Set(0x205,0x40180000); f.Stop(); f.Set(0x205,0x50180000); // Missing array attr3.
    f.Set(0x206,0x300); f.Set(0x207,0); f.Set(0x208,0x10030000); f.Stop();
    f.Set(0x208,0); // Duplicate loader assignments have no supported overwrite policy.
    const auto written=f.uploads.default_attributes.written;
    f.uploads.default_attributes.written.set();
    f.Set(0x202,0xFFFF0000); f.Stop(); // All defaults0..14 known; attr15 still unsupported.
    f.uploads.default_attributes.written=written; f.Set(0x202,0x40100000);
    // Vertex1 is readable; the second vertex's final float array crosses the
    // mapped page. No prefix of the failed fetch replaces the previous plan.
    f.Set(0x203,0xFC0); f.Stop();
    auto device=std::make_shared<ReadProbe>();
    CHECK(f.memory.MapDeviceMemory(kVa+0x1000,device,MemoryPermission::Read));
    device->probes=0; device->reads=0;
    f.Set(0x203,0x1000); f.Stop();
    CHECK(device->probes==0 && device->reads==0); // Private FCRAM excludes CPU device callbacks.
}
} // namespace

int main() {
    FormatsLayoutAndInputMapping(); SourceKindsAndKnownState(); RejectionPreservesPreparedVertices();
    if (failures) return EXIT_FAILURE;
    std::cout << "PASS: native vertex fetch formats, layout, input mapping, known state and atomic rejection; drawing remains guarded\n";
    return EXIT_SUCCESS;
}
