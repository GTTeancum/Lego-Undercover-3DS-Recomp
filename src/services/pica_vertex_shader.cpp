#include "services/pica_vertex_shader.h"
#include <bit>
#include <cfenv>
#include <limits>
#include <utility>

namespace lego::ctr {
namespace {
// Independent implementation of the bounded semantics documented by:
// nihstro f4d8659decbfe5d234f04134b5002b82dc515a44, shader_bytecode.h;
// Azahar 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac, shader_interpreter.cpp,
// pica_types.h and shader_unit.cpp. No reference interpreter is embedded.
static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559);
constexpr std::uint32_t kVsProgramWords=512; // Existing bounded VS upload capacity.
using Vector=std::array<std::uint32_t,4>;

bool KnownBytes(const PicaUploadState& uploads,std::uint32_t id,std::uint8_t mask) {
    return (uploads.register_known_bytes[id]&mask)==mask;
}
bool NormalOrZero(std::uint32_t bits) {
    const auto exponent=bits&0x7F800000U;
    return exponent!=0x7F800000U && (exponent || !(bits&0x007FFFFFU));
}
bool IsZero(std::uint32_t bits) {return (bits&0x7FFFFFFFU)==0;}

struct CallScope {std::uint32_t end{},resume{};};

struct VertexExecutor {
    const PicaUploadState& uploads;
    const PicaFetchedVertex& input;
    PicaShadedVertex& output;
    std::uint32_t& total_instructions;
    const char*& error;
    std::array<Vector,16> temporary{};
    std::array<std::uint8_t,16> input_components{},temporary_components{};

    bool Fail(const char* message) {error=message;return false;}

    bool Source(std::uint32_t reg,std::uint32_t component,std::uint32_t& value) {
        const auto bit=1U<<component;
        if (reg<16) {
            if (!(input_components[reg]&bit))
                return Fail("PICA vertex shader reads an unknown input component");
            value=input.inputs[reg][component];
        } else if (reg<32) {
            if (!(temporary_components[reg-16]&bit))
                return Fail("PICA vertex shader reads an unwritten temporary component");
            value=temporary[reg-16][component];
        } else {
            const auto index=reg-32;
            if (index>=uploads.vs.floats.size() || !uploads.vs.floats_written[index])
                return Fail("PICA vertex shader float uniform has not been uploaded");
            value=uploads.vs.floats[index][component];
        }
        return true;
    }

    bool Operand(std::uint32_t reg,std::uint32_t descriptor,unsigned operand,
                 unsigned lane,std::uint32_t& value) {
        const auto selected=(descriptor>>(11U+9U*operand-2U*lane))&3U;
        if (!Source(reg,selected,value)) return false;
        if (descriptor&(1U<<(4U+9U*operand))) value^=0x80000000U;
        return true;
    }

    bool Multiply(std::uint32_t left,std::uint32_t right,std::uint32_t& result) {
        if (!NormalOrZero(left) || !NormalOrZero(right))
            return Fail("PICA vertex shader arithmetic requires finite normal or zero operands");
        // The volatile float store is a binary32 rounding boundary. In
        // particular, a following Add cannot contract this product into FMA.
        volatile float rounded=std::bit_cast<float>(left)*std::bit_cast<float>(right);
        result=std::bit_cast<std::uint32_t>(static_cast<float>(rounded));
        if (!NormalOrZero(result))
            return Fail("PICA vertex shader arithmetic produced a nonfinite or subnormal result");
        if (IsZero(result) && !IsZero(left) && !IsZero(right))
            return Fail("PICA vertex shader arithmetic underflow is unsupported");
        return true;
    }

    bool Add(std::uint32_t left,std::uint32_t right,std::uint32_t& result) {
        if (!NormalOrZero(left) || !NormalOrZero(right))
            return Fail("PICA vertex shader arithmetic requires finite normal or zero operands");
        volatile float rounded=std::bit_cast<float>(left)+std::bit_cast<float>(right);
        result=std::bit_cast<std::uint32_t>(static_cast<float>(rounded));
        if (!NormalOrZero(result))
            return Fail("PICA vertex shader arithmetic produced a nonfinite or subnormal result");
        // A nonzero cancellation flushed by the host must not masquerade as a
        // supported zero. Exact opposite values (and two zeros) are permitted.
        if (IsZero(result) && !(IsZero(left)&&IsZero(right)) &&
            (left^right)!=0x80000000U)
            return Fail("PICA vertex shader arithmetic underflow is unsupported");
        return true;
    }

    bool Arithmetic(std::uint32_t instruction,std::uint32_t opcode) {
        const bool madi=(opcode&0x38U)==0x30U;
        if (!madi && opcode!=0x13 && opcode!=0x08 && opcode!=0x18)
            return Fail("PICA vertex shader opcode is outside the supported native slice");
        const auto relative=madi?(instruction>>22)&3U:(instruction>>19)&3U;
        if (relative)
            return Fail("PICA vertex shader relative addressing is unsupported");
        const auto descriptor_id=instruction&(madi?31U:127U);
        if (!uploads.vs.swizzle_written[descriptor_id])
            return Fail("PICA vertex shader operand descriptor has not been uploaded");
        const auto descriptor=uploads.vs.swizzle[descriptor_id];
        const auto dest=madi?(instruction>>24)&31U:(instruction>>21)&31U;
        const auto src1=madi?(instruction>>17)&31U:
                              (opcode==0x18?(instruction>>14)&31U:(instruction>>12)&127U);
        const auto src2=madi?(instruction>>12)&31U:
                              (opcode==0x18?(instruction>>7)&127U:(instruction>>7)&31U);
        const auto src3=(instruction>>5)&127U;
        Vector values{};
        std::uint8_t written{};
        std::uint32_t dot{};
        if (opcode==0x18 && (descriptor&15U)) {
            // DPHI first swizzles/negates xyz, then replaces src1.w with +1.
            // The dot starts at +0 and rounds each multiply and addition.
            for (unsigned lane=0;lane<4;++lane) {
                std::uint32_t left=0x3F800000U,right{},product{};
                if (lane<3 && !Operand(src1,descriptor,0,lane,left)) return false;
                if (!Operand(src2,descriptor,1,lane,right) ||
                    !Multiply(left,right,product) || !Add(dot,product,dot)) return false;
            }
        }
        for (unsigned lane=0;lane<4;++lane) {
            if (!(descriptor&(8U>>lane))) continue;
            if (opcode==0x18) values[lane]=dot;
            else {
                std::uint32_t left{},right{},third{},product{};
                if (!Operand(src1,descriptor,0,lane,left)) return false;
                if (opcode==0x13) values[lane]=left;
                else {
                    if (!Operand(src2,descriptor,1,lane,right) ||
                        !Multiply(left,right,product)) return false;
                    if (madi) {
                        if (!Operand(src3,descriptor,2,lane,third) ||
                            !Add(product,third,values[lane])) return false;
                    } else values[lane]=product;
                }
            }
            written|=static_cast<std::uint8_t>(1U<<lane);
        }
        // Read every selected operand before replacing any destination lane:
        // temporary-register source/destination overlap must preserve swizzles.
        auto& destination=dest<16?output.outputs[dest]:temporary[dest-16];
        auto& known=dest<16?output.output_components[dest]:temporary_components[dest-16];
        for (unsigned lane=0;lane<4;++lane)
            if (written&(1U<<lane)) destination[lane]=values[lane];
        known|=written;
        return true;
    }

    bool Run(std::uint32_t entry) {
        for (unsigned reg=0;reg<16;++reg)
            if (input.inputs_written[reg]) input_components[reg]=15;
        std::array<CallScope,kPicaVertexShaderMaxCallDepth> calls{};
        std::size_t depth{};
        auto pc=entry;
        for (;;) {
            if (output.instruction_count>=kPicaVertexShaderMaxInstructions ||
                total_instructions>=kPicaVertexShaderMaxTotalInstructions)
                return Fail("PICA vertex shader exceeds the host instruction bound");
            if (pc>=kVsProgramWords)
                return Fail("PICA vertex shader instruction address is outside the bounded VS bank");
            if (!uploads.vs.program_written[pc])
                return Fail("PICA vertex shader instruction has not been uploaded");
            const auto old_pc=pc,instruction=uploads.vs.program[pc],opcode=instruction>>26;
            ++pc;++output.instruction_count;++total_instructions;
            if (opcode==0x22) return true; // Only an explicit, uploaded END terminates.
            if (opcode==0x26) {
                const auto boolean=(instruction>>22)&15U;
                const auto known_byte=static_cast<std::uint8_t>(1U<<(boolean/8U));
                if (!uploads.vs.booleans_written || !KnownBytes(uploads,0x2B0,known_byte))
                    return Fail("PICA vertex shader boolean uniform is unknown");
                if (uploads.vs.booleans&(1U<<boolean)) {
                    const auto target=(instruction>>10)&0xFFFU,length=instruction&255U;
                    if (!length || target>=kVsProgramWords || target+length>kVsProgramWords)
                        return Fail("PICA vertex shader CALLU target/length is unsupported");
                    if (depth>=calls.size())
                        return Fail("PICA vertex shader exceeds the supported call depth");
                    // These cases interact with the reference's old-PC scope
                    // cleanup in surprising ways. Contain them explicitly; the
                    // supported depth also excludes its special four-pop bug.
                    if (target+length==old_pc+1)
                        return Fail("PICA vertex shader CALLU overlaps a return boundary");
                    for (std::size_t i=0;i<depth;++i)
                        if (calls[i].end==old_pc+1)
                            return Fail("PICA vertex shader CALLU at a return boundary is unsupported");
                    calls[depth++]={target+length,old_pc+1};pc=target;
                }
            } else if (!Arithmetic(instruction,opcode)) return false;

            // Return tests use the executed instruction's old PC, then resume
            // addresses when unwinding surrounding scopes, as in the pin.
            auto boundary=old_pc+1;
            while (depth && calls[depth-1].end==boundary) {
                pc=calls[--depth].resume;boundary=pc;
            }
        }
    }
};
} // namespace

bool StagePicaVertexShader(const PicaGpuRegisters& registers,const PicaUploadState& uploads,
                           const PicaVertexFetchPlan& fetched,
                           PicaVertexShaderPlan& plan,const char*& error) {
    error=nullptr;
    const auto fail=[&](const char* message) {error=message;return false;};
    const auto reg=[&](std::uint32_t id) {return registers[0x400U+id];};
    if (std::fegetround()!=FE_TONEAREST)
        return fail("PICA vertex shader requires nearest host floating-point rounding");
    if (!KnownBytes(uploads,0x229,1) || !KnownBytes(uploads,0x2B9,11) ||
        !KnownBytes(uploads,0x2BA,3) || !KnownBytes(uploads,0x2BD,3))
        return fail("PICA vertex shader configuration contains unknown register bytes");
    if ((reg(0x229)&3U) || (reg(0x2B9)&0xFF00FF00U)!=0xA0000000U)
        return fail("PICA vertex shader requires VS mode without geometry/input-to-uniform routing");
    if (fetched.input_count!=(reg(0x2B9)&15U)+1U)
        return fail("PICA vertex shader input count does not match the fetched snapshot");
    if (fetched.vertices.size()>kPicaVertexFetchMaxVertices)
        return fail("PICA vertex shader vertex count exceeds the host staging bound");
    const auto entry=reg(0x2BA)&0xFFFFU,output_mask=reg(0x2BD)&0xFFFFU;
    if (entry>=kVsProgramWords)
        return fail("PICA vertex shader entry point is outside the bounded VS bank");

    PicaVertexShaderPlan staged;
    staged.output_count=static_cast<std::uint32_t>(std::popcount(output_mask));
    staged.vertices.resize(fetched.vertices.size());
    for (std::size_t vertex=0;vertex<fetched.vertices.size();++vertex) {
        auto& result=staged.vertices[vertex];result.source_index=fetched.vertices[vertex].source_index;
        VertexExecutor execute{uploads,fetched.vertices[vertex],result,staged.instruction_count,error};
        if (!execute.Run(entry)) return false;
        unsigned attribute{};
        for (unsigned output=0;output<16;++output) {
            if (!(output_mask&(1U<<output))) continue;
            result.attributes[attribute]=result.outputs[output];
            result.attribute_components[attribute]=result.output_components[output];
            ++attribute;
        }
    }
    plan=std::move(staged);
    return true;
}
} // namespace lego::ctr
