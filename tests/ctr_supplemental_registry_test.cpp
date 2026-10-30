#include "host/supplemental_registry.h"
#include "runtime/ctr_recorded_step.h"
#include "runtime/ctr_memory.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <new>
namespace {bool fail_allocation=false;}
void* operator new(std::size_t n) {if(fail_allocation){fail_allocation=false;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
namespace {
using namespace lego::ctr;
int failures{};
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}}while(0)
const a32::PackedOp base_ops[]{{0xE3A00007,a32::EncodeMetadata(a32::Opcode::MovImm,a32::Condition::Al,2)}};
const a32::PackedOp extra_ops[]{
 {0xE2800001,a32::EncodeMetadata(a32::Opcode::Add,a32::Condition::Al,2)},
 {0xE1A0F00E,a32::EncodeMetadata(a32::Opcode::MovReg,a32::Condition::Al)}};
const a32::Block base_blocks[]{{0x100000,base_ops,1,false},{0x100020,base_ops,1,false}};
const a32::BlockShard shards[]{{0x100000,0x100ffc,base_blocks,2},{0x101000,0x101ffc,nullptr,0}};
const a32::Registry base{shards,2,nullptr,0};
void ValidAndExecution(){
 const a32::Block extra[]{{0x101ffc,base_ops,1,false},{0x100004,extra_ops,2,false},{0x100018,extra_ops,2,false}};
 lego::host::SupplementalRegistry merged(base,extra);const auto& r=merged.registry();
 CHECK(r.shard_count==2 && r.shards[0].block_count==4 && r.shards[1].block_count==1);
 CHECK(base.shards[0].blocks==base_blocks && base.shards[0].block_count==2);
 CHECK(a32::FindBlock(base,0x100004)==nullptr);
 CHECK(a32::FindBlock(r,0x100004)->ops==extra_ops);
 CHECK(a32::FindBlock(r,0x100020)->ops==base_ops);
 GuestMemory memory;a32::GuestState cpu{};cpu.cpsr=0x10;cpu.r[0]=41;cpu.r[14]=0x101ffc;cpu.r[15]=0x100004;
 CHECK(StepRecordedA32(r,cpu,memory).kind==a32::ExitKind::Fallthrough && cpu.r[0]==42 && cpu.r[15]==0x100008);
 CHECK(StepRecordedA32(r,cpu,memory).kind==a32::ExitKind::Branch && cpu.r[15]==0x101ffc && cpu.r[0]==42);
 cpu.r[15]=0x100010;const auto saved=cpu;
 CHECK(StepRecordedA32(r,cpu,memory).kind==a32::ExitKind::MissingBlock && cpu.r==saved.r);
 lego::host::SupplementalRegistry empty(base,{});CHECK(empty.registry().shards[0].blocks==base_blocks);
}
void Invalid(){
 for(const a32::Block b: {
   a32::Block{0x100000,extra_ops,2,false}, // Exact old entry.
   a32::Block{0x10001c,extra_ops,2,false}, // Overlaps later old block.
   a32::Block{0x0ffffc,extra_ops,2,false}, // Before first shard.
   a32::Block{0x102000,extra_ops,2,false}, // After last shard.
   a32::Block{0x100ffc,extra_ops,2,false}, // Crosses page boundary.
   a32::Block{0x100002,extra_ops,2,false}, // Unaligned.
   a32::Block{0x100004,nullptr,2,false},
   a32::Block{0x100004,extra_ops,0,false},
   a32::Block{0x100004,extra_ops,0xffffffffU,false}}) {
   bool threw=false;try{lego::host::SupplementalRegistry r(base,std::span(&b,1));}catch(const std::invalid_argument&){threw=true;}
   CHECK(threw && base.shards[0].blocks==base_blocks && base.shards[0].block_count==2);
 }
 const a32::Block duplicate[]{{0x100004,extra_ops,2,false},{0x100008,extra_ops,2,false}};
 bool threw=false;try{lego::host::SupplementalRegistry r(base,duplicate);}catch(const std::invalid_argument&){threw=true;}CHECK(threw);
 const a32::Registry bad{};threw=false;try{lego::host::SupplementalRegistry r(bad,{});}catch(const std::invalid_argument&){threw=true;}CHECK(threw);
 fail_allocation=true;threw=false;try{lego::host::SupplementalRegistry r(base,{});}catch(const std::bad_alloc&){threw=true;}CHECK(threw && !fail_allocation);
 // A failed construction cannot change the previously compiled pages or a later build.
 lego::host::SupplementalRegistry ok(base,{});CHECK(ok.registry().shards[0].blocks==base_blocks);
}

// These instruction words are independently authored from A32 field layouts.
// No original-game words, addresses, disassembly or captured values are used.
constexpr std::uint32_t kShiftPc=0x00800000U;
constexpr std::uint32_t kOtherCpsr=0x08050010U; // Q, GE and user-mode bits.
constexpr auto N=a32::kFlagN, Z=a32::kFlagZ, C=a32::kFlagC, V=a32::kFlagV;
const a32::BlockShard shift_shard{kShiftPc,kShiftPc+0xffcU,nullptr,0};
const a32::Registry shift_base{&shift_shard,1,nullptr,0};

constexpr std::uint32_t ShiftWord(unsigned opcode, bool set_flags,
                                unsigned shift_type, unsigned amount,
                                unsigned rd=2, unsigned rm=1, unsigned rn=3,
                                a32::Condition condition=a32::Condition::Al){
 return (static_cast<std::uint32_t>(condition)<<28U) | (opcode<<21U) |
        (set_flags?1U<<20U:0U) | ((opcode==13U?0U:rn)<<16U) |
        (rd<<12U) | (amount<<7U) | (shift_type<<5U) | rm;
}

void ShiftCase(unsigned opcode, unsigned type, unsigned amount, bool set_flags,
               std::uint32_t left, std::uint32_t input,
               std::uint32_t flags_before, std::uint32_t value,
               std::uint32_t flags_after,
               a32::Condition condition=a32::Condition::Al, bool executes=true,
               unsigned rd=2, unsigned rm=1, unsigned rn=3){
 const a32::PackedOp op{ShiftWord(opcode,set_flags,type,amount,rd,rm,rn,condition),
                      a32::EncodeMetadata(a32::Opcode::CoreAlu,condition)};
 const a32::Block block{kShiftPc,&op,1,false};
 lego::host::SupplementalRegistry merged(shift_base,std::span(&block,1));
 GuestMemory memory;
 a32::GuestState cpu{};
 for(unsigned i=0;i<cpu.r.size();++i)cpu.r[i]=0x10203040U+i*0x01010101U;
 cpu.r[rm]=input;
 if(opcode!=13U)cpu.r[rn]=left;
 cpu.r[15]=kShiftPc;cpu.cpsr=kOtherCpsr|flags_before;
 cpu.fpscr=0x01234567U;cpu.thread_pointer=0x08004000U;
 cpu.vfp.fill(0xA55A1234U);
 cpu.exclusive_address=0x08005000U;cpu.exclusive_token=0x123456789abcdef0ULL;
 cpu.exclusive_size=4;cpu.exclusive_valid=true;
 auto expected=cpu;
 if(executes)expected.r[rd]=value;
 expected.r[15]=kShiftPc+4U;expected.cpsr=kOtherCpsr|flags_after;
 const auto result=StepRecordedA32(merged.registry(),cpu,memory);
 CHECK(result.kind==a32::ExitKind::Fallthrough && result.pc==kShiftPc+4U);
 CHECK(result.fallback==a32::FallbackReason::None && result.detail==0U);
 CHECK(cpu.r==expected.r && cpu.cpsr==expected.cpsr);
 CHECK(cpu.fpscr==expected.fpscr && cpu.vfp==expected.vfp);
 CHECK(cpu.thread_pointer==expected.thread_pointer);
 CHECK(cpu.exclusive_address==expected.exclusive_address &&
       cpu.exclusive_token==expected.exclusive_token &&
       cpu.exclusive_size==expected.exclusive_size &&
       cpu.exclusive_valid==expected.exclusive_valid);
 CHECK(a32::FindBlock(shift_base,kShiftPc)==nullptr);
}

void ImmediateShiftSemantics(){
 // Expected values/flags are fixed arithmetic examples, not an implementation
 // of the runtime shifter. Encoded zero means LSR/ASR #32 and RRX, but LSL #0.
 ShiftCase(13,0,0,true,0,0x80000001U,Z|C|V,0x80000001U,N|C|V);
 ShiftCase(13,0,0,true,0,0U,N|V,0U,Z|V);
 ShiftCase(13,0,1,true,0,0x80000001U,N|V,2U,C|V);
 ShiftCase(13,0,31,true,0,3U,0,0x80000000U,N|C);
 ShiftCase(13,0,31,true,0,2U,0,0U,Z|C);
 ShiftCase(13,1,0,true,0,0x80000001U,N|V,0U,Z|C|V);
 ShiftCase(13,1,1,true,0,0x80000001U,N,0x40000000U,C);
 ShiftCase(13,1,31,true,0,0xc0000000U,V,1U,C|V);
 ShiftCase(13,2,0,true,0,0x80000000U,V,0xffffffffU,N|C|V);
 ShiftCase(13,2,0,true,0,0x7fffffffU,N|C,0U,Z);
 ShiftCase(13,2,1,true,0,0x80000001U,V,0xc0000000U,N|C|V);
 ShiftCase(13,2,31,true,0,0x80000000U,C,0xffffffffU,N);
 ShiftCase(13,3,1,true,0,3U,0,0x80000001U,N|C);
 ShiftCase(13,3,31,true,0,0x80000003U,C|V,7U,V);
 ShiftCase(13,3,0,true,0,3U,0,1U,C);
 ShiftCase(13,3,0,true,0,2U,C|V,0x80000001U,N|V);

 // S=0 preserves every CPSR bit even when the shifter would change carry.
 ShiftCase(13,0,1,false,0,0x80000001U,N|Z|C|V,2U,N|Z|C|V);
 ShiftCase(13,1,0,false,0,0x80000001U,N|Z|C|V,0U,N|Z|C|V);
 ShiftCase(13,2,31,false,0,0x80000001U,N|Z|C|V,0xffffffffU,N|Z|C|V);
 ShiftCase(13,3,1,false,0,0x80000001U,N|Z|C|V,0xc0000000U,N|Z|C|V);
 ShiftCase(13,3,0,false,0,0x80000001U,N|Z|C|V,0xc0000000U,N|Z|C|V);

 // ADD derives C/V from addition, not the operand shifter's discarded bit.
 ShiftCase(4,0,1,true,0U,0x80000000U,N|C|V,0U,Z);
 ShiftCase(4,1,1,true,0xffffffffU,2U,V,0U,Z|C);
 ShiftCase(4,1,1,true,0x7fffffffU,2U,C,0x80000000U,N|V);
 ShiftCase(4,3,0,true,0x80000000U,0U,C,0U,Z|C|V);
 ShiftCase(4,1,24,true,0xfffffff0U,0xff000000U,V,0xefU,C);
 ShiftCase(4,1,1,false,0xffffffffU,2U,N|Z|C|V,0U,N|Z|C|V);

 // ORR uses shifter carry, leaves V intact, and computes N/Z from the OR.
 ShiftCase(12,0,1,true,1U,0x80000000U,V,1U,C|V);
 ShiftCase(12,0,0,true,0x80000000U,0U,C|V,0x80000000U,N|C|V);
 ShiftCase(12,1,0,true,0U,0x7fffffffU,N|C|V,0U,Z|V);
 ShiftCase(12,2,31,true,0U,0x80000000U,C,0xffffffffU,N);
 ShiftCase(12,3,0,true,0x10U,2U,C|V,0x80000011U,N|V);
 ShiftCase(12,0,16,false,0x80000001U,0x12345678U,Z|C|V,0xd6780001U,Z|C|V);

 for(unsigned opcode:{4U,12U,13U}){
   ShiftCase(opcode,0,17,true,0xffffffffU,0xffffffffU,C|V,0U,C|V,
             a32::Condition::Eq,false);
 }
 ShiftCase(13,0,1,true,0U,3U,Z|V,6U,V,a32::Condition::Eq);

 // SP/LR and source/destination overlap remain ordinary A32 registers here.
 ShiftCase(4,0,1,false,4U,3U,C,10U,C,a32::Condition::Al,true,13,13,14);
 ShiftCase(12,0,31,true,1U,1U,V,0x80000001U,N|V,
           a32::Condition::Al,true,14,13,14);
 ShiftCase(13,3,1,true,0U,3U,V,0x80000001U,N|C|V,
           a32::Condition::Al,true,14,14);
}

void AndAndSubtractShiftSemantics(){
 // Fixed examples exercise ordinary AND/SUB through the merged registry.
 // The expected results do not call the runtime shifter or flag helpers.
 // S=0 preserves all CPSR bits while selecting bytes or shifting operands.
 ShiftCase(0,0,0,false,0xffU,0x1234U,N|Z|C|V,0x34U,N|Z|C|V);
 ShiftCase(0,0,2,false,0xffU,0x63U,N|Z|C|V,0x8cU,N|Z|C|V);
 ShiftCase(0,1,8,false,0xffU,0x89abcdefU,N|Z|C|V,0xcdU,N|Z|C|V);
 ShiftCase(0,1,16,false,0xffU,0x89abcdefU,N|Z|C|V,0xabU,N|Z|C|V);
 ShiftCase(0,1,24,false,0xffU,0x89abcdefU,N|Z|C|V,0x89U,N|Z|C|V);
 ShiftCase(2,0,2,false,0x90U,0x23U,N|Z|C|V,4U,N|Z|C|V);
 ShiftCase(2,1,0,false,7U,0x80000001U,N|Z|C|V,7U,N|Z|C|V);
 ShiftCase(2,2,0,false,0U,0x80000001U,N|Z|C|V,1U,N|Z|C|V);
 ShiftCase(2,3,0,false,0x80000000U,2U,N|Z|C|V,0xffffffffU,N|Z|C|V);

 // ANDS takes carry from the operand shift and preserves V, including when
 // a zero result differs from the shifted operand's sign or outgoing carry.
 ShiftCase(0,0,0,true,0x80000000U,0x80000001U,Z|C|V,0x80000000U,N|C|V);
 ShiftCase(0,0,0,true,0xffU,0xffffff00U,N|V,0U,Z|V);
 ShiftCase(0,0,1,true,1U,0x80000001U,N|V,0U,Z|C|V);
 ShiftCase(0,0,31,true,0x80000000U,3U,0,0x80000000U,N|C);
 ShiftCase(0,0,31,true,0xffffffffU,2U,0,0U,Z|C);
 ShiftCase(0,1,0,true,0xffffffffU,0x80000001U,N|V,0U,Z|C|V);
 ShiftCase(0,1,1,true,0xc0000000U,0x80000001U,N,0x40000000U,C);
 ShiftCase(0,1,31,true,1U,0x80000000U,C|V,1U,V);
 ShiftCase(0,2,0,true,0x80000000U,0x80000000U,Z|V,0x80000000U,N|C|V);
 ShiftCase(0,2,0,true,0xffffffffU,0x7fffffffU,N|C|V,0U,Z|V);
 ShiftCase(0,2,1,true,0x80000000U,0x80000001U,V,0x80000000U,N|C|V);
 ShiftCase(0,3,1,true,0x80000000U,3U,V,0x80000000U,N|C|V);
 ShiftCase(0,3,31,true,3U,0x80000003U,C|V,3U,V);
 ShiftCase(0,3,0,true,1U,3U,V,1U,C|V);
 ShiftCase(0,3,0,true,0x80000000U,2U,C|V,0x80000000U,N|V);

 // SUBS derives C (no borrow) and V from subtraction, not shifter carry.
 // These include positive/negative overflow, equality and unsigned borrow.
 ShiftCase(2,0,2,true,8U,1U,N|Z|V,4U,C);
 ShiftCase(2,0,2,true,4U,1U,N|V,0U,Z|C);
 ShiftCase(2,0,2,true,3U,1U,Z|C|V,0xffffffffU,N);
 ShiftCase(2,0,1,true,0U,0x80000002U,C|V,0xfffffffcU,N);
 ShiftCase(2,0,1,true,10U,2U,N|V,6U,C);
 ShiftCase(2,0,0,true,0x7fffffffU,0xffffffffU,C,0x80000000U,N|V);
 ShiftCase(2,0,0,true,0x80000000U,1U,N,0x7fffffffU,C|V);
 ShiftCase(2,0,0,true,0U,0x80000000U,C,0x80000000U,N|V);
 ShiftCase(2,0,31,true,0x80000000U,1U,N|V,0U,Z|C);
 ShiftCase(2,1,0,true,0U,0x80000001U,N|V,0U,Z|C);
 ShiftCase(2,1,1,true,0U,3U,C|V,0xffffffffU,N);
 ShiftCase(2,2,0,true,0U,0x80000001U,C|V,1U,0);
 ShiftCase(2,2,0,true,0U,0x7fffffffU,N|V,0U,Z|C);
 ShiftCase(2,2,31,true,0x7fffffffU,0x80000000U,C,0x80000000U,N|V);
 ShiftCase(2,3,1,true,0x80000000U,3U,C|V,0xffffffffU,N);
 ShiftCase(2,3,31,true,10U,0x80000003U,V,3U,C);
 ShiftCase(2,3,0,true,0x80000000U,2U,C|V,0xffffffffU,N);
 ShiftCase(2,3,0,true,0U,3U,V,0xffffffffU,N);

 // False conditions suppress both destination and flags; true conditions
 // use the prior flags even when the operation replaces those same flags.
 for(unsigned opcode:{0U,2U}){
   ShiftCase(opcode,3,0,true,0U,3U,N|C|V,0U,N|C|V,
             a32::Condition::Eq,false);
   ShiftCase(opcode,0,2,true,9U,2U,Z|C|V,0U,Z|C|V,
             a32::Condition::Ne,false);
 }
 ShiftCase(0,0,1,true,2U,1U,Z|C|V,2U,V,a32::Condition::Eq);
 ShiftCase(2,0,2,true,9U,2U,Z|V,1U,C,a32::Condition::Eq);
 ShiftCase(0,0,1,true,2U,1U,N|V,2U,V,a32::Condition::Mi);
 ShiftCase(2,0,2,true,0U,1U,C|V,0xfffffffcU,N,a32::Condition::Cs);
 ShiftCase(2,0,2,true,9U,2U,V,0U,V,a32::Condition::Vc,false);

 // SP/LR and overlapping operands use their incoming values before writing
 // the destination. None of these synthetic encodings reads or writes PC.
 ShiftCase(0,0,0,false,0xffU,0x2aU,Z|C|V,0x2aU,Z|C|V,
           a32::Condition::Al,true,14,13,14);
 ShiftCase(2,0,2,true,0x100U,0x10U,N|V,0xc0U,C,
           a32::Condition::Al,true,13,13,14);
 ShiftCase(0,1,1,true,0x33U,0x33U,V,0x11U,C|V,
           a32::Condition::Al,true,4,4,4);
 ShiftCase(2,0,1,true,0x21U,0x21U,C|V,0xffffffdfU,N,
           a32::Condition::Al,true,4,4,4);
}

constexpr std::uint32_t TstImmediateWord(unsigned rn, unsigned rotate,
                                         unsigned imm8,
                                         a32::Condition condition=a32::Condition::Al){
 return (static_cast<std::uint32_t>(condition)<<28U) | (1U<<25U) |
        (8U<<21U) | (1U<<20U) | (rn<<16U) | (rotate<<8U) | imm8;
}

void TstImmediateCase(unsigned rotate, unsigned imm8, std::uint32_t left,
                      std::uint32_t flags_before, std::uint32_t flags_after,
                      a32::Condition condition=a32::Condition::Al,
                      unsigned rn=6){
 const a32::PackedOp op{TstImmediateWord(rn,rotate,imm8,condition),
                      a32::EncodeMetadata(a32::Opcode::CoreAlu,condition)};
 const a32::Block block{kShiftPc,&op,1,false};
 lego::host::SupplementalRegistry merged(shift_base,std::span(&block,1));
 GuestMemory memory;
 a32::GuestState cpu{};
 for(unsigned i=0;i<cpu.r.size();++i)cpu.r[i]=0x24681000U+i*0x01020304U;
 cpu.r[rn]=left;cpu.r[15]=kShiftPc;cpu.cpsr=kOtherCpsr|flags_before;
 cpu.fpscr=0x01234567U;cpu.vfp.fill(0xC35A2496U);
 cpu.thread_pointer=0x08008000U;
 cpu.exclusive_address=0x08009000U;cpu.exclusive_token=0x13579bdf2468ace0ULL;
 cpu.exclusive_size=8;cpu.exclusive_valid=true;
 auto expected=cpu;
 expected.r[15]=kShiftPc+4U;expected.cpsr=kOtherCpsr|flags_after;
 const auto result=StepRecordedA32(merged.registry(),cpu,memory);
 CHECK(result.kind==a32::ExitKind::Fallthrough && result.pc==kShiftPc+4U);
 CHECK(result.fallback==a32::FallbackReason::None && result.detail==0U);
 // TST has no destination: this includes the encoded Rd=0 register and Rn.
 CHECK(cpu.r==expected.r && cpu.cpsr==expected.cpsr);
 CHECK(cpu.fpscr==expected.fpscr && cpu.vfp==expected.vfp);
 CHECK(cpu.thread_pointer==expected.thread_pointer);
 CHECK(cpu.exclusive_address==expected.exclusive_address &&
       cpu.exclusive_token==expected.exclusive_token &&
       cpu.exclusive_size==expected.exclusive_size &&
       cpu.exclusive_valid==expected.exclusive_valid);
 CHECK(a32::FindBlock(shift_base,kShiftPc)==nullptr);
}

void TstImmediateSemantics(){
 // Independent fixed expectations from Arm DDI 0100I A4.1.117 and A5.1.3:
 // https://documentation-service.arm.com/static/5f8dacc8f86e16515cdb865a
 // No runtime expansion/shift helper computes these expected flag values.
 // Each position is the one set bit of immediate byte 1 at that rotation.
 struct Rotation {unsigned encoded;std::uint32_t position,carry;};
 constexpr Rotation rotations[]{
   {0,0x00000001U,C},{1,0x40000000U,0},{2,0x10000000U,0},{3,0x04000000U,0},
   {4,0x01000000U,0},{5,0x00400000U,0},{6,0x00100000U,0},{7,0x00040000U,0},
   {8,0x00010000U,0},{9,0x00004000U,0},{10,0x00001000U,0},{11,0x00000400U,0},
   {12,0x00000100U,0},{13,0x00000040U,0},{14,0x00000010U,0},{15,0x00000004U,0}};
 for(const auto& r:rotations){
   TstImmediateCase(r.encoded,1,r.position,N|Z|C|V,r.carry|V);
   TstImmediateCase(r.encoded,1,~r.position,N|Z|C|V,Z|r.carry|V);
 }
 // Rotation zero preserves either incoming carry; AND determines N/Z only.
 TstImmediateCase(0,0x3c,0x2cU,N|Z|V,V);
 TstImmediateCase(0,0x3c,0xc3U,N|V,Z|V);
 TstImmediateCase(0,0,0xffffffffU,N|C|V,Z|C|V);
 // A nonzero encoded rotation of zero clears C, even though both encodings
 // below represent the same operand value as unrotated byte zero.
 TstImmediateCase(1,0,0xffffffffU,N|C|V,Z|V);
 TstImmediateCase(15,0,0xffffffffU,N|C,Z);
 // Carry comes from the expanded operand, not the AND result. In particular
 // zero AND 0x80000000 sets Z and C together, with N clear and V preserved.
 TstImmediateCase(1,2,0xffffffffU,Z|V,N|C|V);
 TstImmediateCase(1,2,0U,N|V,Z|C|V);
 TstImmediateCase(4,0x80,0x80000000U,Z,N|C);
 TstImmediateCase(1,0x80,0x20U,N|Z|C|V,V);
 TstImmediateCase(15,0x81,0x204U,N|Z|C|V,V);
 // Condition checks precede all flag effects; true and false cases use the
 // same canonical TST encoding, with independently chosen prior flags.
 TstImmediateCase(1,2,0xffffffffU,C|V,C|V,a32::Condition::Eq);
 TstImmediateCase(1,2,0xffffffffU,Z|V,N|C|V,a32::Condition::Eq);
 TstImmediateCase(1,0,0xffffffffU,N|Z|V,N|Z|V,a32::Condition::Ne);
 TstImmediateCase(1,0,0xffffffffU,C,Z,a32::Condition::Ne);
 TstImmediateCase(1,2,0U,N|Z|V,N|Z|V,a32::Condition::Cs);
 TstImmediateCase(1,2,0U,C|V,Z|C|V,a32::Condition::Cs);
 TstImmediateCase(1,2,0U,C|V,C|V,a32::Condition::Mi);
 TstImmediateCase(1,2,0U,N|V,Z|C|V,a32::Condition::Mi);
 TstImmediateCase(1,2,0U,N|C,N|C,a32::Condition::Vs);
 TstImmediateCase(1,2,0U,V,Z|C|V,a32::Condition::Vs);
 // Low registers, SP and LR are read without being written.
 TstImmediateCase(12,1,0x301U,N|Z|C|V,V,a32::Condition::Al,0);
 TstImmediateCase(1,1,0x40000000U,N|Z|C|V,V,a32::Condition::Al,13);
 TstImmediateCase(1,2,0x80000000U,Z,N|C,a32::Condition::Al,14);
}

void TstConditionalDispatch(){
 // An authored flag-test dispatch exercises the registry suffix at BNE,
 // both condition outcomes and a strict missing-code stop at the join.
 constexpr auto entry=kShiftPc+0x200U, nonzero=entry+0x40U, join=entry+0x80U;
 const a32::PackedOp test_ops[]{
   {TstImmediateWord(7,0,4),a32::EncodeMetadata(a32::Opcode::CoreAlu,a32::Condition::Al)},
   // BNE from entry+4: PC+8 plus 13 words reaches entry+0x40.
   {(1U<<28U)|(5U<<25U)|13U,a32::EncodeMetadata(a32::Opcode::Branch,a32::Condition::Ne)}};
 const a32::PackedOp zero_ops[]{
   {(14U<<28U)|(1U<<25U)|(13U<<21U)|(10U<<12U)|0x21U,
    a32::EncodeMetadata(a32::Opcode::MovImm,a32::Condition::Al,a32::Immediate)},
   {(14U<<28U)|(5U<<25U)|27U,a32::EncodeMetadata(a32::Opcode::Branch,a32::Condition::Al)}};
 const a32::PackedOp nonzero_ops[]{
   {(14U<<28U)|(1U<<25U)|(13U<<21U)|(10U<<12U)|0x42U,
    a32::EncodeMetadata(a32::Opcode::MovImm,a32::Condition::Al,a32::Immediate)},
   {(14U<<28U)|(5U<<25U)|13U,a32::EncodeMetadata(a32::Opcode::Branch,a32::Condition::Al)}};
 const a32::Block additions[]{{entry,test_ops,2,false},{entry+8U,zero_ops,2,false},
                             {nonzero,nonzero_ops,2,false}};
 lego::host::SupplementalRegistry merged(shift_base,additions);
 CHECK(a32::FindBlock(merged.registry(),entry+4U)==nullptr);
 GuestMemory memory;
 for(bool bit_set:{false,true}){
   a32::GuestState cpu{};cpu.r[0]=0x76543210U;cpu.r[7]=bit_set?0x84U:2U;
   cpu.r[10]=0xdeadbeefU;cpu.r[14]=0x00c0ffeeU;cpu.r[15]=entry;
   cpu.cpsr=kOtherCpsr|N|C|V;
   auto expected=cpu;expected.r[15]=entry+4U;
   const auto flags=kOtherCpsr|C|V|(bit_set?0U:Z);expected.cpsr=flags;
   const auto tested=StepRecordedA32(merged.registry(),cpu,memory);
   CHECK(tested.kind==a32::ExitKind::Fallthrough && tested.pc==entry+4U);
   CHECK(cpu.r==expected.r && cpu.cpsr==flags);
   const auto arm=bit_set?nonzero:entry+8U;
   const auto branch=StepRecordedA32(merged.registry(),cpu,memory);
   CHECK(branch.kind==(bit_set?a32::ExitKind::Branch:a32::ExitKind::Fallthrough));
   CHECK(branch.pc==arm && cpu.r[15]==arm && cpu.r[10]==0xdeadbeefU && cpu.cpsr==flags);
   CHECK(StepRecordedA32(merged.registry(),cpu,memory).kind==a32::ExitKind::Fallthrough);
   CHECK(cpu.r[10]==(bit_set?0x42U:0x21U) && cpu.r[15]==arm+4U);
   const auto joined=StepRecordedA32(merged.registry(),cpu,memory);
   CHECK(joined.kind==a32::ExitKind::Branch && joined.pc==join && cpu.r[15]==join);
   CHECK(cpu.r[0]==0x76543210U && cpu.r[7]==(bit_set?0x84U:2U));
   CHECK(cpu.r[14]==0x00c0ffeeU && cpu.cpsr==flags);
   const auto stopped=cpu;
   CHECK(StepRecordedA32(merged.registry(),cpu,memory).kind==a32::ExitKind::MissingBlock);
   CHECK(cpu.r==stopped.r && cpu.cpsr==stopped.cpsr);
 }
 CHECK(a32::FindBlock(shift_base,entry)==nullptr && a32::FindBlock(shift_base,nonzero)==nullptr);
}

void ComputedPcDispatch(){
 // A separate, authored four-arm dispatch proves PC+8, data-table reads and
 // ordinary ADD/MOV PC exits. None of these addresses or values comes from a
 // game image. Table bytes stay data, never decoded as supplemental code.
 constexpr auto entry=kShiftPc+0x400U, table=kShiftPc+0x440U;
 constexpr auto return_pc=kShiftPc+0x700U;
 const a32::PackedOp dispatch_ops[]{
   {(14U<<28U)|(1U<<25U)|(4U<<21U)|(15U<<16U)|(4U<<12U)|0x38U,
    a32::EncodeMetadata(a32::Opcode::Add,a32::Condition::Al,2)},
   {(14U<<28U)|(3U<<25U)|(1U<<24U)|(1U<<23U)|(1U<<20U)|(4U<<16U)|(5U<<12U),
    a32::EncodeMetadata(a32::Opcode::CoreMemory,a32::Condition::Al)},
   {ShiftWord(4,false,0,0,15,5,4),
    a32::EncodeMetadata(a32::Opcode::Add,a32::Condition::Al)}};
 a32::PackedOp arm_ops[4][2]{};
 a32::Block additions[5]{{entry,dispatch_ops,3,false}};
 GuestMemory memory;
 CHECK(memory.Map(kShiftPc,0x1000,MemoryPermission::Read|MemoryPermission::Write));
 for(unsigned i=0;i<4;++i){
   const auto arm=kShiftPc+0x480U+i*0x20U;
   arm_ops[i][0]={(14U<<28U)|(1U<<25U)|(13U<<21U)|(2U<<12U)|(0x20U+i),
                  a32::EncodeMetadata(a32::Opcode::MovImm,a32::Condition::Al,2)};
   arm_ops[i][1]={ShiftWord(13,false,0,0,15,14),
                  a32::EncodeMetadata(a32::Opcode::MovReg,a32::Condition::Al)};
   additions[i+1]={arm,arm_ops[i],2,false};
   CHECK(memory.Write32(table+i*4U,arm-table));
 }
 lego::host::SupplementalRegistry merged(shift_base,additions);
 for(unsigned i=0;i<4;++i){
   a32::GuestState cpu{};cpu.r[0]=i*4U;cpu.r[14]=return_pc;
   cpu.r[15]=entry;cpu.cpsr=kOtherCpsr|V;
   CHECK(StepRecordedA32(merged.registry(),cpu,memory).kind==a32::ExitKind::Fallthrough);
   CHECK(cpu.r[4]==table && cpu.r[15]==entry+4U);
   CHECK(StepRecordedA32(merged.registry(),cpu,memory).kind==a32::ExitKind::Fallthrough);
   CHECK(cpu.r[5]==0x40U+i*0x20U && cpu.r[15]==entry+8U);
   CHECK(StepRecordedA32(merged.registry(),cpu,memory).kind==a32::ExitKind::Branch);
   CHECK(cpu.r[15]==kShiftPc+0x480U+i*0x20U && cpu.r[14]==return_pc);
   CHECK(StepRecordedA32(merged.registry(),cpu,memory).kind==a32::ExitKind::Fallthrough);
   CHECK(cpu.r[2]==0x20U+i);
   CHECK(StepRecordedA32(merged.registry(),cpu,memory).kind==a32::ExitKind::Branch);
   CHECK(cpu.r[15]==return_pc && cpu.cpsr==(kOtherCpsr|V));
   const auto stopped=cpu;
   CHECK(StepRecordedA32(merged.registry(),cpu,memory).kind==a32::ExitKind::MissingBlock);
   CHECK(cpu.r==stopped.r && cpu.cpsr==stopped.cpsr);
 }
 for(unsigned i=0;i<4;++i){
   a32::GuestState data{};data.r[15]=table+i*4U;data.cpsr=kOtherCpsr;
   const auto before=data;
   CHECK(a32::FindBlock(merged.registry(),data.r[15])==nullptr);
   CHECK(StepRecordedA32(merged.registry(),data,memory).kind==a32::ExitKind::MissingBlock);
   CHECK(data.r==before.r && data.cpsr==before.cpsr);
 }
}

void InteriorLoopAndStackReturn(){
 // Independently authored memory-summing function. Its loop target is inside
 // one supplemental block, and POP obtains its return address from memory
 // after the function deliberately changes live LR. No game bytes are used.
 constexpr auto entry=kShiftPc+0x800U, loop=entry+16U, pop_pc=entry+32U;
 constexpr auto return_pc=kShiftPc+0xc00U, changed_lr=kShiftPc+0xd00U;
 constexpr auto data=0x0800a000U, stack_page=0x0800c000U, stack_top=stack_page+0x800U;
 constexpr auto saved_r5=0x13579bdfU, saved_r7=0x2468ace0U;
 const a32::PackedOp loop_ops[]{
   // STMDB sp!, {r5,r7,lr}
   {(14U<<28U)|(4U<<25U)|(1U<<24U)|(1U<<21U)|(13U<<16U)|
      (1U<<5U)|(1U<<7U)|(1U<<14U),
    a32::EncodeMetadata(a32::Opcode::CoreMemory,a32::Condition::Al)},
   {ShiftWord(13,false,0,0,14,6),
    a32::EncodeMetadata(a32::Opcode::MovReg,a32::Condition::Al)},
   {(14U<<28U)|(1U<<25U)|(13U<<21U)|(5U<<12U)|3U,
    a32::EncodeMetadata(a32::Opcode::MovImm,a32::Condition::Al,a32::Immediate)},
   {(14U<<28U)|(1U<<25U)|(13U<<21U)|(7U<<12U),
    a32::EncodeMetadata(a32::Opcode::MovImm,a32::Condition::Al,a32::Immediate)},
   // LDR r2, [r0], #4
   {(14U<<28U)|(1U<<26U)|(1U<<23U)|(1U<<20U)|(2U<<12U)|4U,
    a32::EncodeMetadata(a32::Opcode::CoreMemory,a32::Condition::Al)},
   {ShiftWord(4,false,0,0,7,2,7),
    a32::EncodeMetadata(a32::Opcode::Add,a32::Condition::Al)},
   {(14U<<28U)|(1U<<25U)|(2U<<21U)|(1U<<20U)|(5U<<16U)|(5U<<12U)|1U,
    a32::EncodeMetadata(a32::Opcode::Sub,a32::Condition::Al,a32::Immediate|a32::SetFlags)},
   // BNE: PC+8 at entry+36, minus five words reaches entry+16.
   {(1U<<28U)|(5U<<25U)|0x00fffffbU,
    a32::EncodeMetadata(a32::Opcode::Branch,a32::Condition::Ne)}};
 const a32::PackedOp pop_op{
   (14U<<28U)|(4U<<25U)|(1U<<23U)|(1U<<21U)|(1U<<20U)|(13U<<16U)|
      (1U<<5U)|(1U<<7U)|(1U<<15U),
   a32::EncodeMetadata(a32::Opcode::CoreMemory,a32::Condition::Al)};
 const a32::Block additions[]{{entry,loop_ops,8,false},{pop_pc,&pop_op,1,false}};
 lego::host::SupplementalRegistry merged(shift_base,additions);
 CHECK(a32::FindBlock(merged.registry(),loop)==nullptr); // Must resolve a suffix.
 GuestMemory memory;
 CHECK(memory.Map(data,0x1000,MemoryPermission::Read|MemoryPermission::Write));
 CHECK(memory.Map(stack_page,0x1000,MemoryPermission::Read|MemoryPermission::Write));
 CHECK(memory.Write32(data,2U) && memory.Write32(data+4U,7U) && memory.Write32(data+8U,11U));
 a32::GuestState cpu{};cpu.cpsr=kOtherCpsr|V;cpu.r[0]=data;
 cpu.r[5]=saved_r5;cpu.r[6]=changed_lr;cpu.r[7]=saved_r7;
 cpu.r[13]=stack_top;cpu.r[14]=return_pc;cpu.r[15]=entry;
 unsigned steps=0, loop_visits=0, taken=0;
 while(cpu.r[15]!=pop_pc && steps<32U){
   const auto pc=cpu.r[15];
   if(pc==loop)++loop_visits;
   const auto result=StepRecordedA32(merged.registry(),cpu,memory);++steps;
   if(pc==entry+28U && result.kind==a32::ExitKind::Branch){
     ++taken;CHECK(result.pc==loop && cpu.r[15]==loop);
   }else CHECK(result.kind==a32::ExitKind::Fallthrough && result.pc==pc+4U);
 }
 CHECK(steps==16U && loop_visits==3U && taken==2U && cpu.r[15]==pop_pc);
 CHECK(cpu.r[0]==data+12U && cpu.r[2]==11U && cpu.r[5]==0U && cpu.r[7]==20U);
 CHECK(cpu.r[13]==stack_top-12U && cpu.r[14]==changed_lr);
 std::uint32_t saved_word{};
 CHECK(memory.Read32(stack_top-12U,&saved_word) && saved_word==saved_r5);
 CHECK(memory.Read32(stack_top-8U,&saved_word) && saved_word==saved_r7);
 CHECK(memory.Read32(stack_top-4U,&saved_word) && saved_word==return_pc);
 const auto returned=StepRecordedA32(merged.registry(),cpu,memory);++steps;
 CHECK(steps==17U && returned.kind==a32::ExitKind::Branch && returned.pc==return_pc);
 CHECK(cpu.r[13]==stack_top && cpu.r[14]==changed_lr && cpu.r[15]==return_pc);
 CHECK(cpu.r[5]==saved_r5 && cpu.r[7]==saved_r7 && cpu.cpsr==(kOtherCpsr|Z|C));
 const auto after_return=cpu;
 CHECK(StepRecordedA32(merged.registry(),cpu,memory).kind==a32::ExitKind::MissingBlock);
 CHECK(cpu.r==after_return.r && cpu.cpsr==after_return.cpsr);

 // An unreadable stack is a memory fault, never an implicit return through LR.
 cpu.r[15]=pop_pc;cpu.r[13]=stack_page+0x2000U;
 const auto bad_stack=cpu;
 const auto fault=StepRecordedA32(merged.registry(),cpu,memory);
 CHECK(fault.kind==a32::ExitKind::MemoryFault && fault.pc==pop_pc && fault.detail==bad_stack.r[13]);
 CHECK(cpu.r==bad_stack.r && cpu.cpsr==bad_stack.cpsr);

 // Preserve an unsupported loaded target for the next strict execution check;
 // this asserts no Thumb/interworking support and does not align a bad return.
 cpu.r[13]=stack_top-12U;cpu.r[15]=pop_pc;
 CHECK(memory.Write32(stack_top-4U,return_pc|1U));
 const auto odd_return=StepRecordedA32(merged.registry(),cpu,memory);
 CHECK(odd_return.kind==a32::ExitKind::Branch && odd_return.pc==(return_pc|1U));
 CHECK(cpu.r[13]==stack_top && cpu.r[14]==changed_lr && cpu.r[15]==(return_pc|1U));
 const auto unsupported=cpu;
 CHECK(StepRecordedA32(merged.registry(),cpu,memory).kind==a32::ExitKind::Unsupported);
 CHECK(cpu.r==unsupported.r && cpu.cpsr==unsupported.cpsr);
 CHECK(a32::FindBlock(shift_base,entry)==nullptr && a32::FindBlock(shift_base,pop_pc)==nullptr);
}

void LongBlockAndInteriorSuffix(){
 // Authored 96-word block: the first control exit is its final BNE, and its
 // loop starts beyond word 64. Prefix counters expose any truncation or
 // accidental prefix replay when either dispatcher resolves the loop suffix.
 constexpr unsigned word_count=96U, loop_index=90U;
 constexpr auto entry=kShiftPc+0xa00U, loop=entry+loop_index*4U;
 constexpr auto branch_pc=entry+(word_count-1U)*4U, end=entry+word_count*4U;
 static_assert(word_count*4U>256U && word_count*4U<=512U);
 static_assert((entry>>12U)==((end-1U)>>12U));
 std::array<a32::PackedOp,word_count> ops{};
 for(auto& op:ops){
   // ADD r10, r10, #1; the 86 middle prefix words have no control effects.
   op={(14U<<28U)|(1U<<25U)|(4U<<21U)|(10U<<16U)|(10U<<12U)|1U,
       a32::EncodeMetadata(a32::Opcode::Add,a32::Condition::Al,a32::Immediate)};
 }
 for(unsigned rd=0;rd<3U;++rd){
   ops[rd]={(14U<<28U)|(1U<<25U)|(13U<<21U)|(rd<<12U)|(rd==1U?3U:0U),
            a32::EncodeMetadata(a32::Opcode::MovImm,a32::Condition::Al,a32::Immediate)};
 }
 ops[3]={(14U<<28U)|(1U<<25U)|(4U<<21U)|(12U<<16U)|(12U<<12U)|1U,
         a32::EncodeMetadata(a32::Opcode::Add,a32::Condition::Al,a32::Immediate)};
 // Extract byte 0x34, subtract 3*4 and accumulate 40 on each loop visit.
 ops[90]={ShiftWord(0,false,1,8,6,5,4),
          a32::EncodeMetadata(a32::Opcode::CoreAlu,a32::Condition::Al)};
 ops[91]={ShiftWord(2,false,0,2,6,7,6),
          a32::EncodeMetadata(a32::Opcode::CoreAlu,a32::Condition::Al)};
 ops[92]={ShiftWord(4,false,0,0,0,6,0),
          a32::EncodeMetadata(a32::Opcode::Add,a32::Condition::Al)};
 ops[93]={(14U<<28U)|(1U<<25U)|(4U<<21U)|(2U<<16U)|(2U<<12U)|1U,
          a32::EncodeMetadata(a32::Opcode::Add,a32::Condition::Al,a32::Immediate)};
 ops[94]={(14U<<28U)|(1U<<25U)|(2U<<21U)|(1U<<20U)|(1U<<16U)|(1U<<12U)|1U,
          a32::EncodeMetadata(a32::Opcode::Sub,a32::Condition::Al,a32::Immediate|a32::SetFlags)};
 // Architectural PC is word 97; a displacement of -7 words reaches word 90.
 ops[95]={(1U<<28U)|(5U<<25U)|0x00fffff9U,
          a32::EncodeMetadata(a32::Opcode::Branch,a32::Condition::Ne)};
 const a32::Block addition{entry,ops.data(),word_count,false};
 lego::host::SupplementalRegistry merged(shift_base,std::span(&addition,1));
 const auto* recorded=a32::FindBlock(merged.registry(),entry);
 CHECK(recorded && recorded->op_count==word_count);
 CHECK(a32::FindBlock(merged.registry(),loop)==nullptr);
 CHECK(a32::FindBlock(merged.registry(),end)==nullptr);
 GuestMemory memory;
 a32::GuestState initial{};
 for(unsigned i=0;i<initial.r.size();++i)initial.r[i]=0xa0b0c000U+i;
 initial.r[4]=0xffU;initial.r[5]=0x00123400U;initial.r[7]=3U;
 initial.r[10]=9U;initial.r[12]=0x30U;initial.r[15]=entry;
 initial.cpsr=kOtherCpsr|N|V;
 initial.fpscr=0x13579bdfU;initial.vfp.fill(0x5a5a5a5aU);
 initial.thread_pointer=0x0800e000U;
 initial.exclusive_address=0x0800f000U;initial.exclusive_token=0x02468ace13579bdfULL;
 initial.exclusive_size=4;initial.exclusive_valid=true;

 auto first_pass=initial;
 const auto first=a32::ExecuteBlock(addition,first_pass,memory);
 CHECK(first.kind==a32::ExitKind::Branch && first.pc==loop);
 CHECK(first_pass.r[0]==40U && first_pass.r[1]==2U && first_pass.r[2]==1U);
 CHECK(first_pass.r[10]==95U && first_pass.r[12]==0x31U);
 CHECK(first_pass.cpsr==(kOtherCpsr|C));

 auto expected=initial;
 expected.r[0]=120U;expected.r[1]=0U;expected.r[2]=3U;expected.r[6]=40U;
 expected.r[10]=95U;expected.r[12]=0x31U;expected.r[15]=end;
 expected.cpsr=kOtherCpsr|Z|C;
 auto stepped=initial;
 unsigned steps=0, loop_visits=0, taken=0;
 while(stepped.r[15]!=end && steps<128U){
   const auto pc=stepped.r[15];
   if(pc==loop)++loop_visits;
   const auto result=StepRecordedA32(merged.registry(),stepped,memory);++steps;
   if(pc==branch_pc && result.kind==a32::ExitKind::Branch){
     ++taken;CHECK(result.pc==loop && stepped.r[15]==loop);
   }else CHECK(result.kind==a32::ExitKind::Fallthrough && result.pc==pc+4U);
   CHECK(result.fallback==a32::FallbackReason::None && result.detail==0U);
 }
 CHECK(steps==108U && loop_visits==3U && taken==2U);
 CHECK(stepped.r==expected.r && stepped.cpsr==expected.cpsr);
 CHECK(stepped.fpscr==expected.fpscr && stepped.vfp==expected.vfp);
 CHECK(stepped.thread_pointer==expected.thread_pointer);
 CHECK(stepped.exclusive_address==expected.exclusive_address &&
       stepped.exclusive_token==expected.exclusive_token &&
       stepped.exclusive_size==expected.exclusive_size &&
       stepped.exclusive_valid==expected.exclusive_valid);
 const auto stopped=stepped;
 const auto missing=StepRecordedA32(merged.registry(),stepped,memory);
 CHECK(missing.kind==a32::ExitKind::MissingBlock && missing.pc==end);
 CHECK(stepped.r==stopped.r && stepped.cpsr==stopped.cpsr);

 // Full-block dispatch must execute the same two interior suffixes as the
 // single-operation path, then stop exactly beyond the recorded interval.
 auto dispatched=initial;
 const auto completed=a32::Dispatch(merged.registry(),entry,dispatched,memory,nullptr,nullptr,8U);
 CHECK(completed.kind==a32::ExitKind::MissingBlock && completed.pc==end);
 CHECK(completed.fallback==a32::FallbackReason::None && completed.detail==0U);
 CHECK(dispatched.r==expected.r && dispatched.cpsr==expected.cpsr);
 CHECK(dispatched.fpscr==expected.fpscr && dispatched.vfp==expected.vfp);
 CHECK(dispatched.thread_pointer==expected.thread_pointer);
 CHECK(dispatched.exclusive_address==expected.exclusive_address &&
       dispatched.exclusive_token==expected.exclusive_token &&
       dispatched.exclusive_size==expected.exclusive_size &&
       dispatched.exclusive_valid==expected.exclusive_valid);

 // An independently entered interior suffix must never run the prefix's
 // initializers or counters, even when it loops within the large block.
 auto interior=initial;
 interior.r[0]=7U;interior.r[1]=2U;interior.r[2]=5U;
 interior.r[10]=0x111U;interior.r[12]=0x222U;interior.r[15]=loop;
 auto interior_expected=interior;
 interior_expected.r[0]=87U;interior_expected.r[1]=0U;interior_expected.r[2]=7U;
 interior_expected.r[6]=40U;interior_expected.r[15]=end;
 interior_expected.cpsr=kOtherCpsr|Z|C;
 const auto suffix=a32::Dispatch(merged.registry(),loop,interior,memory,nullptr,nullptr,8U);
 CHECK(suffix.kind==a32::ExitKind::MissingBlock && suffix.pc==end);
 CHECK(interior.r==interior_expected.r && interior.cpsr==interior_expected.cpsr);
 CHECK(shift_base.shards[0].blocks==nullptr && shift_base.shards[0].block_count==0U);
}
}
int main(){ValidAndExecution();Invalid();ImmediateShiftSemantics();AndAndSubtractShiftSemantics();TstImmediateSemantics();TstConditionalDispatch();ComputedPcDispatch();InteriorLoopAndStackReturn();LongBlockAndInteriorSuffix();if(failures)return EXIT_FAILURE;std::cout<<"PASS: immutable compile-time supplement, shifted MOV/ADD/ORR/AND/SUB and immediate TST semantics, flag-driven and computed-PC dispatch, large-block and interior-loop suffix execution, memory-loaded return, data/code separation, holes/overlap/range/lifetime, allocation failure, unchanged base and strict missing-PC stop\n";}
