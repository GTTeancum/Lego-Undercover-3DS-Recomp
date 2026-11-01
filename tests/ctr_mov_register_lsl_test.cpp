#include "host/supplemental_registry.h"
#include "runtime/ctr_recorded_step.h"
#include "runtime/ctr_memory.h"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace {
using namespace lego::ctr;
unsigned failures{}, cases{};
#define CHECK(x) do { if (!(x)) { if (failures < 12) std::cerr << "FAIL " << __LINE__ << ": " #x "\n"; ++failures; } } while (0)
constexpr auto N=a32::kFlagN, Z=a32::kFlagZ, C=a32::kFlagC, V=a32::kFlagV;
constexpr std::uint32_t kPc=0x00860000U, kOther=0x08050010U;
// Authored architectural fields, not words copied from any game image.
constexpr std::uint32_t Word(unsigned rd, unsigned rm, unsigned rs,
                             bool flags, unsigned condition=14) {
    return (condition<<28U)|(13U<<21U)|(flags?1U<<20U:0U)|
           (rd<<12U)|(rs<<8U)|(1U<<4U)|rm;
}
struct Expected {std::uint32_t value; bool carry;};
// Independent bit-at-a-time arithmetic reference. No runtime shift/flag helper
// or C++ variable-width shift is used. Arm DDI 0406C.d, LSL (register)
// and A8.4: the low byte supplies the count; count zero preserves incoming C.
// https://documentation-service.arm.com/static/5f8daeb7f86e16515cdb8c4e
Expected Reference(std::uint32_t value, std::uint32_t count, bool carry) {
    for (unsigned n=0; n<count%256U; ++n) {
        carry=value>=0x80000000U;
        value=static_cast<std::uint32_t>(std::uint64_t(value)*2U);
    }
    return {value,carry};
}
bool Passes(unsigned condition, std::uint32_t flags) {
    const bool n=flags&N, z=flags&Z, c=flags&C, v=flags&V;
    const std::array<bool,15> truth{z,!z,c,!c,n,!n,v,!v,c&&!z,!c||z,n==v,n!=v,!z&&(n==v),z||(n!=v),true};
    return truth[condition];
}
std::uint32_t Flags(unsigned bits) {
    return (bits&8U?N:0U)|(bits&4U?Z:0U)|(bits&2U?C:0U)|(bits&1U?V:0U);
}
void Same(const a32::GuestState& actual, const a32::GuestState& expected) {
    CHECK(actual.r==expected.r && actual.cpsr==expected.cpsr);
    CHECK(actual.fpscr==expected.fpscr && actual.vfp==expected.vfp);
    CHECK(actual.thread_pointer==expected.thread_pointer);
    CHECK(actual.exclusive_address==expected.exclusive_address &&
          actual.exclusive_token==expected.exclusive_token &&
          actual.exclusive_size==expected.exclusive_size &&
          actual.exclusive_valid==expected.exclusive_valid);
}
void Run(std::uint32_t value, std::uint32_t count, std::uint32_t flags,
         bool set_flags, unsigned condition=14, unsigned rd=5, unsigned rm=7,
         unsigned rs=9, bool both_paths=false) {
    const a32::PackedOp op{Word(rd,rm,rs,set_flags,condition),
        a32::EncodeMetadata(a32::Opcode::CoreAlu,static_cast<a32::Condition>(condition))};
    const a32::BlockShard shard{kPc,kPc+0xffcU,nullptr,0};
    const a32::Registry base{&shard,1,nullptr,0};
    const a32::Block block{kPc,&op,1,false};
    lego::host::SupplementalRegistry merged(base,std::span(&block,1));
    GuestMemory memory;
    a32::GuestState initial{};
    for (unsigned i=0;i<initial.r.size();++i)initial.r[i]=0x12340000U+i*0x01010101U;
    initial.r[rm]=value;initial.r[rs]=count;initial.r[15]=kPc;
    initial.cpsr=kOther|flags;initial.fpscr=0x02468135U;initial.vfp.fill(0x5aa51234U);
    initial.thread_pointer=0x08007000U;initial.exclusive_address=0x08009000U;
    initial.exclusive_token=0x12345678abcdef01ULL;initial.exclusive_size=4;initial.exclusive_valid=true;
    auto expected=initial;
    if (Passes(condition,flags)) {
        // Aliases deliberately use the final incoming register values, before
        // the destination changes. In particular Rd==Rs must not reread Rd.
        const auto answer=Reference(initial.r[rm],initial.r[rs],(flags&C)!=0U);
        expected.r[rd]=answer.value;
        if (set_flags) expected.cpsr=(initial.cpsr&~(N|Z|C))|
            (answer.value&0x80000000U?N:0U)|(answer.value==0U?Z:0U)|(answer.carry?C:0U);
    }
    expected.r[15]=kPc+4U;
    auto state=initial;
    const auto result=StepRecordedA32(merged.registry(),state,memory);
    CHECK(result.kind==a32::ExitKind::Fallthrough && result.pc==kPc+4U);
    CHECK(result.fallback==a32::FallbackReason::None && result.detail==0U);
    Same(state,expected);
    const auto stopped=state;
    CHECK(StepRecordedA32(merged.registry(),state,memory).kind==a32::ExitKind::MissingBlock);
    Same(state,stopped);
    if (both_paths) {
        state=initial;
        const auto direct=a32::ExecuteBlock(block,state,memory);
        CHECK(direct.kind==a32::ExitKind::Fallthrough && direct.pc==kPc+4U);
        Same(state,expected);
        state=initial;
        const auto dispatched=a32::Dispatch(merged.registry(),kPc,state,memory,nullptr,nullptr,4U);
        CHECK(dispatched.kind==a32::ExitKind::MissingBlock && dispatched.pc==kPc+4U);
        Same(state,expected);
    }
    CHECK(a32::FindBlock(base,kPc)==nullptr);
    ++cases;
}
void FixedExpectations() {
    // Pin the independent reference against fixed architectural boundary cases.
    struct Example {std::uint32_t in,count,value;bool before,carry;};
    constexpr Example examples[]{
        {0x80000001U,0,0x80000001U,false,false},
        {0x80000001U,0,0x80000001U,true,true},
        {0x80000001U,1,2U,false,true},
        {3U,31,0x80000000U,false,true},
        {2U,31,0U,false,true},
        {1U,32,0U,false,true},
        {2U,32,0U,true,false},
        {0xffffffffU,33,0U,true,false},
        {0xffffffffU,255,0U,true,false},
        {0x80000001U,256,0x80000001U,true,true},
        {0x80000001U,257,2U,false,true},
        {1U,0x12340020U,0U,false,true},
        {0xffffffffU,0xffffffffU,0U,true,false}};
    for (const auto& e:examples) {
        const auto answer=Reference(e.in,e.count,e.before);
        CHECK(answer.value==e.value && answer.carry==e.carry);
        for (bool s:{false,true}) Run(e.in,e.count,N|Z|V|(e.before?C:0U),s,14,5,7,9,true);
    }
}
void AllCountsAndFlags() {
    constexpr std::array<std::uint32_t,10> values{0U,1U,2U,3U,0x7fffffffU,0x80000000U,
        0x80000001U,0xffffffffU,0x13579bdfU,0xabcdef12U};
    for (unsigned amount=0;amount<256;++amount)
        for (auto value:values)
            for (unsigned flags=0;flags<16;++flags)
                for (bool s:{false,true}) Run(value,amount,Flags(flags),s);
    // High bits do not change the count, including 256 becoming zero.
    for (auto high:{0x100U,0x80000000U,0xffffff00U})
        for (unsigned amount=0;amount<256;++amount)
            for (bool s:{false,true}) Run(0x80000001U,high|amount,N|Z|C|V,s);
}
void ConditionsAndAliases() {
    for (unsigned condition=0;condition<15;++condition)
        for (unsigned flags=0;flags<16;++flags)
            for (bool s:{false,true})
                for (auto count:{0U,1U,31U,32U,33U,255U,256U})
                    Run(0x80000001U,count,Flags(flags),s,condition,5,7,9,true);
    // Each register can be an ordinary source, shift count or destination;
    // cover every pairwise and three-way alias plus SP/LR explicitly.
    for (unsigned reg=0;reg<15;++reg)
        for (bool s:{false,true})
            for (auto count:{0U,1U,31U,32U,33U,255U,256U}) {
                Run(0x80000001U,count,N|Z|C|V,s,14,reg,reg,(reg+1)%15,true);
                Run(0x80000001U,count,N|Z|C|V,s,14,reg,(reg+1)%15,reg,true);
                Run(count,count,N|Z|C|V,s,14,(reg+1)%15,reg,reg,true);
                Run(count,count,N|Z|C|V,s,14,reg,reg,reg,true);
            }
}
}
int main() {
    FixedExpectations();AllCountsAndFlags();ConditionsAndAliases();
    if (failures) return EXIT_FAILURE;
    std::cout << "PASS: " << cases << " authored MOV register-LSL cases; all low-byte counts, flags, conditions, aliases, full state preservation and strict missing-code stops\n";
}
