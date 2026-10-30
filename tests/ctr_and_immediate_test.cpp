#include "host/supplemental_registry.h"
#include "runtime/ctr_recorded_step.h"
#include "runtime/ctr_memory.h"
#include <array>
#include <cstdlib>
#include <iostream>

namespace {
using namespace lego::ctr;
int failures{};
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL " << __LINE__ << ": " #x "\n"; ++failures; } } while (0)
constexpr auto N=a32::kFlagN, Z=a32::kFlagZ, C=a32::kFlagC, V=a32::kFlagV;
constexpr std::uint32_t pc=0x00810000U, other_flags=0x08050010U;
// Authored A32 instruction fields, never copied from an original game image.
constexpr std::uint32_t Word(unsigned rotate, unsigned byte, bool flags,
                             a32::Condition condition, unsigned rn, unsigned rd) {
    return (static_cast<std::uint32_t>(condition)<<28U) | (1U<<25U) |
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
void ImmediateSemantics() {
    // Fixed expected expanded values; no runtime shift/ALU helper is used.
    // Arm A32 immediate operand is imm8 rotated right by twice rotate_imm.
    // S=1 updates NZC, preserves V; rotation zero preserves incoming C.
    constexpr std::array<std::uint32_t,16> bit{
        0x00000001U,0x40000000U,0x10000000U,0x04000000U,
        0x01000000U,0x00400000U,0x00100000U,0x00040000U,
        0x00010000U,0x00004000U,0x00001000U,0x00000400U,
        0x00000100U,0x00000040U,0x00000010U,0x00000004U};
    for (unsigned r=0;r<bit.size();++r) {
        Case(r,1,true,0xffffffffU,N|Z|C|V,bit[r],(r==0?C:0U)|V);
        Case(r,1,true,~bit[r],N|C|V,0U,Z|(r==0?C:0U)|V);
        Case(r,1,false,0xffffffffU,N|Z|C|V,bit[r],N|Z|C|V);
    }
    Case(0,0x3c,true,0x12abcdefU,N|Z|V,0x2cU,V);
    Case(0,0x3c,true,0x12abcdc3U,N|C|V,0U,Z|C|V);
    Case(0,0,true,0xffffffffU,N|C|V,0U,Z|C|V);
    Case(1,0,true,0xffffffffU,N|C|V,0U,Z|V);
    Case(15,0,true,0xffffffffU,N|C,0U,Z);
    // Carry follows expanded immediate bit 31, even when AND yields zero.
    Case(1,2,true,0xffffffffU,Z|V,0x80000000U,N|C|V);
    Case(1,2,true,0U,N|V,0U,Z|C|V);
    Case(4,0x80,true,0x80000000U,Z,0x80000000U,N|C);
    Case(1,0x80,true,0xffffffffU,N|Z|C|V,0x20U,V);
    Case(15,0x81,true,0xffffffffU,N|Z|C|V,0x204U,V);
    Case(1,2,false,0xffffffffU,Z|V,0x80000000U,Z|V);
    Case(1,0,false,0xffffffffU,N|C|V,0U,N|C|V);
    // Rejected conditions change neither destination nor flags.
    Case(1,2,true,0xffffffffU,C|V,0U,C|V,a32::Condition::Eq,false);
    Case(1,2,true,0xffffffffU,Z|V,0x80000000U,N|C|V,a32::Condition::Eq);
    Case(1,0,true,0xffffffffU,N|Z|V,0U,N|Z|V,a32::Condition::Ne,false);
    Case(1,0,true,0xffffffffU,C,0U,Z,a32::Condition::Ne);
    Case(1,2,true,0U,V,0U,V,a32::Condition::Cs,false);
    Case(1,2,true,0U,C|V,0U,Z|C|V,a32::Condition::Cs);
    Case(1,2,false,0xffffffffU,Z|V,0U,Z|V,a32::Condition::Ne,false);
    // All admitted general registers, overlap, SP and LR remain ordinary data.
    for (unsigned r=0;r<15;++r)
        Case(15,0x81,true,0xffffffffU,N|Z|C|V,0x204U,V,a32::Condition::Al,true,r,r);
    Case(1,2,true,0x80000000U,Z,0x80000000U,N|C,a32::Condition::Al,true,13,14);
    Case(0,0x81,false,0x12345681U,N|Z|C|V,0x81U,N|Z|C|V,a32::Condition::Al,true,14,13);
}
}
int main() {
    ImmediateSemantics();
    if (failures) return EXIT_FAILURE;
    std::cout << "PASS: authored immediate AND/ANDS values, rotations, NZCV, conditions, overlap, preserved CPU state and strict next-code stop\n";
}
