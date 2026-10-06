#include "runtime/ctr_memory.h"
#include "runtime/ctr_svc_bridge.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>

// Fail individual metadata allocations without exhausting the host machine.
static long fail_after=-1;
void* operator new(std::size_t n) {
    if (fail_after==0) throw std::bad_alloc();
    if (fail_after>0) --fail_after;
    if (void* p=std::malloc(n?n:1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }

namespace {
using namespace lego::ctr;
constexpr auto RW=MemoryPermission::Read|MemoryPermission::Write;
constexpr std::uint32_t Base=0x08000000, Source=Base+0x45000, Target=0x0E000000, Size=0x8000;
constexpr Result InvalidAddress=0xE0E01BF5, InvalidState=0xE0A01BF5;
int failures=0;
#define CHECK(x) do { if (!(x)) {std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;} } while(0)
struct Fixture {
    GuestMemory memory; Kernel kernel; SvcBridge bridge{kernel};
    std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x60000);
    Fixture() {
        CHECK(memory.Map(Base,bytes.size(),RW));
        for (std::size_t i=0;i<bytes.size();++i) bytes[i]=(i*37+(i>>8)+11)&255;
        CHECK(memory.LoadBytes(Base,bytes));
    }
    std::uint32_t Read(std::uint32_t a) {std::uint32_t v{};CHECK(memory.Read32(a,&v));return v;}
    a32::GuestState Cpu(unsigned permission=3,unsigned op=4) {
        a32::GuestState cpu{};for(unsigned i=0;i<16;++i)cpu.r[i]=0x12340000+i;
        cpu.r[0]=op;cpu.r[1]=Target;cpu.r[2]=Source;cpu.r[3]=Size;cpu.r[4]=permission;
        cpu.r[15]=0x25C9F4;cpu.cpsr=0x20000010;cpu.fpscr=0x23000018;return cpu;
    }
    a32::ExecutionResult Call(a32::GuestState& cpu) {
        return bridge.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcControlMemory},cpu,&memory);
    }
};
void ExactOriginalRequest() {
    Fixture f;auto cpu=f.Cpu(),before=cpu;
    const auto handles=f.kernel.handles().OpenHandleCount(),threads=f.kernel.threads().size();
    Handle resource{};CHECK(f.kernel.GetResourceLimit(&resource,kCurrentProcessPseudoHandle)==0);
    auto obj=std::dynamic_pointer_cast<ResourceLimitObject>(f.kernel.handles().Get(resource));
    CHECK(obj);const auto commit=obj->Current(ResourceLimitType::Commit);
    const auto result=f.Call(cpu);
    CHECK(result.kind==a32::ExitKind::Fallthrough && cpu.r[0]==0 && cpu.r[1]==0 && cpu.r[15]==0x25C9F8);
    for(unsigned i=2;i<15;++i)CHECK(cpu.r[i]==before.r[i]);
    CHECK(cpu.cpsr==before.cpsr && cpu.fpscr==before.fpscr && cpu.vfp==before.vfp);
    CHECK(f.kernel.now_ns()==0 && f.kernel.threads().size()==threads && f.kernel.handles().OpenHandleCount()==handles+1);
    CHECK(obj->Current(ResourceLimitType::Commit)==commit && !f.kernel.current_thread()->pending_wake);
    CHECK(f.memory.user_aliases().size()==1);
    CHECK(f.memory.private_state(Source-1)==GuestMemory::PrivateState::Private);
    CHECK(f.memory.private_state(Source)==GuestMemory::PrivateState::Aliased);
    CHECK(f.memory.private_state(Source+Size-1)==GuestMemory::PrivateState::Aliased);
    CHECK(f.memory.private_state(Source+Size)==GuestMemory::PrivateState::Private);
    CHECK(f.memory.private_state(Target)==GuestMemory::PrivateState::Alias);
    CHECK(f.memory.IsReadable(Base,0x60000) && f.memory.IsWritable(Source,Size));
    for(unsigned i=0;i<Size;++i) {std::uint8_t a{},b{};CHECK(f.memory.Read8(Source+i,&a) && f.memory.Read8(Target+i,&b));CHECK(a==b && a==f.bytes[Source-Base+i]);}
    CHECK(f.memory.Write32(Target,0xAABBCCDD) && f.Read(Source)==0xAABBCCDD);
    CHECK(f.memory.Write16(Source+0x1000,0x7654));std::uint16_t v{};CHECK(f.memory.Read16(Target+0x1000,&v) && v==0x7654);
    std::uint32_t fault{};std::uint64_t d{};CHECK(f.memory.Write64(Target+17,0xFFEEDDCCBBAA9988ULL,&fault));
    CHECK(f.memory.Read64(Source+17,&d,&fault) && d==0xFFEEDDCCBBAA9988ULL);
    CHECK(f.memory.Write8(Source+Size-1,0xE7));std::uint8_t byte{};CHECK(f.memory.Read8(Target+Size-1,&byte) && byte==0xE7);
    CHECK(f.memory.SpansAlias(Source,Size,Target,Size));
    CHECK(f.memory.SpansAlias(Target+5,4,Source+8,3));
    CHECK(!f.memory.SpansAlias(Target,4,Source+4,4));
    CHECK(!f.memory.SpansAlias(Target,Size,Source-Size,Size));
    CHECK(!f.memory.SpansAlias(Target,0,Source,Size));
    CHECK(!f.memory.SpansAlias(Target+Size,1,Source,1));
    // Metadata growth cannot detach or invalidate the owned backing.
    for(unsigned i=0;i<150;++i)CHECK(f.memory.Map(0x20000000+i*0x1000,0x1000,RW));
    CHECK(f.Read(Target)==f.Read(Source));
    CHECK(f.memory.MapUserAlias(Target+0x10000,Source+Size,0x1000,3)==0);
    CHECK(!f.memory.SpansAlias(Target+0x10000,0x1000,Target,Size));
    CHECK(f.memory.Write32(Target+0x10000,0xDEADBEEF) && f.Read(Source+Size)==0xDEADBEEF);
}
void PermissionsAndErrors() {
    for(unsigned perm=0;perm<4;++perm) {
        Fixture f;CHECK(f.memory.MapUserAlias(Target,Source,Size,perm)==0);
        CHECK(f.memory.IsMapped(Target,Size));
        CHECK(f.memory.IsReadable(Target)==bool(perm&1));CHECK(f.memory.IsWritable(Target)==bool(perm&2));
        CHECK(f.memory.IsReadable(Source,Size) && f.memory.IsWritable(Source,Size));
        std::uint32_t v{};CHECK(f.memory.Read32(Target,&v)==bool(perm&1));CHECK(f.memory.Write32(Target,123)==bool(perm&2));
        CHECK(f.memory.MapUserAlias(Target+0x10000,Source,Size,3)==InvalidState);
        CHECK(f.memory.MapUserAlias(Target+0x20000,Target,Size,3)==InvalidState);
        CHECK(f.memory.MapUserAlias(Target,Source+Size,0x1000,3)==InvalidState);
    }
    Fixture f;
    CHECK(f.memory.MapUserAlias(Target+1,Source,Size,3)==kResultMisalignedAddress);
    CHECK(f.memory.MapUserAlias(Target,Source+1,Size,3)==kResultMisalignedAddress);
    CHECK(f.memory.MapUserAlias(Target,Source,Size+1,3)==kResultMisalignedSize);
    CHECK(f.memory.MapUserAlias(Target,Source,Size,4)==kResultInvalidCombination);
    CHECK(f.memory.MapUserAlias(Target,0x07000000,Size,3)==InvalidAddress);
    CHECK(f.memory.MapUserAlias(Target,0x0FFFF000,Size,3)==InvalidAddress);
    CHECK(f.memory.MapUserAlias(Source+0x1000,Source,Size,3)==InvalidState);
    CHECK(f.memory.MapUserAlias(Target,Base+0x60000,Size,3)==InvalidState);
    CHECK(!f.memory.MapUserAlias(Target,Source,0,3));
    CHECK(!f.memory.MapUserAlias(Target,Source,0x101000,3));
    CHECK(!f.memory.MapUserAlias(0xFFFFFFF0&~0xFFFU,Source,Size,3));
    CHECK(f.memory.user_aliases().empty() && !f.memory.IsMapped(Target));
    GuestMemory ro;CHECK(ro.Map(Source,Size,MemoryPermission::Read));CHECK(ro.MapUserAlias(Target,Source,Size,3)==InvalidState);
    GuestMemory shared;CHECK(shared.MapSharedServicePage(Source,std::make_shared<ServiceSharedMemoryObject>(),RW));
    CHECK(shared.MapUserAlias(Target,Source,0x1000,3)==InvalidState);
    GuestMemory split;CHECK(split.Map(Source,0x1000,RW) && split.Map(Source+0x1000,0x1000,RW));
    CHECK(!split.MapUserAlias(Target,Source,0x2000,3));CHECK(!split.IsMapped(Target));
    CHECK(split.MapUserAlias(Target,Source,0x3000,3)==InvalidState);
    GuestMemory rwx;CHECK(rwx.Map(Source,Size,RW|MemoryPermission::Execute));CHECK(!rwx.MapUserAlias(Target,Source,Size,3));
    for(unsigned op:{0x104U,0x10004U,5U,6U,1U}) {
        auto cpu=f.Cpu(3,op),before=cpu;CHECK(f.Call(cpu).kind==a32::ExitKind::Svc);CHECK(cpu.r==before.r);
    }
    auto cpu=f.Cpu();cpu.r[2]++;CHECK(f.Call(cpu).kind==a32::ExitKind::Fallthrough && cpu.r[0]==kResultMisalignedAddress && cpu.r[1]==0);
}
void ReservationsAndDeviceWrites() {
    Fixture f;std::uint64_t value{},token{};std::uint32_t fault{};
    CHECK(f.memory.Write32(Source,0xAA55AA55));
    CHECK(f.memory.LoadExclusive(Source,4,&value,&token,&fault));
    CHECK(f.memory.MapUserAlias(Target,Source,Size,3)==0);
    std::uint64_t alias_token{};CHECK(f.memory.LoadExclusive(Target,4,&value,&alias_token,&fault) && alias_token==token);
    // Creating metadata alone does not manufacture a write.
    CHECK(f.memory.StoreExclusive(Target,4,77,token,&fault)==a32::ExclusiveStoreResult::Success);
    CHECK(f.Read(Source)==77);
    CHECK(f.memory.StoreExclusive(Source,4,1,token,&fault)==a32::ExclusiveStoreResult::ReservationLost);
    CHECK(f.memory.LoadExclusive(Source+16,8,&value,&token,&fault));
    CHECK(f.memory.Write8(Target+17,2));
    CHECK(f.memory.StoreExclusive(Source+16,8,3,token,&fault)==a32::ExclusiveStoreResult::ReservationLost);
    CHECK(f.memory.LoadExclusive(Target+32,4,&value,&token,&fault));
    CHECK(f.memory.Write32(Source+40,4));
    CHECK(f.memory.StoreExclusive(Target+32,4,5,token,&fault)==a32::ExclusiveStoreResult::Success);
    std::uint32_t old{};CHECK(f.memory.AtomicSwap(Target+64,4,0xCAFEBABE,&old,&fault));CHECK(f.Read(Source+64)==0xCAFEBABE);
    const std::array<std::uint8_t,16> data{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    CHECK(!f.memory.CommitDeviceWrite(Target+0x2000,data));
    CHECK(f.memory.LoadExclusive(Source+0x2000,4,&value,&token,&fault));
    CHECK(f.memory.PrepareDeviceWrite(Source+0x2000,data.size()));
    CHECK(f.memory.LoadExclusive(Target+0x2000,4,&value,&alias_token,&fault) && alias_token==token);
    CHECK(f.memory.CommitDeviceWrite(Target+0x2000,data));
    CHECK(f.Read(Source+0x2000)==0x04030201);
    CHECK(f.memory.StoreExclusive(Source+0x2000,4,0,token,&fault)==a32::ExclusiveStoreResult::ReservationLost);
    CHECK(f.memory.PrepareDeviceWrite(Target+0x2010,data.size()) && f.memory.CommitDeviceWrite(Source+0x2010,data));
    CHECK(f.Read(Target+0x2010)==0x04030201);
    // Loader/host initialization still bypasses guest permissions, with shared bytes.
    CHECK(f.memory.LoadBytes(Target+0x3000,data));CHECK(f.Read(Source+0x3000)==0x04030201);
    CHECK(f.memory.ZeroBytes(Source+0x3000,16));CHECK(f.Read(Target+0x3000)==0);
}
void OriginalSourceProtection() {
    Fixture f;auto cpu=f.Cpu();CHECK(f.Call(cpu).kind==a32::ExitKind::Fallthrough);
    std::uint64_t value{},token{},after_token{};std::uint32_t fault{};
    CHECK(f.memory.LoadExclusive(Target,4,&value,&token,&fault));
    // Original second SVC: op=Protect, original source, same size, permissions=None.
    cpu=f.Cpu(0,6);cpu.r[1]=Source;cpu.r[2]=0;const auto before=cpu;
    CHECK(f.Call(cpu).kind==a32::ExitKind::Fallthrough && cpu.r[0]==0 && cpu.r[1]==0 && cpu.r[15]==0x25C9F8);
    for(unsigned i=2;i<15;++i)CHECK(cpu.r[i]==before.r[i]);
    CHECK(f.memory.IsMapped(Source,Size) && !f.memory.IsReadable(Source) && !f.memory.IsWritable(Source));
    CHECK(f.memory.IsReadable(Target,Size) && f.memory.IsWritable(Target,Size));
    CHECK(f.memory.private_state(Source)==GuestMemory::PrivateState::Aliased);
    CHECK(f.memory.private_state(Target)==GuestMemory::PrivateState::Alias);
    CHECK(f.memory.IsReadable(Source-1) && f.memory.IsWritable(Source+Size));
    CHECK(!f.memory.IsReadable(Source-4,8));CHECK(!f.memory.IsWritable(Source+Size-4,8));
    std::uint8_t b{};std::uint16_t h{};std::uint32_t w{};std::uint64_t d{};
    CHECK(!f.memory.Read8(Source,&b) && !f.memory.Read16(Source,&h) && !f.memory.Read32(Source,&w));
    CHECK(!f.memory.Read64(Source,&d,&fault) && fault==Source);
    CHECK(!f.memory.Write8(Source,1) && !f.memory.Write16(Source,2) && !f.memory.Write32(Source,3));
    CHECK(!f.memory.Write64(Source,4,&fault) && !f.memory.AtomicSwap(Source,4,5,&w,&fault));
    CHECK(!f.memory.LoadExclusive(Source,4,&d,&after_token,&fault));
    CHECK(f.memory.StoreExclusive(Source,4,0,token,&fault)==a32::ExclusiveStoreResult::MemoryFault);
    CHECK(!f.memory.PrepareDeviceWrite(Source,32));
    const std::array<std::uint8_t,4> replacement{9,8,7,6};CHECK(!f.memory.CommitDeviceWrite(Source,replacement));
    CHECK(f.memory.LoadExclusive(Target,4,&d,&after_token,&fault) && after_token==token);
    CHECK(f.memory.PrepareDeviceWrite(Target,4) && f.memory.CommitDeviceWrite(Target,replacement));
    CHECK(f.Read(Target)==0x06070809);
    // Reprotecting this exact source restores access to the same bytes, not a copy.
    for(unsigned permission=0;permission<4;++permission) {
        CHECK(f.memory.ProtectUserAlias(Source,Size,permission)==0);
        CHECK(f.memory.IsReadable(Source)==bool(permission&1));CHECK(f.memory.IsWritable(Source)==bool(permission&2));
        CHECK(f.memory.IsReadable(Target) && f.memory.IsWritable(Target));
    }
    CHECK(f.Read(Source)==f.Read(Target));
    CHECK(f.memory.ProtectUserAlias(Target,Size,1)==0 && !f.memory.IsWritable(Target));
    CHECK(f.memory.IsWritable(Source));CHECK(f.memory.Write32(Source,0x99887766));CHECK(f.Read(Target)==0x99887766);
    CHECK(f.memory.ProtectUserAlias(Target,Size,3)==0);
    CHECK(!f.memory.ProtectUserAlias(Source+0x1000,Size-0x1000,0));
    CHECK(!f.memory.ProtectUserAlias(Source,0,0));
    CHECK(f.memory.ProtectUserAlias(Source+1,Size,0)==kResultMisalignedAddress);
    CHECK(f.memory.ProtectUserAlias(Source,Size+1,0)==kResultMisalignedSize);
    CHECK(f.memory.ProtectUserAlias(Source,Size,4)==kResultInvalidCombination);
    CHECK(f.memory.IsWritable(Source) && f.memory.IsWritable(Target));
    // Protection changes no bytes/metadata allocations and works under allocator failure.
    fail_after=0;const auto result=f.memory.ProtectUserAlias(Source,Size,0);fail_after=-1;
    CHECK(result==0 && f.memory.IsReadable(Target) && !f.memory.IsReadable(Source));
    CHECK(f.kernel.now_ns()==0 && !f.kernel.current_thread()->pending_wake);
}
void AllocationRollbackAndCapacity() {
    unsigned failures_seen=0, successes=0;
    for(long i=0;i<9;++i) {
        Fixture f;std::uint64_t value{},before_token{},after_token{};std::uint32_t fault{};
        CHECK(f.memory.Write32(Source,0x11223344));CHECK(f.memory.LoadExclusive(Source,4,&value,&before_token,&fault));
        fail_after=i;const auto result=f.memory.MapUserAlias(Target,Source,Size,3);fail_after=-1;
        CHECK(f.Read(Source)==0x11223344);
        CHECK(f.memory.LoadExclusive(Source,4,&value,&after_token,&fault) && after_token==before_token);
        if (!result) {
            ++failures_seen;CHECK(f.memory.user_aliases().empty() && !f.memory.IsMapped(Target));
            CHECK(f.memory.private_state(Source)==GuestMemory::PrivateState::Private);
            CHECK(f.memory.MapUserAlias(Target,Source,Size,3)==0); // Failed attempt leaves no residue.
        } else {++successes;CHECK(*result==0 && f.Read(Target)==0x11223344);}
    }
    CHECK(failures_seen>=2 && successes>0);
    Fixture f;auto cpu=f.Cpu(),old=cpu;
    fail_after=0;const auto stop=f.Call(cpu);fail_after=-1;
    CHECK(stop.kind==a32::ExitKind::Svc && cpu.r==old.r && !f.memory.IsMapped(Target));
    GuestMemory m;CHECK(m.Map(Base,0x100000,RW));
    for(unsigned i=0;i<GuestMemory::kMaxUserAliases;++i)CHECK(m.MapUserAlias(Target+i*0x1000,Base+i*0x1000,0x1000,3)==0);
    CHECK(!m.MapUserAlias(Target+0x80000,Base+0x80000,0x1000,3));
    CHECK(m.user_aliases().size()==GuestMemory::kMaxUserAliases && !m.IsMapped(Target+0x80000));
}
}
int main() {
    ExactOriginalRequest();PermissionsAndErrors();ReservationsAndDeviceWrites();OriginalSourceProtection();AllocationRollbackAndCapacity();
    if(failures)return EXIT_FAILURE;
    std::cout<<"PASS: real private aliases, independent permissions, physical reservation identity and transactional metadata\n";
    return EXIT_SUCCESS;
}
