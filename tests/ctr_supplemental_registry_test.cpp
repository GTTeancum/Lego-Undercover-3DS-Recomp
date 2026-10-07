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
}
int main(){ValidAndExecution();Invalid();if(failures)return EXIT_FAILURE;std::cout<<"PASS: immutable compile-time supplement, holes/overlap/range/lifetime, suffix execution, allocation failure, unchanged base and strict missing-PC stop\n";}
