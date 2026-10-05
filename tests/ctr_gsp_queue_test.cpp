#include "services/gsp_gpu_service.h"
#include "runtime/ctr_svc_bridge.h"
#include <cstdlib>
#include <iostream>

namespace {
using namespace lego::ctr;
constexpr auto RW = MemoryPermission::Read | MemoryPermission::Write;
constexpr std::uint32_t kShared = 0x10000000, kData = 0x14000000;
constexpr std::uint32_t kQueue = 0x800;
int failures = 0;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL " << __LINE__ << ": " #x "\n"; ++failures; } } while (0)
struct Fixture {
    Kernel kernel; GuestMemory memory; IpcRouter ipc; SvcBridge bridge{kernel,&ipc};
    std::shared_ptr<GspGpuService> endpoint = std::make_shared<GspGpuService>();
    Handle session{}, event{}; a32::GuestState cpu{};
    explicit Fixture(bool acquire = true) {
        CHECK(memory.EnsureTlsMappings(kernel));
        CHECK(ipc.RegisterService("gsp::Gpu",endpoint) == 0); Connect();
        CHECK(kernel.CreateEvent(&event,0) == 0);
        Put({0x00130042,1,0,event}); CHECK(Call().kind == a32::ExitKind::Fallthrough);
        if (acquire) {
            Put({0x00160042,0,0,kCurrentProcessPseudoHandle});
            CHECK(Call().kind == a32::ExitKind::Fallthrough && cpu.r[0] == 0);
        }
        CHECK(memory.MapSharedServicePage(kShared,endpoint->shared_memory(),RW));
        CHECK(memory.Map(kData,0x10000,RW));
        for (unsigned i=0;i<0x10000;i+=4) CHECK(memory.Write32(kData+i,0x12340000+i));
    }
    void Connect() { CHECK(ipc.ConnectToService(kernel,"gsp::Gpu",&session) == 0); }
    std::uint32_t cb() const { return kernel.current_thread()->tls_address+kIpcCommandBufferOffset; }
    void Put(const IpcCommandBuffer& q = {0x000C0000}, GuestMemory* other = nullptr) {
        auto& m = other ? *other : memory;
        for(unsigned i=0;i<q.size();++i) CHECK(m.Write32(cb()+4*i,q[i]));
        for(unsigned i=0;i<16;++i) cpu.r[i]=0xABC00000+i;
        cpu.r[0]=session; cpu.r[15]=0x25947C; cpu.cpsr=0x60000010; cpu.fpscr=0x23000010;
        cpu.exclusive_address=kData; cpu.exclusive_size=4; cpu.exclusive_token=123; cpu.exclusive_valid=true;
    }
    IpcCommandBuffer Read(GuestMemory* other = nullptr) {
        auto& m=other?*other:memory; IpcCommandBuffer q{};
        for(unsigned i=0;i<q.size();++i) CHECK(m.Read32(cb()+4*i,&q[i])); return q;
    }
    a32::ExecutionResult Call(GuestMemory* other = nullptr) {
        return bridge.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcSendSyncRequest},cpu,other?other:&memory);
    }
    std::vector<std::uint8_t> Page() const {
        const auto b=endpoint->shared_memory()->bytes(); return {b.begin(),b.end()};
    }
    void Word(std::uint32_t offset,std::uint32_t value) { CHECK(memory.Write32(kShared+offset,value)); }
    std::uint32_t Word(std::uint32_t offset) {
        std::uint32_t v=0; CHECK(memory.Read32(kShared+offset,&v)); return v;
    }
    void Packet(unsigned index, std::uint32_t flags = 0x00000105, unsigned slot = 0) {
        const auto p=kQueue+slot*0x200+0x20+index*0x20;
        Word(p,flags); Word(p+4,kData+0x3790); Word(p+8,0x7480);
        Word(p+12,0); Word(p+16,0); Word(p+20,0); Word(p+24,0); Word(p+28,0xDEADC0DE);
    }
    void Stable(const a32::GuestState& before, std::size_t handles, std::size_t threads,
                std::uint64_t now, bool signaled) {
        CHECK(kernel.handles().OpenHandleCount()==handles && kernel.threads().size()==threads && kernel.now_ns()==now);
        CHECK(std::dynamic_pointer_cast<EventObject>(kernel.handles().Get(event))->signaled()==signaled);
        CHECK(kernel.current_thread()->status==ThreadStatus::Running && !kernel.current_thread()->pending_wake);
        CHECK(cpu.cpsr==before.cpsr && cpu.fpscr==before.fpscr);
        CHECK(cpu.exclusive_address==before.exclusive_address && cpu.exclusive_size==before.exclusive_size &&
              cpu.exclusive_token==before.exclusive_token && cpu.exclusive_valid==before.exclusive_valid);
    }
    void Reply(std::uint32_t expected, unsigned slot=0) {
        Put(); const auto before=cpu;
        const auto handles=kernel.handles().OpenHandleCount(),threads=kernel.threads().size();const auto now=kernel.now_ns();
        const bool signal=std::dynamic_pointer_cast<EventObject>(kernel.handles().Get(event))->signaled();
        auto want=Page();const auto pos=kQueue+slot*0x200;
        for(unsigned i=0;i<4;++i)want[pos+i]=static_cast<std::uint8_t>(expected>>(8*i));
        CHECK(Call().kind==a32::ExitKind::Fallthrough && cpu.r[0]==0 && cpu.r[15]==0x259480);
        CHECK(Read()==(IpcCommandBuffer{0x000C0040,0})); CHECK(Page()==want);
        for(unsigned i=1;i<15;++i) CHECK(cpu.r[i]==before.r[i]);
        for(unsigned i=0;i<0x10000;i+=4){std::uint32_t v=0;CHECK(memory.Read32(kData+i,&v) && v==0x12340000+i);}
        Stable(before,handles,threads,now,signal);
    }
    void Stop(const IpcCommandBuffer& q={0x000C0000},GuestMemory* other=nullptr) {
        Put(q,other);const auto before=cpu;const auto page=Page();
        const auto handles=kernel.handles().OpenHandleCount(),threads=kernel.threads().size();const auto now=kernel.now_ns();
        const bool signal=std::dynamic_pointer_cast<EventObject>(kernel.handles().Get(event))->signaled();
        CHECK(Call(other).kind==a32::ExitKind::Svc && ipc.unsupported_request());
        CHECK(Read(other)==q && Page()==page && cpu.r==before.r);
        Stable(before,handles,threads,now,signal);
    }
};
void OriginalPacketAndWrap() {
    Fixture f; f.Packet(0); f.Word(kQueue,0x100); f.Reply(1); f.Reply(1); // Empty is idempotent.
    CHECK(f.kernel.SignalEvent(f.event)==0);f.Packet(14,0xA5000105);f.Packet(0);f.Packet(1);
    f.Word(kQueue,0x30E);f.Reply(2); // Index wraps 14 -> 0 -> 1 -> 2, no new event.
    for(unsigned i=0;i<15;++i)f.Packet(i);
    f.Word(kQueue,0xF07);f.Reply(7); // Full ring, same index after fifteen commands.
    Fixture zero;zero.Packet(0);
    for(unsigned n=0;n<3;++n){zero.Word(kQueue+0x24+n*8,0xFFFFFFFF);zero.Word(kQueue+0x28+n*8,0);}
    zero.Word(kQueue,0x100);zero.Reply(1);
    // Region validation does not demand write access or change read-only bytes.
    Fixture ro;CHECK(ro.memory.Map(0x18000000,4,MemoryPermission::Read));ro.Packet(0);
    for(unsigned n=0;n<3;++n){ro.Word(kQueue+0x24+n*8,0x18000000);ro.Word(kQueue+0x28+n*8,4);}
    ro.Word(kQueue,0x100);ro.Reply(1);
}
void StopFlags() {
    Fixture f;f.Packet(0,0x00FF0105);f.Packet(1,1);f.Word(kQueue,0x200);
    f.Reply(0x00010101); // Stop byte consumes first command, leaves unsupported tail pending.
    f.Reply(0x00010101); // Already STOPPED: tail must not be inspected or consumed.
    f.Word(kQueue,0x02000200);f.Reply(0x02010200); // should_stop wins; no dequeue.
    f.Word(kQueue,0xFF000000);f.Reply(0xFF000000); // Empty: even should_stop does nothing.
    f.Packet(0,0x00010005);f.Word(kQueue,0x100);f.Reply(0x10001); // Stop on final packet retained.
}
void UnsupportedAndMalformed() {
    Fixture f;
    for(auto id:{0U,1U,2U,3U,4U,6U,255U}){f.Packet(0,id);f.Word(kQueue,0x100);f.Stop();}
    f.Packet(0);f.Packet(1,1);f.Word(kQueue,0x200);f.Stop(); // NO prefix progress on unsupported tail.
    for(auto header:{0x10FU,0x1000U,0x800100U,0x020100U,0xFF0100U}){f.Word(kQueue,header);f.Stop();}
    f.Word(kQueue,0x100);f.Packet(0);
    for(auto q:{IpcCommandBuffer{0x000C0001},IpcCommandBuffer{0x000C0040},IpcCommandBuffer{0x000D0000}})f.Stop(q);
    for(unsigned n=0;n<3;++n){
        for(auto bad:std::array<std::array<std::uint32_t,2>,4>{{{0xDEAD0000,4},{0xFFFFFFFE,4},{kData+0xFFFF,2},{0,1}}}){
            f.Packet(0);f.Word(kQueue+0x24+n*8,bad[0]);f.Word(kQueue+0x28+n*8,bad[1]);f.Stop();
        }
    }
    // A partially valid multi-packet batch with invalid second region is atomic too.
    f.Packet(0);f.Packet(1);f.Word(kQueue,0x200);f.Word(kQueue+0x48,0xFFFFFFFF);f.Stop();
}
void OwnershipAndNoAllocations() {
    Fixture none(false);none.Packet(0);none.Word(kQueue,0x100);none.Stop();
    Fixture f;const auto owner=f.session;f.Connect();
    f.Packet(0);f.Word(kQueue,0x100);f.Packet(0,1,1);f.Word(kQueue+0x200,0x100);
    f.Reply(1); // Caller slot 1 triggers OWNER slot 0; slot 1 bytes remain untouched.
    Handle duplicate=0;CHECK(f.kernel.DuplicateHandle(&duplicate,owner)==0);CHECK(f.kernel.CloseHandle(owner)==0);
    f.Word(kQueue,0x100);f.Reply(1);CHECK(f.kernel.CloseHandle(duplicate)==0);f.Stop();
    Fixture full;full.Packet(0);full.Word(kQueue,0x100);std::vector<Handle> handles;
    for(;;){Handle h=0;auto rc=full.kernel.DuplicateHandle(&h,full.event);if(rc){CHECK(rc==kResultOutOfHandles);break;}handles.push_back(h);}
    full.Reply(1);
    // Nonzero owner slot is selected, not a hard-coded slot-zero queue.
    Fixture second(false);second.Connect();second.Put({0x00160042,0,0,kCurrentProcessPseudoHandle});
    CHECK(second.Call().kind==a32::ExitKind::Fallthrough);second.Packet(0,5,1);second.Word(kQueue+0x200,0x100);second.Reply(1,1);
}
void ResponsePreflightAndAliases() {
    Fixture f;f.Packet(0);f.Word(kQueue,0x100);const IpcCommandBuffer request{0x000C0000};
    for(bool partial:{false,true}){
        GuestMemory blocked;const auto n=partial?4U:unsigned(sizeof(request));
        CHECK(blocked.Map(f.cb(),n,partial?RW:MemoryPermission::Read));
        CHECK(blocked.LoadBytes(f.cb(),{reinterpret_cast<const std::uint8_t*>(request.data()),n}));
        f.Put();const auto page=f.Page();CHECK(f.Call(&blocked).kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer);
        CHECK(f.Page()==page);std::uint32_t word=0;CHECK(blocked.Read32(f.cb(),&word) && word==request[0]);
    }
    GuestMemory aliases;auto object=f.endpoint->shared_memory();
    CHECK(aliases.MapSharedServicePage(f.cb()&~0xFFFU,object,RW));
    CHECK(aliases.MapSharedServicePage(0x11000000,object,RW));
    CHECK(aliases.UsesSharedBacking(f.cb(),sizeof(request),*object));
    CHECK(aliases.UsesSharedBacking(0x11000000,0x1000,*object));
    CHECK(!aliases.UsesSharedBacking(0x11000FFF,2,*object));
    CHECK(!aliases.UsesSharedBacking(0x11000000,0,*object));
    ServiceSharedMemoryObject different;CHECK(!aliases.UsesSharedBacking(0x11000000,4,different));
    f.Stop(request,&aliases); // A reply in any VA of the GSP page must not overwrite it.
}
void ExclusiveReservations() {
    Fixture f;f.Packet(0);f.Word(kQueue,0x100);f.Put();
    std::uint64_t value=0,queue_token=0,data_token=0,packet_token=0;std::uint32_t fault=0;
    CHECK(f.memory.LoadExclusive(kShared+kQueue,4,&value,&queue_token,&fault));
    CHECK(f.memory.LoadExclusive(kData,4,&value,&data_token,&fault));
    CHECK(f.memory.LoadExclusive(kShared+kQueue+0x20,4,&value,&packet_token,&fault));
    CHECK(f.Call().kind==a32::ExitKind::Fallthrough);
    CHECK(f.memory.StoreExclusive(kShared+kQueue,4,1,queue_token,&fault)==a32::ExclusiveStoreResult::ReservationLost);
    CHECK(f.memory.StoreExclusive(kData,4,0x12340000,data_token,&fault)==a32::ExclusiveStoreResult::Success);
    CHECK(f.memory.StoreExclusive(kShared+kQueue+0x20,4,0x105,packet_token,&fault)==a32::ExclusiveStoreResult::Success);
    // Empty queue does not even rewrite its header or lose the header reservation.
    f.Put();CHECK(f.memory.LoadExclusive(kShared+kQueue,4,&value,&queue_token,&fault));CHECK(f.Call().kind==a32::ExitKind::Fallthrough);
    CHECK(f.memory.StoreExclusive(kShared+kQueue,4,1,queue_token,&fault)==a32::ExclusiveStoreResult::Success);
}
} // namespace
int main() {
    OriginalPacketAndWrap();StopFlags();UnsupportedAndMalformed();OwnershipAndNoAllocations();
    ResponsePreflightAndAliases();ExclusiveReservations();
    if(failures)return EXIT_FAILURE;
    std::cout << "PASS: owner-selected CacheFlush queue, bounded progress, stop flags, atomic guards and no IRQ/time\n";
    return EXIT_SUCCESS;
}
