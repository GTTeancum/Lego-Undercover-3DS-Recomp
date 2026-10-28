#include "services/pica_vertex_shader.h"
#include <array>
#include <cfenv>
#include <cstdlib>
#include <initializer_list>
#include <iostream>

using namespace lego::ctr;
namespace {
int failures{};
#define CHECK(x) do {if (!(x)) {std::cerr << "FAIL " << __LINE__ << ": " #x "\n"; ++failures;}} while (0)
using Vector=std::array<std::uint32_t,4>;
constexpr std::uint32_t kEnd=0x88000000;
constexpr Vector kOne{0x3F800000,0x3F800000,0x3F800000,0x3F800000};
constexpr Vector kTwo{0x40000000,0x40000000,0x40000000,0x40000000};

// Authored fixtures use the pinned public instruction format. None of these
// programs, vertices or uniforms are copied from a game capture.
std::uint32_t Common(unsigned opcode,unsigned dest,unsigned src1,unsigned src2=0,
                     unsigned descriptor=0,unsigned relative=0) {
    const bool inverted=opcode==0x18;
    return (opcode<<26)|(dest<<21)|(relative<<19)|(src1<<(inverted?14:12))|
           (src2<<7)|descriptor;
}
std::uint32_t Madi(unsigned dest,unsigned src1,unsigned src2,unsigned src3,
                   unsigned descriptor=0) {
    return 0xC0000000U|(dest<<24)|(src1<<17)|(src2<<12)|(src3<<5)|descriptor;
}
std::uint32_t Call(unsigned boolean,unsigned target,unsigned length) {
    return 0x98000000U|(boolean<<22)|(target<<10)|length;
}
std::uint32_t Descriptor(unsigned mask=15,
        std::array<unsigned,4> a={0,1,2,3},std::array<unsigned,4> b={0,1,2,3},
        std::array<unsigned,4> c={0,1,2,3},unsigned negations=0) {
    std::uint32_t word=mask|((negations&1)?1U<<4:0)|((negations&2)?1U<<13:0)|((negations&4)?1U<<22:0);
    for (unsigned i=0;i<4;++i) word|=(a[i]<<(11-2*i))|(b[i]<<(20-2*i))|(c[i]<<(29-2*i));
    return word;
}

struct Fixture {
    PicaGpuRegisters regs{};
    PicaUploadState uploads{};
    PicaVertexFetchPlan fetched;
    PicaVertexShaderPlan plan;
    const char* error{};
    explicit Fixture(bool known_booleans=true) {
        Set(0x229,0); Set(0x2B9,0xA0000003); Set(0x2BA,0); Set(0x2BD,1);
        if (known_booleans) Set(0x2B0,0);
        fetched.input_count=4; fetched.attribute_count=4; fetched.vertices.resize(1);
        fetched.vertices[0].source_index=7;
        Input(0,kOne); Input(1,kTwo); Swizzle(0,Descriptor());
    }
    void Set(unsigned id,std::uint32_t value,unsigned mask=15) {
        std::array<std::uint8_t,8> bytes{}; const auto header=id|(mask<<16);
        for (unsigned b=0;b<4;++b) {
            bytes[b]=static_cast<std::uint8_t>(value>>(8*b));
            bytes[4+b]=static_cast<std::uint8_t>(header>>(8*b));
        }
        PicaListPlan staged; CHECK(StagePicaStartupList(bytes,regs,uploads,staged));
        regs=staged.registers; uploads=staged.uploads;
    }
    void Instruction(unsigned pc,std::uint32_t word) {
        uploads.vs.program[pc]=word; uploads.vs.program_written.set(pc);
    }
    void Program(std::initializer_list<std::uint32_t> words) {
        uploads.vs.program_written.reset(); unsigned pc{};
        for (auto word:words) Instruction(pc++,word);
    }
    void Swizzle(unsigned index,std::uint32_t word) {
        uploads.vs.swizzle[index]=word; uploads.vs.swizzle_written.set(index);
    }
    void Input(unsigned index,const Vector& value) {
        fetched.vertices[0].inputs[index]=value; fetched.vertices[0].inputs_written.set(index);
    }
    void Uniform(unsigned index,const Vector& value) {
        uploads.vs.floats[index]=value; uploads.vs.floats_written.set(index);
    }
    bool Run() {return StagePicaVertexShader(regs,uploads,fetched,plan,error);}
    void Stop() {
        const auto before=plan; const auto old_regs=regs; const auto old_uploads=uploads;
        const auto old_fetch=fetched;
        CHECK(!Run()); CHECK(error);
        CHECK(plan==before && regs==old_regs && uploads==old_uploads && fetched==old_fetch);
    }
};

void LiteralTransportMasksAndCompaction() {
    Fixture f; f.Set(0x2BD,0x12);
    f.Input(0,{0x3F800000,0xC0000000,0x40400000,0xC0800000}); // 1,-2,3,-4.
    f.Input(1,{0x41200000,0x41A00000,0x41F00000,0x42200000}); // 10,20,30,40.
    f.Swizzle(0,0x00001C9A); // r0.xz = -v0.wzyx; other temporary lanes remain unknown.
    f.Swizzle(1,0x0000000F); // o4.xyzw = r0.xxxx, reading only a known lane.
    f.Swizzle(2,0x00000365); // o1.yw = v1.xyzw; x/z remain unknown.
    f.Program({0x4E000000,0x4C810001,0x4C201002,kEnd});
    const auto before=f.fetched;
    CHECK(f.Run() && !f.error); CHECK(f.fetched==before);
    CHECK(f.plan.vertices.size()==1 && f.plan.output_count==2 && f.plan.instruction_count==4);
    if (f.plan.vertices.size()!=1) return;
    const auto& v=f.plan.vertices[0];
    CHECK(v.source_index==7 && v.instruction_count==4);
    CHECK(v.output_components[1]==0xA && v.output_components[4]==0xF);
    CHECK(v.outputs[1][1]==0x41A00000 && v.outputs[1][3]==0x42200000);
    CHECK((v.outputs[4]==Vector{0x40800000,0x40800000,0x40800000,0x40800000}));
    CHECK(v.attribute_components[0]==0xA && v.attribute_components[1]==0xF);
    CHECK(v.attributes[0]==v.outputs[1] && v.attributes[1]==v.outputs[4]);
    CHECK(v.output_components[0]==0 && v.attribute_components[2]==0);

    // MOV has no arithmetic conversion, even for payloads outside the supported
    // arithmetic slice; negation flips exactly the sign bit.
    f.Set(0x2BD,1); f.Swizzle(0,Descriptor(15,{0,1,2,3},{0,1,2,3},{0,1,2,3},1));
    f.Input(0,{0x7FC12345,1,0x80000000,0x7F800000});
    f.Program({Common(0x13,0,0),kEnd}); CHECK(f.Run());
    CHECK((f.plan.vertices[0].outputs[0]==Vector{0xFFC12345,0x80000001,0,0xFF800000}));
}

void ArithmeticOperandsAndRounding() {
    Fixture f; f.Set(0x2BD,0x21);
    f.Input(0,{0x40000000,0xC0400000,0x40800000,0xC0A00000}); // 2,-3,4,-5.
    f.Input(1,{0x3F800000,0x40000000,0x40400000,0x40800000}); // 1,2,3,4.
    f.Uniform(0,{0x3F000000,0x40000000,0xBF800000,0xC0000000});
    f.Uniform(1,{0x3F800000,0x40000000,0x40400000,0x40800000});
    f.Uniform(2,{0x41200000,0x41A00000,0x41F00000,0x42200000});
    f.Swizzle(0,Descriptor(15,{0,1,2,3},{0,1,2,3},{0,1,2,3},2));
    f.Swizzle(1,Descriptor(15,{3,2,1,0},{0,1,2,3},{0,1,2,3},1));
    f.Swizzle(2,Descriptor(15,{0,1,2,3},{3,2,1,0},{0,1,2,3},6));
    f.Program({Common(8,16,32,0),Common(0x18,0,16,33,1),Madi(5,1,16,34,2),kEnd});
    CHECK(f.Run());
    if (f.plan.vertices.empty()) return;
    CHECK((f.plan.vertices[0].outputs[0]==Vector{0xC1400000,0xC1400000,0xC1400000,0xC1400000})); // -12.
    CHECK((f.plan.vertices[0].outputs[5]==Vector{0,0xC1E00000,0xC2400000,0xC2100000})); // 0,-28,-48,-36.

    Fixture rounded;
    rounded.Input(0,{0x3F800001,0x3F800001,0x3F800001,0x3F800001});
    rounded.Input(1,{0x3F7FFFFE,0x3F7FFFFE,0x3F7FFFFE,0x3F7FFFFE});
    rounded.Uniform(0,{0xBF800000,0xBF800000,0xBF800000,0xBF800000});
    // (1+2^-23)*(1-2^-23) rounds to1 before adding-1. FMA would retain -2^-46.
    rounded.Program({Madi(0,0,1,32),kEnd}); CHECK(rounded.Run());
    if (rounded.plan.vertices.empty()) return;
    CHECK(rounded.plan.vertices[0].outputs[0]==Vector{});
    rounded.Program({Common(8,0,0,1),kEnd}); CHECK(rounded.Run());
    CHECK(rounded.plan.vertices[0].outputs[0]==kOne);
    rounded.Input(0,{0x3F800001,0x3F800000,0,0x41200000});
    rounded.Uniform(0,{0x3F7FFFFE,0xBF800000,0,0});
    rounded.Program({Common(0x18,0,0,32),kEnd}); CHECK(rounded.Run());
    CHECK(rounded.plan.vertices[0].outputs[0]==Vector{});
}

void ConditionalCallsAndReturns() {
    Fixture f; f.Set(0x2BD,7); f.Set(0x2B0,1U<<8); f.Uniform(0,kTwo);
    f.Program({Call(8,8,2),Common(0x13,1,1),kEnd});
    f.Instruction(8,Common(0x13,0,0)); f.Instruction(9,Common(0x13,2,32));
    CHECK(f.Run()); CHECK(f.plan.instruction_count==5);
    if (f.plan.vertices.empty()) return;
    CHECK(!f.uploads.vs.program_written[10]); // Return occurs before fetching the next callee word.
    CHECK(f.plan.vertices[0].outputs[0]==kOne && f.plan.vertices[0].outputs[2]==kTwo);
    f.Set(0x2B0,0); f.uploads.vs.floats_written.reset(0);
    CHECK(f.Run()); CHECK(f.plan.instruction_count==3);
    CHECK(f.plan.vertices[0].output_components[0]==0 && f.plan.vertices[0].output_components[1]==15);
    f.Instruction(0,Call(8,4095,255)); CHECK(f.Run()); // False branch does not validate/fetch its target.
    f.Set(0x2B0,1U<<8); f.Stop();
    f.Instruction(0,Call(8,8,0)); f.Stop();

    Fixture nested; nested.Set(0x2BD,3); nested.Set(0x2B0,1);
    nested.Program({Call(0,8,2),kEnd});
    nested.Instruction(8,Call(0,16,1)); nested.Instruction(9,Common(0x13,1,1));
    nested.Instruction(16,Common(0x13,0,0));
    CHECK(nested.Run() && nested.plan.instruction_count==5);
    if (nested.plan.vertices.empty()) return;
    CHECK(nested.plan.vertices[0].outputs[0]==kOne && nested.plan.vertices[0].outputs[1]==kTwo);
    nested.Instruction(0,Call(0,8,1)); nested.Stop(); // Taken call at its caller's return boundary.
    for (unsigned level=0;level<=kPicaVertexShaderMaxCallDepth;++level) {
        nested.Instruction(8*level,Call(0,8*(level+1),2));
        nested.Instruction(8*level+1,level?Common(0x13,0,0):kEnd);
    }
    nested.Stop();

    Fixture partial(false); partial.Set(0x2B0,0,1); partial.Program({Call(8,8,1),kEnd});
    partial.Stop(); // The write made byte0 known; it did not establish bool8's byte1.
}

void KnownStateAndArithmeticBounds() {
    Fixture f; f.Program({Common(0x13,0,0),kEnd}); CHECK(f.Run());
    f.uploads.vs.program_written.reset(0); f.Stop(); f.uploads.vs.program_written.set(0);
    f.uploads.vs.swizzle_written.reset(0); f.Stop(); f.uploads.vs.swizzle_written.set(0);
    f.fetched.vertices[0].inputs_written.reset(0); f.Stop(); f.fetched.vertices[0].inputs_written.set(0);
    f.Program({Common(0x13,0,32),kEnd}); f.Stop(); // Unknown uniform.
    f.Program({Common(0x13,0,16),kEnd}); f.Stop(); // Unknown temporary components.
    f.Program({Common(0,0,0,1),kEnd}); f.Stop(); // Unsupported ADD remains a stop.
    f.Program({Common(0x13,0,0,0,0,1),kEnd}); f.Stop();
    f.Program({Common(0x13,0,0),kEnd});
    auto second=f.fetched.vertices[0]; second.inputs_written.reset(0);
    f.fetched.vertices.push_back(second); f.Stop(); f.fetched.vertices.pop_back();
    f.Set(0x229,2); f.Stop(); f.Set(0x229,0);

    f.Program({Common(8,0,0,1),kEnd});
    for (const auto bits:{0x7F800000U,1U,0x7F7FFFFFU}) {
        f.Input(0,{bits,bits,bits,bits}); f.Input(1,kTwo); f.Stop();
    }
    f.Input(0,{0x00800000,0x00800000,0x00800000,0x00800000});
    f.Input(1,{0x3F000000,0x3F000000,0x3F000000,0x3F000000}); f.Stop(); // Subnormal result.
    f.Input(1,{0x00800000,0x00800000,0x00800000,0x00800000}); f.Stop(); // Underflow to zero.
    f.Input(0,kOne); f.Input(1,kTwo);
    const int rounding=std::fegetround();
    CHECK(std::fesetround(FE_UPWARD)==0); f.Stop(); CHECK(std::fesetround(rounding)==0);
    CHECK(f.Run()); CHECK(f.plan.vertices[0].outputs[0]==kTwo);
}

void BoundedWorkAndAtomicFailure() {
    Fixture f; f.Set(0x2B0,1); f.Program({Common(0x13,0,0),kEnd}); CHECK(f.Run());
    // Shared 80-instruction function, called13times: below code-memory capacity
    // and call-depth limits, but above the 1024-step invocation work bound.
    f.uploads.vs.program_written.reset();
    for (unsigned pc=0;pc<13;++pc) f.Instruction(pc,Call(0,32,80));
    f.Instruction(13,kEnd);
    for (unsigned pc=32;pc<112;++pc) f.Instruction(pc,Common(0x13,0,0));
    f.Stop();
    f.Instruction(12,kEnd); // 12*(CALL+80MOV)+END=973 steps per vertex.
    CHECK(f.Run()); CHECK(f.plan.instruction_count==973);
    const auto vertex=f.fetched.vertices[0];
    f.fetched.vertices.assign(kPicaVertexShaderMaxTotalInstructions/973+1,vertex);
    f.Stop(); // Each invocation fits; their aggregate exceeds the plan work bound.
}
} // namespace

int main() {
    LiteralTransportMasksAndCompaction(); ArithmeticOperandsAndRounding(); ConditionalCallsAndReturns();
    KnownStateAndArithmeticBounds(); BoundedWorkAndAtomicFailure();
    if (failures) return EXIT_FAILURE;
    std::cout << "PASS: bounded native VS transport, arithmetic, control flow, known components and atomic failure; no draw execution\n";
    return EXIT_SUCCESS;
}
