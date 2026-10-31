// Authored A32 fixtures only: no game words, addresses, captures or assets.
#include "host/supplemental_registry.h"
#include "runtime/ctr_memory.h"
#include "runtime/ctr_recorded_step.h"
#include <algorithm>
#include <array>
#include <iterator>
#include <cstdlib>
#include <iostream>

namespace {
using namespace lego::ctr;
int failures{};
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL " << __LINE__ << ": " #x "\n"; ++failures; } } while (0)
constexpr std::uint32_t page = 0x00801000U, entry = page - 16U, loop = entry + 8U;
constexpr std::uint32_t data = 0x08010000U, stack = 0x08012000U, sp = stack + 0x800U;
constexpr std::uint32_t returned = 0x00b00000U, flags = 0x08050010U;
constexpr std::uint32_t al = 14U << 28U;
constexpr std::uint32_t Imm(unsigned opcode, unsigned rn, unsigned rd,
                            unsigned value, bool set_flags = false) {
    return al | (1U << 25U) | (opcode << 21U) | (unsigned(set_flags) << 20U) |
           (rn << 16U) | (rd << 12U) | value;
}
const a32::PackedOp first_ops[]{
    {Imm(4,10,10,1), a32::EncodeMetadata(a32::Opcode::Add,a32::Condition::Al,a32::Immediate)},
    {Imm(13,0,1,3), a32::EncodeMetadata(a32::Opcode::MovImm,a32::Condition::Al,a32::Immediate)},
    {Imm(4,2,2,1), a32::EncodeMetadata(a32::Opcode::Add,a32::Condition::Al,a32::Immediate)},
    // Architectural PC at the last word before the boundary is page+4.
    {Imm(4,15,3,0), a32::EncodeMetadata(a32::Opcode::Add,a32::Condition::Al,a32::Immediate)}};
const a32::PackedOp second_ops[]{
    {al | (1U<<26U) | (1U<<24U) | (1U<<23U) | (2U<<12U),
     a32::EncodeMetadata(a32::Opcode::Str32,a32::Condition::Al)},
    {Imm(2,1,1,1,true), a32::EncodeMetadata(a32::Opcode::Sub,a32::Condition::Al,a32::Immediate|a32::SetFlags)},
    // BNE page+8, with architectural PC page+16: -6 words reaches loop.
    {(1U<<28U) | (5U<<25U) | 0x00fffffaU,
     a32::EncodeMetadata(a32::Opcode::Branch,a32::Condition::Ne)}};
const a32::PackedOp return_op{
    // LDR pc, [sp], #4. This is an ordinary stack-loaded return, not live LR.
    al | (1U<<26U) | (1U<<23U) | (1U<<20U) | (13U<<16U) | (15U<<12U) | 4U,
    a32::EncodeMetadata(a32::Opcode::CoreMemory,a32::Condition::Al)};
const a32::Block parts[]{{entry,first_ops,4,false},{page,second_ops,3,false},
                         {page+12U,&return_op,1,false}};
const a32::BlockShard shards[]{{page-0x1000U,page-4U,nullptr,0},
                               {page,page+0xffcU,nullptr,0}};
const a32::Registry base{shards,2,nullptr,0};

a32::GuestState Initial() {
    a32::GuestState s{};
    for (unsigned i=0;i<s.r.size();++i) s.r[i]=0x10020000U+i;
    s.r[0]=data; s.r[2]=0; s.r[10]=50; s.r[13]=sp;
    s.r[14]=0x00c00000U; s.r[15]=entry; s.cpsr=flags|a32::kFlagV;
    s.vfp.fill(0xa5c33c5aU); s.fpscr=0x12345678U; s.thread_pointer=0x08014000U;
    s.exclusive_address=data+32U; s.exclusive_token=0x123456789abcdef0ULL;
    s.exclusive_size=4; s.exclusive_valid=true;
    return s;
}
void Same(const a32::GuestState& s,const a32::GuestState& e) {
    CHECK(s.r==e.r && s.cpsr==e.cpsr);
    CHECK(s.vfp==e.vfp && s.fpscr==e.fpscr && s.thread_pointer==e.thread_pointer);
    CHECK(s.exclusive_address==e.exclusive_address && s.exclusive_token==e.exclusive_token);
    CHECK(s.exclusive_size==e.exclusive_size && s.exclusive_valid==e.exclusive_valid);
}
void Map(GuestMemory& m,bool map_data=true) {
    if(map_data) { CHECK(m.Map(data,4096,MemoryPermission::Read|MemoryPermission::Write)); CHECK(m.Write32(data,0xdeadbeefU)); }
    CHECK(m.Map(stack,4096,MemoryPermission::Read|MemoryPermission::Write));
    CHECK(m.Write32(sp,returned));
}
void BoundaryAndLoop() {
    lego::host::SupplementalRegistry merged(base,parts);
    GuestMemory m; Map(m);
    auto head=Initial(), expected=head;
    const auto next=a32::ExecuteBlock(parts[0],head,m);
    expected.r[10]=51; expected.r[1]=3; expected.r[2]=1; expected.r[3]=page+4U;
    expected.r[15]=page;
    CHECK(next.kind==a32::ExitKind::Fallthrough && next.pc==page);
    Same(head,expected); // No synthetic branch or flag change at the page edge.
    CHECK(merged.registry().shards[0].block_count==1);
    CHECK(merged.registry().shards[1].block_count==2);
    CHECK(a32::FindBlock(merged.registry(),loop)==nullptr); // Suffix entry, not exact.
    auto stepped=Initial(); unsigned operations=0, boundary_crossings=0, backward_branches=0;
    while(operations<32U && stepped.r[15]!=returned) {
        const auto before=stepped.r[15];
        const auto r=StepRecordedA32(merged.registry(),stepped,m); ++operations;
        CHECK(r.fallback==a32::FallbackReason::None && r.detail==0);
        if(before==page-4U) { ++boundary_crossings; CHECK(r.kind==a32::ExitKind::Fallthrough && r.pc==page); }
        else if(before==page+8U && r.kind==a32::ExitKind::Branch) { ++backward_branches; CHECK(r.pc==loop); }
        else if(before==page+12U) CHECK(r.kind==a32::ExitKind::Branch && r.pc==returned);
        else CHECK(r.kind==a32::ExitKind::Fallthrough && r.pc==before+4U);
    }
    CHECK(operations==18U && boundary_crossings==3U && backward_branches==2U);
    expected=Initial(); expected.r[10]=51; expected.r[1]=0; expected.r[2]=3;
    expected.r[3]=page+4U; expected.r[13]=sp+4U; expected.r[15]=returned;
    expected.cpsr=flags|a32::kFlagZ|a32::kFlagC;
    Same(stepped,expected);
    const auto stopped=stepped;
    CHECK(StepRecordedA32(merged.registry(),stepped,m).kind==a32::ExitKind::MissingBlock);
    Same(stepped,stopped);
    std::uint32_t value{}; CHECK(m.Read32(data,&value) && value==3);
    auto dispatched=Initial();
    const auto r=a32::Dispatch(merged.registry(),entry,dispatched,m,nullptr,nullptr,32);
    CHECK(r.kind==a32::ExitKind::MissingBlock && r.pc==returned);
    Same(dispatched,expected);
    CHECK(base.shards[0].blocks==nullptr && base.shards[1].blocks==nullptr);
}
void DirectSuffixAndTail() {
    lego::host::SupplementalRegistry merged(base,parts);
    for(bool tail:{false,true}) {
        GuestMemory m; Map(m); auto s=Initial();
        s.r[15]=tail?page:loop; s.r[1]=tail?1U:2U; s.r[2]=tail?9U:4U;
        auto e=s; e.r[1]=0; e.r[2]=tail?9U:6U;
        if(!tail)e.r[3]=page+4U;
        e.r[13]=sp+4U; e.r[15]=returned; e.cpsr=flags|a32::kFlagZ|a32::kFlagC;
        const auto r=a32::Dispatch(merged.registry(),s.r[15],s,m,nullptr,nullptr,32);
        CHECK(r.kind==a32::ExitKind::MissingBlock && r.pc==returned); Same(s,e);
        std::uint32_t value{}; CHECK(m.Read32(data,&value) && value==(tail?9U:6U));
    }
}
void MissingTailAndFault() {
    GuestMemory m; Map(m);
    lego::host::SupplementalRegistry missing(base,std::span(parts,1));
    auto s=Initial(),e=s; e.r[10]=51; e.r[1]=3; e.r[2]=1; e.r[3]=page+4U; e.r[15]=page;
    const auto r=a32::Dispatch(missing.registry(),entry,s,m,nullptr,nullptr,32);
    CHECK(r.kind==a32::ExitKind::MissingBlock && r.pc==page); Same(s,e);
    std::uint32_t value{}; CHECK(m.Read32(data,&value) && value==0xdeadbeefU);
    lego::host::SupplementalRegistry merged(base,parts);
    for(bool stepped:{false,true}) {
        GuestMemory absent; Map(absent,false); s=Initial();
        a32::ExecutionResult fault{};
        if(stepped) {
            for(unsigned i=0;i<4;++i)CHECK(StepRecordedA32(merged.registry(),s,absent).kind==a32::ExitKind::Fallthrough);
            fault=StepRecordedA32(merged.registry(),s,absent);
        } else fault=a32::Dispatch(merged.registry(),entry,s,absent,nullptr,nullptr,32);
        CHECK(fault.kind==a32::ExitKind::MemoryFault && fault.pc==page && fault.detail==data);
        Same(s,e); // Earlier effects survive; no tail instruction or return is invented.
    }
    // The registry itself still refuses a physical block spanning two shards.
    std::array<a32::PackedOp,7> unsplit{};
    std::copy(std::begin(first_ops),std::end(first_ops),unsplit.begin());
    std::copy(std::begin(second_ops),std::end(second_ops),unsplit.begin()+4);
    const a32::Block bad{entry,unsplit.data(),7,false}; bool threw=false;
    try { lego::host::SupplementalRegistry rejected(base,std::span(&bad,1)); }
    catch(const std::invalid_argument&) { threw=true; }
    CHECK(threw);
}
void JoinUnchangedOriginalBlock() {
    // An authored same-page missing prefix joins an original loop. The join
    // emits no branch, replaces no original operation and replays no setup.
    constexpr auto start=page+0x400U, join=start+8U, done=join+12U;
    const a32::PackedOp prefix_ops[]{
        {Imm(4,10,10,1),a32::EncodeMetadata(a32::Opcode::Add,a32::Condition::Al,a32::Immediate)},
        {Imm(13,0,1,3),a32::EncodeMetadata(a32::Opcode::MovImm,a32::Condition::Al,a32::Immediate)}};
    const a32::PackedOp loop_ops[]{
        {Imm(4,2,2,1),a32::EncodeMetadata(a32::Opcode::Add,a32::Condition::Al,a32::Immediate)},
        {Imm(2,1,1,1,true),a32::EncodeMetadata(a32::Opcode::Sub,a32::Condition::Al,a32::Immediate|a32::SetFlags)},
        {(1U<<28U)|(5U<<25U)|0x00fffffcU,a32::EncodeMetadata(a32::Opcode::Branch,a32::Condition::Ne)}};
    const a32::PackedOp ret{al|(13U<<21U)|(15U<<12U)|14U,
                           a32::EncodeMetadata(a32::Opcode::MovReg,a32::Condition::Al)};
    const a32::Block original_blocks[]{{join,loop_ops,3,false},{done,&ret,1,false}};
    const a32::BlockShard original_shard{page,page+0xffcU,original_blocks,2};
    const a32::Registry original{&original_shard,1,nullptr,0};
    const a32::Block prefix{start,prefix_ops,2,false};
    lego::host::SupplementalRegistry merged(original,std::span(&prefix,1));
    CHECK(a32::FindBlock(original,start)==nullptr);
    CHECK(a32::FindBlock(merged.registry(),join)->ops==loop_ops);
    CHECK(a32::FindBlock(merged.registry(),done)->ops==&ret);
    auto initial=Initial();initial.r[14]=returned;initial.r[15]=start;
    GuestMemory memory;Map(memory);
    auto head=initial,head_expected=initial;
    head_expected.r[10]=51;head_expected.r[1]=3;head_expected.r[15]=join;
    const auto fell=a32::ExecuteBlock(prefix,head,memory);
    CHECK(fell.kind==a32::ExitKind::Fallthrough && fell.pc==join);
    Same(head,head_expected);
    auto expected=initial;expected.r[10]=51;expected.r[1]=0;
    expected.r[2]=3;expected.r[15]=returned;expected.cpsr=flags|a32::kFlagZ|a32::kFlagC;
    auto stepped=initial;unsigned count=0,joins=0;
    while(count<20 && stepped.r[15]!=returned){
        const auto pc=stepped.r[15];
        const auto r=StepRecordedA32(merged.registry(),stepped,memory);++count;
        CHECK(r.fallback==a32::FallbackReason::None && r.detail==0);
        if(pc==start+4U){++joins;CHECK(r.kind==a32::ExitKind::Fallthrough && r.pc==join);}
        else CHECK(r.kind==a32::ExitKind::Fallthrough || r.kind==a32::ExitKind::Branch);
    }
    CHECK(count==12 && joins==1);Same(stepped,expected);
    auto dispatched=initial;
    const auto stopped=a32::Dispatch(merged.registry(),start,dispatched,memory,nullptr,nullptr,16);
    CHECK(stopped.kind==a32::ExitKind::MissingBlock && stopped.pc==returned);
    Same(dispatched,expected);
    auto tail=initial;tail.r[15]=join;tail.r[1]=2;tail.r[2]=4;tail.r[10]=99;
    auto tail_expected=tail;tail_expected.r[1]=0;tail_expected.r[2]=6;
    tail_expected.r[15]=returned;tail_expected.cpsr=flags|a32::kFlagZ|a32::kFlagC;
    CHECK(a32::Dispatch(merged.registry(),join,tail,memory,nullptr,nullptr,16).kind==a32::ExitKind::MissingBlock);
    Same(tail,tail_expected);
    CHECK(original.shards[0].blocks==original_blocks && original.shards[0].block_count==2);

    // A missing original target stays missing; metadata cannot decode a tail.
    const a32::BlockShard empty_shard{page,page+0xffcU,nullptr,0};
    const a32::Registry empty{&empty_shard,1,nullptr,0};
    lego::host::SupplementalRegistry missing(empty,std::span(&prefix,1));
    auto absent=initial;
    const auto no_tail=a32::Dispatch(missing.registry(),start,absent,memory,nullptr,nullptr,16);
    CHECK(no_tail.kind==a32::ExitKind::MissingBlock && no_tail.pc==join);
    Same(absent,head_expected);

    // An original tail's failed memory access still faults after prefix effects.
    const a32::PackedOp load{al|(1U<<26U)|(1U<<24U)|(1U<<23U)|(1U<<20U)|(3U<<12U),
                            a32::EncodeMetadata(a32::Opcode::Ldr32,a32::Condition::Al)};
    const a32::Block fault_block{join,&load,1,false};
    const a32::BlockShard fault_shard{page,page+0xffcU,&fault_block,1};
    const a32::Registry fault_base{&fault_shard,1,nullptr,0};
    lego::host::SupplementalRegistry fault_registry(fault_base,std::span(&prefix,1));
    GuestMemory unmapped;auto faulted=initial;
    const auto fault=a32::Dispatch(fault_registry.registry(),start,faulted,unmapped,nullptr,nullptr,16);
    CHECK(fault.kind==a32::ExitKind::MemoryFault && fault.pc==join && fault.detail==data);
    Same(faulted,head_expected);
}

}
int main() {
    BoundaryAndLoop(); DirectSuffixAndTail(); MissingTailAndFault(); JoinUnchangedOriginalBlock();
    if(failures)return EXIT_FAILURE;
    std::cout << "PASS: page-edge fallthrough, PC+8, cross-page loop suffix, direct tail, flags, stack return, original-block join, strict missing tail and fault effects\n";
}
