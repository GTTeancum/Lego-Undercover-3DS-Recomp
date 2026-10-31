#include "host/supplemental_registry.h"
#include "runtime/ctr_recorded_step.h"
#include "runtime/ctr_memory.h"
#include <array>
#include <bit>
#include <limits>
#include <cstdlib>
#include <iostream>

namespace {
using namespace lego::ctr;
int failures{};
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL " << __LINE__ << ": " #x "\n"; ++failures; } } while (0)
constexpr auto N=a32::kFlagN, Z=a32::kFlagZ, C=a32::kFlagC, V=a32::kFlagV;
constexpr std::uint32_t pc=0x00820000U, other_flags=0x08050010U;
// Authored A32 instruction fields, never copied from an original game image.
constexpr std::uint32_t Word(unsigned rotate, unsigned byte, bool flags,
                             a32::Condition condition, unsigned rn, unsigned rd) {
    return (static_cast<std::uint32_t>(condition)<<28U) | (1U<<25U) | (3U<<21U) |
           (flags ? 1U<<20U : 0U) | (rn<<16U) | (rd<<12U) | (rotate<<8U) | byte;
}
void Case(unsigned rotate, unsigned byte, bool set_flags, std::uint32_t input,
          std::uint32_t before, std::uint32_t value, std::uint32_t after,
          a32::Condition condition=a32::Condition::Al, bool executes=true,
          unsigned rn=5, unsigned rd=7) {
    const a32::PackedOp op{Word(rotate,byte,set_flags,condition,rn,rd),
                          a32::EncodeMetadata(a32::Opcode::CoreAlu,condition)};
    const a32::BlockShard shard{pc,pc+0xffcU,nullptr,0};
    const a32::Registry base{&shard,1,nullptr,0};
    const a32::Block block{pc,&op,1,false};
    lego::host::SupplementalRegistry merged(base,std::span(&block,1));
    GuestMemory memory;
    a32::GuestState state{};
    for (unsigned i=0;i<state.r.size();++i) state.r[i]=0x13570000U+i;
    state.r[rn]=input; state.r[15]=pc; state.cpsr=other_flags|before;
    state.fpscr=0x12345678U; state.vfp.fill(0x5a3c0123U);
    state.thread_pointer=0x08004000U; state.exclusive_address=0x08005000U;
    state.exclusive_token=0x24681357abcdef01ULL; state.exclusive_size=4;
    state.exclusive_valid=true;
    auto expected=state;
    if (executes) expected.r[rd]=value;
    expected.r[15]=pc+4U; expected.cpsr=other_flags|after;
    const auto result=StepRecordedA32(merged.registry(),state,memory);
    CHECK(result.kind==a32::ExitKind::Fallthrough && result.pc==pc+4U);
    CHECK(result.fallback==a32::FallbackReason::None && result.detail==0U);
    CHECK(state.r==expected.r && state.cpsr==expected.cpsr);
    CHECK(state.fpscr==expected.fpscr && state.vfp==expected.vfp);
    CHECK(state.thread_pointer==expected.thread_pointer);
    CHECK(state.exclusive_address==expected.exclusive_address &&
          state.exclusive_token==expected.exclusive_token &&
          state.exclusive_size==expected.exclusive_size &&
          state.exclusive_valid==expected.exclusive_valid);
    CHECK(a32::FindBlock(base,pc)==nullptr);
    const auto stopped=state;
    CHECK(StepRecordedA32(merged.registry(),state,memory).kind==a32::ExitKind::MissingBlock);
    CHECK(state.r==stopped.r && state.cpsr==stopped.cpsr);
}
// Arm DDI 0406C.d, A8.8.153: A1 RSB immediate is available on ARMv6.
// https://documentation-service.arm.com/static/5f8daeb7f86e16515cdb8c4e
// Authored expectations below never use the runtime ALU or expansion helpers.
void FixedCases() {
    // RSB #0 negates its source. Signed minimum overflows but preserves its bits.
    Case(0,0,true,0U,N|V,0U,Z|C);
    Case(0,0,true,1U,Z|C|V,0xffffffffU,N);
    Case(0,0,true,0xffffffffU,N|Z|C|V,1U,0U);
    Case(0,0,true,0x7fffffffU,C|V,0x80000001U,N);
    Case(0,0,true,0x80000000U,C,0x80000000U,N|V);
    Case(0,0,true,0x80000001U,N|Z|C|V,0x7fffffffU,0U);
    // Nonzero immediates distinguish reverse subtraction from ordinary SUB.
    Case(0,0x80,true,0x7fU,N|V,1U,C);
    Case(0,0x80,true,0x80U,N|V,0U,Z|C);
    Case(0,0x80,true,0x81U,Z|C|V,0xffffffffU,N);
    Case(0,1,true,0xffffffffU,N|Z|C|V,2U,0U);
    Case(1,2,true,1U,N,0x7fffffffU,C|V);
    Case(1,2,true,0U,Z|V,0x80000000U,N|C);
    Case(1,2,true,0xffffffffU,C|V,0x80000001U,N);
    Case(1,1,true,0xc0000000U,C,0x80000000U,N|V);
    Case(4,0xff,true,0x01000000U,Z|V,0xfe000000U,N|C);
    Case(4,0xff,true,0x7fffffffU,N|Z,0x7f000001U,C|V);
    // A rotated encoding of zero must not import or preserve shifter carry.
    for (unsigned rotate=0;rotate<16;++rotate) {
        Case(rotate,0,true,0U,N|V,0U,Z|C);
        Case(rotate,0,true,1U,Z|C|V,0xffffffffU,N);
        Case(rotate,0,false,1U,N|Z|C|V,0xffffffffU,N|Z|C|V);
    }
    for (unsigned reg=0;reg<15;++reg)
        Case(0,0,true,0x80000000U,C,0x80000000U,N|V,
             a32::Condition::Al,true,reg,reg);
    Case(0,0x80,false,0x81U,N|Z|C|V,0xffffffffU,N|Z|C|V,
         a32::Condition::Al,true,13,14);
    Case(0,0x80,true,0x80U,V,0U,Z|C,a32::Condition::Al,true,14,13);
}

std::int64_t Signed(std::uint32_t value) {
    return value&0x80000000U ? std::int64_t(value)-0x100000000LL : value;
}
void AllImmediateEncodings() {
    // Independent 64-bit signed arithmetic detects overflow, while an unsigned
    // comparison detects no-borrow. No runtime AddWithCarry is used as oracle.
    constexpr std::array<std::uint32_t,8> inputs{
        0U,1U,0x7fffffffU,0x80000000U,0x80000001U,0xffffffffU,0x13579bdfU,0xfedcba98U};
    for (unsigned rotate=0;rotate<16;++rotate) {
        for (unsigned byte=0;byte<256;++byte) {
            const auto operand=std::rotr(std::uint32_t(byte),int(rotate*2U));
            for (auto input:inputs) {
                const std::uint32_t value=operand-input;
                const auto wide=Signed(operand)-Signed(input);
                const auto expected=(value&0x80000000U?N:0U) | (!value?Z:0U) |
                    (operand>=input?C:0U) |
                    (wide<std::numeric_limits<std::int32_t>::min() ||
                     wide>std::numeric_limits<std::int32_t>::max()?V:0U);
                // Opposite incoming carry states produce the same arithmetic.
                Case(rotate,byte,true,input,N|Z|C|V,value,expected);
                Case(rotate,byte,true,input,0U,value,expected);
                Case(rotate,byte,false,input,N|Z|C|V,value,N|Z|C|V);
            }
        }
    }
}

void Conditions() {
    for (unsigned bits=0;bits<16;++bits) {
        const bool n=bits&8U,z=bits&4U,c=bits&2U,v=bits&1U;
        const auto before=(n?N:0U)|(z?Z:0U)|(c?C:0U)|(v?V:0U);
        const std::array<bool,15> pass{z,!z,c,!c,n,!n,v,!v,c&&!z,!c||z,
                                       n==v,n!=v,!z&&n==v,z||n!=v,true};
        for (unsigned cond=0;cond<pass.size();++cond) {
            Case(0,0,true,5U,before,0xfffffffbU,pass[cond]?N:before,
                 static_cast<a32::Condition>(cond),pass[cond]);
            Case(0,0,false,5U,before,0xfffffffbU,before,
                 static_cast<a32::Condition>(cond),pass[cond]);
        }
    }
}

void FlagDrivenDispatch() {
    // An independently authored RSB/BMI decision tests both dispatchers and
    // an interior conditional-branch suffix; all targets are synthetic.
    constexpr auto negative=pc+0x40U,join=pc+0x80U;
    const a32::PackedOp decide[]{
        {Word(0,0,true,a32::Condition::Al,7,6),a32::EncodeMetadata(a32::Opcode::CoreAlu,a32::Condition::Al)},
        {(4U<<28U)|(5U<<25U)|13U,a32::EncodeMetadata(a32::Opcode::Branch,a32::Condition::Mi)}};
    const a32::PackedOp nonnegative[]{
        {(14U<<28U)|(1U<<25U)|(13U<<21U)|(10U<<12U)|0x21U,
         a32::EncodeMetadata(a32::Opcode::MovImm,a32::Condition::Al,a32::Immediate)},
        {(14U<<28U)|(5U<<25U)|27U,a32::EncodeMetadata(a32::Opcode::Branch,a32::Condition::Al)}};
    const a32::PackedOp neg[]{
        {(14U<<28U)|(1U<<25U)|(13U<<21U)|(10U<<12U)|0x42U,
         a32::EncodeMetadata(a32::Opcode::MovImm,a32::Condition::Al,a32::Immediate)},
        {(14U<<28U)|(5U<<25U)|13U,a32::EncodeMetadata(a32::Opcode::Branch,a32::Condition::Al)}};
    const a32::BlockShard shard{pc,pc+0xffcU,nullptr,0};
    const a32::Registry base{&shard,1,nullptr,0};
    const a32::Block additions[]{{pc,decide,2,false},{pc+8U,nonnegative,2,false},{negative,neg,2,false}};
    lego::host::SupplementalRegistry merged(base,additions);
    GuestMemory memory;
    for (auto input:{3U,0U,0xfffffffdU}) {
        a32::GuestState initial{};initial.r[7]=input;initial.r[6]=0xabcdef12U;
        initial.r[15]=pc;initial.r[14]=0x00550000U;initial.cpsr=other_flags|V;
        auto expected=initial;expected.r[6]=0U-input;expected.r[10]=input==3U?0x42U:0x21U;
        expected.r[15]=join;expected.cpsr=other_flags|(input==3U?N:input==0U?Z|C:0U);
        auto stepped=initial;
        for (unsigned i=0;i<4;++i) {
            const auto result=StepRecordedA32(merged.registry(),stepped,memory);
            CHECK(result.kind==a32::ExitKind::Branch || result.kind==a32::ExitKind::Fallthrough);
        }
        CHECK(stepped.r==expected.r && stepped.cpsr==expected.cpsr);
        const auto missing=StepRecordedA32(merged.registry(),stepped,memory);
        CHECK(missing.kind==a32::ExitKind::MissingBlock && missing.pc==join);
        CHECK(stepped.r==expected.r && stepped.cpsr==expected.cpsr);
        auto dispatched=initial;
        const auto result=a32::Dispatch(merged.registry(),pc,dispatched,memory,nullptr,nullptr,8U);
        CHECK(result.kind==a32::ExitKind::MissingBlock && result.pc==join);
        CHECK(dispatched.r==expected.r && dispatched.cpsr==expected.cpsr);
    }
    CHECK(a32::FindBlock(base,pc)==nullptr);
}
}
int main() {
    FixedCases();AllImmediateEncodings();Conditions();FlagDrivenDispatch();
    if (failures) return EXIT_FAILURE;
    std::cout << "PASS: authored immediate RSB/RSBS negation, all 4096 immediate encodings, arithmetic NZCV, conditions, overlap, unchanged auxiliary state and flag-driven strict dispatch\n";
}
