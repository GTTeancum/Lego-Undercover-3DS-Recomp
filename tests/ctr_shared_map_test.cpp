#include "runtime/ctr_ipc.h"
#include "runtime/ctr_svc_bridge.h"
#include "runtime/ctr_shared_memory.h"
#include <cstdlib>
#include <iostream>

namespace {
using namespace lego::ctr;int failures=0;
#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}}while(0)
constexpr auto rw=MemoryPermission::Read|MemoryPermission::Write;
struct Fixture {
    Kernel kernel;GuestMemory memory;SvcBridge bridge{kernel};a32::GuestState cpu{};
    std::shared_ptr<ServiceSharedMemoryObject> object=std::make_shared<ServiceSharedMemoryObject>();Handle handle{};
    Fixture(){CHECK(kernel.handles().Create(&handle,object)==0);}
    a32::ExecutionResult Map(std::uint32_t base,std::uint32_t perm=3,std::uint32_t other=0x10000000,Handle h=0){
        for(unsigned i=0;i<16;++i)cpu.r[i]=0xAA000000+i;cpu.r[0]=h?h:handle;cpu.r[1]=base;cpu.r[2]=perm;cpu.r[3]=other;cpu.r[15]=0x130910;
        return bridge.Handle({a32::ExitKind::Svc,0x130910,a32::FallbackReason::None,kSvcMapMemoryBlock},cpu,&memory);
    }
};
void ActualMappingAndAliases(){
    Fixture f;constexpr std::uint32_t a=0x10000000,b=0x10001000;
    const auto count=f.kernel.handles().OpenHandleCount(),threads=f.kernel.threads().size();const auto now=f.kernel.now_ns();
    CHECK(f.Map(a).kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==0 && f.cpu.r[15]==0x130914);
    CHECK(f.cpu.r[1]==a && f.cpu.r[2]==3 && f.cpu.r[3]==0x10000000);
    for(unsigned i=4;i<15;++i)CHECK(f.cpu.r[i]==0xAA000000+i);
    CHECK(f.memory.IsReadable(a,0x1000) && f.memory.IsWritable(a,0x1000) && !f.memory.IsMapped(a+0x1000));
    CHECK(count==f.kernel.handles().OpenHandleCount() && threads==f.kernel.threads().size() && now==f.kernel.now_ns());
    CHECK(f.Map(b).kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==0);
    CHECK(f.memory.Write32(a+0x40,0x78563412));std::uint32_t v=0;CHECK(f.memory.Read32(b+0x40,&v) && v==0x78563412);
    CHECK(f.object->bytes()[0x40]==0x12 && f.object->bytes()[0x43]==0x78);
    const std::array<std::uint8_t,4> host{1,2,3,4};CHECK(f.object->Write(0x44,host));CHECK(f.memory.Read32(a+0x44,&v) && v==0x04030201);
    // An alias, a host service write and a load helper must all invalidate the
    // same backing granule's exclusive token; unrelated bytes must not.
    std::uint64_t value=0,token=0;std::uint32_t fault=0;
    CHECK(f.memory.LoadExclusive(a+0x40,4,&value,&token,&fault));CHECK(f.memory.Write32(b+0x40,1));
    CHECK(f.memory.StoreExclusive(a+0x40,4,2,token,&fault)==a32::ExclusiveStoreResult::ReservationLost);
    CHECK(f.memory.LoadExclusive(a+0x40,4,&value,&token,&fault));CHECK(f.object->Write(0x44,host));
    CHECK(f.memory.StoreExclusive(a+0x40,4,2,token,&fault)==a32::ExclusiveStoreResult::ReservationLost);
    CHECK(f.memory.LoadExclusive(a+0x40,4,&value,&token,&fault));CHECK(f.memory.LoadBytes(b+0x40,host));
    CHECK(f.memory.StoreExclusive(a+0x40,4,2,token,&fault)==a32::ExclusiveStoreResult::ReservationLost);
    CHECK(f.memory.LoadExclusive(a+0x40,4,&value,&token,&fault));CHECK(f.memory.Write32(b+0x50,9));
    CHECK(f.memory.StoreExclusive(a+0x40,4,2,token,&fault)==a32::ExclusiveStoreResult::Success);
    CHECK(f.memory.AtomicSwap(b+0x40,4,3,&v,&fault) && v==2);CHECK(f.memory.Read32(a+0x40,&v) && v==3);
    CHECK(f.memory.ZeroBytes(b+0x40,8));CHECK(f.memory.Read64(a+0x40,&value,&fault) && value==0);
    CHECK(f.memory.Write64(b+0xFF8,0x0102030405060708ULL,&fault));CHECK(f.memory.Read64(a+0xFF8,&value,&fault) && value==0x0102030405060708ULL);
    CHECK(!f.memory.Write64(a+0xFFD,1,&fault));
    std::weak_ptr<ServiceSharedMemoryObject> weak=f.object;f.object.reset();CHECK(f.kernel.CloseHandle(f.handle)==0 && !weak.expired());
    CHECK(f.memory.Read32(a+0xFF8,&v) && v==0x05060708); // Mapping owns backing after handles close.
}
void GuardsAndPermissions(){
    Fixture f;constexpr std::uint32_t a=0x10000000;std::uint32_t v=0;
    for(auto perm:{0U,4U,7U,0x10000000U,0xFFFFFFFFU}){
        CHECK(f.Map(a,perm).kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidCombination);CHECK(!f.memory.IsMapped(a));
    }
    CHECK(f.Map(a,3,3).kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidCombination && !f.memory.IsMapped(a));
    for(auto addr:{0x07000000U,0x13FFF000U,0x14000000U,0xFFFFF000U}){
        CHECK(f.Map(addr).kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==0xE0E01BF5U);CHECK(!f.memory.IsMapped(addr));
    }
    for(auto addr:{0U,a+1}){
        CHECK(f.Map(addr).kind==a32::ExitKind::Svc && f.cpu.r[0]==f.handle && f.cpu.r[15]==0x130910);CHECK(!f.memory.IsMapped(addr));
    }
    for(auto h:{kCurrentThreadPseudoHandle,kCurrentProcessPseudoHandle,Handle(0xDEADBEEF)}){
        CHECK(f.Map(a,3,0x10000000,h).kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidHandle);CHECK(!f.memory.IsMapped(a));
    }
    CHECK(f.memory.Map(a+0x800,4,rw));CHECK(f.memory.Write32(a+0x800,0xBAD));
    CHECK(f.Map(a).kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==0xE0A01BF5U);CHECK(f.memory.Read32(a+0x800,&v) && v==0xBAD);
    constexpr std::uint32_t ro=0x11000000,wo=0x11001000;
    CHECK(f.Map(ro,1).kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==0);
    CHECK(f.Map(wo,2).kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==0);
    CHECK(!f.memory.Write32(ro,1) && !f.memory.Read32(wo,&v));
    CHECK(f.memory.Write32(wo,2) && f.memory.Read32(ro,&v) && v==2);
    CHECK(f.Map(ro).kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==0xE0A01BF5U && !f.memory.IsWritable(ro));
    // No shared-object operations may become a generic device/anonymous mapping.
    Handle fake=0;CHECK(f.kernel.handles().Create(&fake,std::make_shared<GenericObject>(KernelObject::Type::SharedMemory))==0);
    CHECK(f.Map(0x12000000,3,0x10000000,fake).kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidHandle);
    CHECK(!f.object->Write(0x1000,std::array<std::uint8_t,1>{1}));
}
void DistinctPagesAndMemoryLifetime(){
    auto one=std::make_shared<ServiceSharedMemoryObject>(),two=std::make_shared<ServiceSharedMemoryObject>();
    std::weak_ptr<ServiceSharedMemoryObject> weak=one;
    {GuestMemory first,second;CHECK(first.MapSharedServicePage(0x10000000,one,rw));CHECK(second.MapSharedServicePage(0x10000000,one,rw));
     CHECK(first.MapSharedServicePage(0x10001000,two,rw));CHECK(first.Write32(0x10000000,123));std::uint32_t v=0;
     CHECK(second.Read32(0x10000000,&v) && v==123);CHECK(first.Read32(0x10001000,&v) && v==0);
     one.reset();CHECK(!weak.expired());}
    CHECK(weak.expired());
}
}
int main(){ActualMappingAndAliases();GuardsAndPermissions();DistinctPagesAndMemoryLifetime();if(failures)return EXIT_FAILURE;std::cout<<"PASS: real shared-page mapping, coherent aliases, permissions and exclusive tokens\n";return EXIT_SUCCESS;}
