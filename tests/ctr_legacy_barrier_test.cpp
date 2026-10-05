#include "runtime/ctr_runner.h"
#include <cstdlib>
#include <iostream>

namespace {
using namespace lego::ctr;
using namespace lego::ctr::a32;
int failures=0;
#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}}while(0)
PackedOp Op(std::uint32_t raw,Opcode code,Condition cond=Condition::Al){return {raw,EncodeMetadata(code,cond)};}
void ExistingFenceRouting(){
    GuestMemory memory;CHECK(memory.Map(0x08000000,4096,MemoryPermission::Read|MemoryPermission::Write));
    CHECK(memory.Write32(0x08000000,0xABCD1234));
    std::uint64_t loaded=0,token=0;std::uint32_t fault=0;
    CHECK(memory.LoadExclusive(0x08000000,4,&loaded,&token,&fault));
    for(auto base:{0xEE070F9AU,0xEE070FBAU})for(unsigned rt=0;rt<15;++rt)for(auto category:{Opcode::CoreAlu,Opcode::CoreSystem}){
        GuestState cpu{};for(unsigned i=0;i<15;++i)cpu.r[i]=0x12340000+i;
        cpu.cpsr=0xB0000010;cpu.fpscr=0x23000010;cpu.vfp[7]=0x0123456789ABCDEFULL;
        cpu.thread_pointer=0x1FF82000;cpu.exclusive_valid=true;cpu.exclusive_address=0x08000000;
        cpu.exclusive_size=4;cpu.exclusive_token=token;auto before=cpu;
        const std::array<PackedOp,2> ops{Op(base|(rt<<12),category),Op(0xEF00007F,Opcode::Svc)};
        const auto end=ExecuteBlock({0x00248404,ops.data(),2},cpu,memory,nullptr,nullptr);
        CHECK(end.kind==ExitKind::Svc && end.pc==0x00248408 && end.detail==0x7F);
        before.r[15]=0x00248408;CHECK(cpu.r==before.r && cpu.cpsr==before.cpsr);
        CHECK(cpu.fpscr==before.fpscr && cpu.vfp==before.vfp && cpu.thread_pointer==before.thread_pointer);
        CHECK(cpu.exclusive_address==before.exclusive_address && cpu.exclusive_size==4 && cpu.exclusive_valid && cpu.exclusive_token==token);
    }
    CHECK(memory.StoreExclusive(0x08000000,4,loaded,token,&fault)==ExclusiveStoreResult::Success);
    for(auto raw:{0xEE07FF9AU,0xFE071F9AU,0xEE071F9BU,0xEE171F9AU,0xEE071E9AU}){
        GuestState cpu{};auto op=Op(raw,Opcode::CoreAlu);
        const auto end=ExecuteBlock({0x100000,&op,1},cpu,memory,nullptr,nullptr);
        CHECK(end.kind==ExitKind::Fallback && end.pc==0x100000 && end.detail==raw);
    }
    for(auto flags:{0U,0x40000000U}){
        GuestState cpu{};cpu.cpsr=flags|0x10;
        const std::array<PackedOp,2> ops{Op(0x0E071F9A,Opcode::CoreAlu,Condition::Eq),Op(0xEF00007F,Opcode::Svc)};
        const auto end=ExecuteBlock({0x100000,ops.data(),2},cpu,memory,nullptr,nullptr);
        CHECK(end.kind==ExitKind::Svc && end.pc==0x100004 && cpu.cpsr==(flags|0x10));
    }
}
void RunnerDoesNotInventCompletion(){
    const std::array<PackedOp,4> ops{
        Op(0xE5840000,Opcode::Str32),Op(0xEE071F9A,Opcode::CoreAlu),
        Op(0xE5942000,Opcode::Ldr32),Op(0xEF00007F,Opcode::Svc)};
    const Block block{0x100000,ops.data(),4};const BlockShard shard{0x100000,0x100FFC,&block,1};
    const Registry registry{&shard,1,nullptr,0};Kernel kernel;GuestMemory memory;NativeRunner runner(registry,memory,kernel);
    CHECK(runner.InitializeMainThread());auto page=std::make_shared<ServiceSharedMemoryObject>();
    CHECK(memory.MapSharedServicePage(0x10000000,page,MemoryPermission::Read|MemoryPermission::Write));
    Handle event=0;CHECK(kernel.CreateEvent(&event,static_cast<std::uint32_t>(ResetType::OneShot))==0);
    const auto handles=kernel.handles().OpenHandleCount(),threads=kernel.threads().size();
    runner.live_state().r[0]=0xC0FFEE00;runner.live_state().r[4]=0x10000800;
    auto end=runner.Run(10,10);
    CHECK(end.reason==RunnerStopReason::UnsupportedSvc && end.exit.pc==0x10000C && end.exit.detail==0x7F);
    CHECK(runner.live_state().r[2]==0xC0FFEE00);std::uint32_t v=0;CHECK(memory.Read32(0x10000800,&v)&&v==0xC0FFEE00);
    CHECK(kernel.now_ns()==0 && kernel.handles().OpenHandleCount()==handles && kernel.threads().size()==threads);
    CHECK(!std::dynamic_pointer_cast<EventObject>(kernel.handles().Get(event))->signaled());
}
}
int main(){ExistingFenceRouting();RunnerDoesNotInventCompletion();if(failures)return EXIT_FAILURE;
std::cout<<"PASS: archived ALU-category DSB/DMB use existing native fences and continue within their block\n";return EXIT_SUCCESS;}
