#include "host/supplemental_registry.h"
#include "runtime/ctr_recorded_step.h"
#include "runtime/ctr_memory.h"
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
}
int main(){ValidAndExecution();Invalid();ImmediateShiftSemantics();ComputedPcDispatch();InteriorLoopAndStackReturn();if(failures)return EXIT_FAILURE;std::cout<<"PASS: immutable compile-time supplement, shifted MOV/ADD/ORR semantics, computed-PC dispatch, interior loop and memory-loaded return, data/code separation, holes/overlap/range/lifetime, suffix execution, allocation failure, unchanged base and strict missing-PC stop\n";}
