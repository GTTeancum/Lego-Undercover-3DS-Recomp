#include "runtime/ctr_runner.h"
#include "runtime/ctr_recorded_step.h"
#include <cstdlib>
#include <iostream>
using namespace lego::ctr;
namespace {
int failures=0;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}}while(0)
void Equivalence(){
 const a32::PackedOp ops[]{
  {0xE3A00003,a32::EncodeMetadata(a32::Opcode::CoreAlu,a32::Condition::Al)},
  {0xE2500001,a32::EncodeMetadata(a32::Opcode::CoreAlu,a32::Condition::Al)},
  {0x1AFFFFFD,a32::EncodeMetadata(a32::Opcode::Branch,a32::Condition::Ne)},
  {0xE5820000,a32::EncodeMetadata(a32::Opcode::CoreMemory,a32::Condition::Al)},
  {0xEF000032,a32::EncodeMetadata(a32::Opcode::Svc,a32::Condition::Al)}};
 const a32::Block blocks[]{{0x100000,ops,5,false}};
 const a32::BlockShard shard{0x100000,0x100FFC,blocks,1};const a32::Registry reg{&shard,1,nullptr,0};
 GuestMemory a,b;CHECK(a.Map(0x8000000,4,MemoryPermission::Read|MemoryPermission::Write));
 CHECK(b.Map(0x8000000,4,MemoryPermission::Read|MemoryPermission::Write));
 a32::GuestState x{},y{};x.r[2]=y.r[2]=0x8000000;x.cpsr=y.cpsr=0x10;x.r[15]=y.r[15]=0x100000;
 auto expected=a32::Dispatch(reg,x.r[15],x,a,nullptr,nullptr,100);
 a32::ExecutionResult got;unsigned n=0;
 do{got=StepRecordedA32(reg,y,b);++n;}while(n<100&&(got.kind==a32::ExitKind::Fallthrough||got.kind==a32::ExitKind::Branch));
 CHECK(expected.kind==a32::ExitKind::Svc && got.kind==expected.kind && got.pc==expected.pc && got.detail==expected.detail);
 CHECK(x.r==y.r&&x.cpsr==y.cpsr&&x.fpscr==y.fpscr&&x.vfp==y.vfp);CHECK(n==9);
 std::uint32_t av=99,bv=99;CHECK(a.Read32(0x8000000,&av)&&b.Read32(0x8000000,&bv)&&av==bv&&av==0);
 // Every recorded interior word is independently executable; never replay MOV.
 y={};y.r[15]=0x100004;y.r[0]=77;y.cpsr=0x10;CHECK(StepRecordedA32(reg,y,b).kind==a32::ExitKind::Fallthrough&&y.r[0]==76);
 y.r[15]=0x100002;const auto before=y;CHECK(StepRecordedA32(reg,y,b).kind==a32::ExitKind::Unsupported&&y.r==before.r);
 y.r[15]=0x100000;y.cpsr|=32;CHECK(StepRecordedA32(reg,y,b).kind==a32::ExitKind::Unsupported&&y.r[0]==76);
 y.cpsr=0x10;y.r[15]=0x100014;CHECK(StepRecordedA32(reg,y,b).kind==a32::ExitKind::MissingBlock);
}
}
int main(){Equivalence();if(failures)return EXIT_FAILURE;std::cout<<"PASS: one recorded A32 operation, suffix equivalence, conditions, strict missing/Thumb entries\n";}
