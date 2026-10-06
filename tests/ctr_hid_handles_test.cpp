#include "services/hid_user_service.h"
#include "runtime/ctr_svc_bridge.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>

namespace {
using namespace lego::ctr;
int failures = 0;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL " << __LINE__ << ": " #x "\n"; ++failures; } } while (0)
struct Fixture {
    Kernel kernel; GuestMemory memory; IpcRouter router; SvcBridge bridge{kernel,&router};
    std::shared_ptr<HidUserService> endpoint = std::make_shared<HidUserService>();
    Handle session{}; a32::GuestState cpu{};
    Fixture() {
        CHECK(memory.EnsureTlsMappings(kernel));
        CHECK(router.RegisterService("hid:USER",endpoint)==0);
        CHECK(router.ConnectToService(kernel,"hid:USER",&session)==0);
    }
    std::uint32_t cb() const { return kernel.current_thread()->tls_address+kIpcCommandBufferOffset; }
    void Put(IpcCommandBuffer q={0x000A0000}) {
        for(unsigned i=0;i<q.size();++i)CHECK(memory.Write32(cb()+4*i,q[i]));
        for(unsigned i=0;i<16;++i)cpu.r[i]=0x34560000+i;
        cpu.r[0]=session;cpu.r[15]=0x25947C;cpu.cpsr=0x60000010;cpu.fpscr=0x23000010;
    }
    IpcCommandBuffer Read(GuestMemory* m=nullptr) {
        IpcCommandBuffer q{};for(unsigned i=0;i<q.size();++i)CHECK((m?m:&memory)->Read32(cb()+4*i,&q[i]));return q;
    }
    a32::ExecutionResult Call(GuestMemory* m=nullptr) {
        return bridge.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcSendSyncRequest},cpu,m?m:&memory);
    }
    std::array<Handle,6> Get() {
        Put();const auto before=cpu;const auto count=kernel.handles().OpenHandleCount();
        const auto now=kernel.now_ns();const auto threads=kernel.threads().size();
        CHECK(Call().kind==a32::ExitKind::Fallthrough);
        CHECK(cpu.r[0]==0 && cpu.r[15]==before.r[15]+4);
        for(unsigned i=1;i<15;++i)CHECK(cpu.r[i]==before.r[i]);
        CHECK(cpu.cpsr==before.cpsr && cpu.fpscr==before.fpscr);
        const auto reply=Read();CHECK(reply[0]==0x000A0047 && reply[1]==0 && reply[2]==0x14000000);
        for(unsigned i=9;i<reply.size();++i)CHECK(reply[i]==0);
        CHECK(!router.unsupported_request() && router.last_session_name()=="hid:USER");
        CHECK(kernel.handles().OpenHandleCount()==count+6 && kernel.now_ns()==now && kernel.threads().size()==threads);
        std::array<Handle,6> out{};
        for(unsigned i=0;i<6;++i)out[i]=reply[3+i];
        CHECK(kernel.handles().Get(out[0])==endpoint->shared_memory());
        for(unsigned i=0;i<5;++i)CHECK(kernel.handles().Get(out[1+i])==endpoint->event(i));
        CHECK(std::set<Handle>(out.begin(),out.end()).size()==6);return out;
    }
    void Map(Handle h,std::uint32_t address,std::uint32_t permission,Result expected) {
        cpu={};cpu.r[0]=h;cpu.r[1]=address;cpu.r[2]=permission;cpu.r[3]=0x10000000;cpu.r[15]=0x25947C;
        const auto stop=bridge.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcMapMemoryBlock},cpu,&memory);
        CHECK(stop.kind==a32::ExitKind::Fallthrough && cpu.r[0]==expected);
    }
};
void ExportsAndLifetime() {
    Fixture f;
    CHECK(!f.endpoint->CanHandle({0x000A0000}));
    CHECK(f.endpoint->shared_memory()->size()==4096);
    CHECK(f.endpoint->shared_memory()->owner_permissions()==3 && f.endpoint->shared_memory()->other_permissions()==1);
    CHECK(std::all_of(f.endpoint->shared_memory()->bytes().begin(),f.endpoint->shared_memory()->bytes().end(),[](auto v){return v==0;}));
    std::set<const EventObject*> distinct;
    for(unsigned i=0;i<5;++i){auto e=f.endpoint->event(i);CHECK(e && !e->signaled() && e->reset_type()==ResetType::OneShot);distinct.insert(e.get());}
    CHECK(distinct.size()==5 && !f.endpoint->event(5));
    const auto first=f.Get();
    const std::array<std::uint8_t,4> pattern{0x12,0x34,0x56,0x78};
    CHECK(f.endpoint->shared_memory()->Write(0x40,pattern));
    const auto epoch=f.endpoint->shared_memory()->Epoch(0x40);f.endpoint->event(0)->Signal();
    const auto second=f.Get();CHECK(first!=second);
    CHECK(f.endpoint->shared_memory()->Epoch(0x40)==epoch && f.endpoint->event(0)->signaled());
    for(auto h:first)CHECK(f.kernel.CloseHandle(h)==0);
    for(auto h:second)CHECK(f.kernel.CloseHandle(h)==0);
    CHECK(f.kernel.CloseHandle(f.session)==0);
    CHECK(f.router.ConnectToService(f.kernel,"hid:USER",&f.session)==0);
    const auto third=f.Get();CHECK(f.kernel.handles().Get(third[0])==f.endpoint->shared_memory());
    CHECK(f.endpoint->event(0)->signaled() && f.endpoint->shared_memory()->Epoch(0x40)==epoch);
    Fixture independent;CHECK(independent.endpoint->shared_memory()!=f.endpoint->shared_memory());
    CHECK(!independent.endpoint->event(0)->signaled());
}
void MappingAndRealEvents() {
    Fixture f;auto h=f.Get();
    for(auto perms:{2U,3U,4U}) {f.Map(h[0],0x10001000,perms,kResultInvalidCombination);CHECK(!f.memory.IsMapped(0x10001000,4096));}
    f.Map(h[0],0x10001000,1,0);f.Map(h[0],0x10002000,1,0);
    CHECK(f.memory.IsReadable(0x10001000,4096) && !f.memory.IsWritable(0x10001000,4096));
    CHECK(!f.memory.Write32(0x10001040,0x87654321));
    CHECK(!f.memory.MapSharedServicePage(0x10003000,f.endpoint->shared_memory(),MemoryPermission::Read|MemoryPermission::Write));
    const std::array<std::uint8_t,4> pattern{0x12,0x34,0x56,0x78};
    CHECK(f.endpoint->shared_memory()->Write(0x40,pattern));
    std::uint32_t word=0;CHECK(f.memory.Read32(0x10002040,&word) && word==0x78563412);
    CHECK(f.memory.SpansAlias(0x10001040,4,0x10002040,4));
    CHECK(f.kernel.CloseHandle(h[0])==0);CHECK(f.memory.Read32(0x10001040,&word) && word==0x78563412);
    CHECK(f.kernel.WaitSynchronization1(h[1],0).result==kResultTimeout);
    const auto event=f.endpoint->event(0);const auto waited=f.kernel.current_thread();
    CHECK(f.kernel.WaitSynchronization1(h[1],-1).blocked);
    CHECK(f.kernel.CloseHandle(h[1])==0);
    f.kernel.SignalEventObject(*event); // SYNTHETIC test action, never input production.
    CHECK(waited->status==ThreadStatus::Ready && waited->pending_wake && !event->signaled());
    for(unsigned i=1;i<5;++i)CHECK(!f.endpoint->event(i)->signaled());
}
void MalformedAndProtected() {
    Fixture f;const auto count=f.kernel.handles().OpenHandleCount();
    for(auto header:{0x000A0040U,0x000A0002U,0x000A1000U,0x00110000U,0x00130000U,0x00170000U,0x00010000U}) {
        IpcCommandBuffer q{};q.fill(0xA1B2C3D4);q[0]=header;f.Put(q);const auto before=f.cpu;
        CHECK(f.Call().kind==a32::ExitKind::Svc && f.router.unsupported_request());
        CHECK(f.Read()==q && f.cpu.r==before.r && f.cpu.cpsr==before.cpsr);
        CHECK(f.kernel.handles().OpenHandleCount()==count);
    }
    for(int mode=0;mode<3;++mode) {
        f.Put();GuestMemory protected_mem;const IpcCommandBuffer q{0x000A0000};
        const unsigned bytes=mode==1?4:sizeof(q);
        const auto permission=mode==0?MemoryPermission::Read:mode==1?MemoryPermission::Read|MemoryPermission::Write:MemoryPermission::Write;
        CHECK(protected_mem.Map(f.cb(),bytes,permission));
        CHECK(protected_mem.LoadBytes(f.cb(),{reinterpret_cast<const std::uint8_t*>(q.data()),bytes}));
        CHECK(f.Call(&protected_mem).kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer);
        CHECK(f.kernel.handles().OpenHandleCount()==count);
        if(mode!=2){std::uint32_t word=0;CHECK(protected_mem.Read32(f.cb(),&word) && word==q[0]);}
    }
    for(unsigned i=0;i<5;++i)CHECK(!f.endpoint->event(i)->signaled());
}
void BatchAllocationFailures() {
    auto object=std::make_shared<EventObject>(ResetType::OneShot);
    for(unsigned free=0;free<6;++free) {
        Fixture f;Handle spare=0;
        while(f.kernel.handles().OpenHandleCount()<HandleTable::kMaxCount-free)
            CHECK(f.kernel.handles().Create(&spare,object)==0);
        auto expected=f.kernel.handles();const auto count=f.kernel.handles().OpenHandleCount();
        f.Put();const auto q=f.Read();CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultOutOfHandles);
        CHECK(f.Read()==q && f.kernel.handles().OpenHandleCount()==count);
        // Comparing subsequent allocations detects consumed generation/free-list state.
        if(free==0){CHECK(f.kernel.handles().Close(spare)==0);CHECK(expected.Close(spare)==0);}
        Handle actual=0,wanted=0;CHECK(expected.Create(&wanted,object)==0);CHECK(f.kernel.handles().Create(&actual,object)==0);CHECK(actual==wanted);
    }
    HandleTable table;std::array<Handle,2> out{0xAA,0xBB};
    std::array<std::shared_ptr<KernelObject>,2> objects{object,nullptr};
    CHECK(table.CreateCopies(objects,out)==kResultInvalidHandle && out[0]==0xAA && out[1]==0xBB && table.OpenHandleCount()==0);
    objects[1]=object;CHECK(table.CreateCopies(objects,std::span(out).first(1))==kResultInvalidPointer);
    CHECK(table.CreateCopies({}, {})==kResultInvalidPointer);
    CHECK(table.CreateCopies(objects,out)==0 && out[0]!=out[1] && table.OpenHandleCount()==2);
}
void SessionsAndFailedConnection() {
    Fixture f;std::array<Handle,6> sessions{};sessions[0]=f.session;
    Handle duplicate=0;CHECK(f.kernel.DuplicateHandle(&duplicate,f.session)==0);
    for(unsigned i=1;i<6;++i)CHECK(f.router.ConnectToService(f.kernel,"hid:USER",&sessions[i])==0);
    Handle blocked=0xBB;CHECK(f.router.ConnectToService(f.kernel,"hid:USER",&blocked)==kResultMaxConnectionsReached && blocked==0xBB);
    CHECK(f.kernel.CloseHandle(sessions[0])==0);
    CHECK(f.router.ConnectToService(f.kernel,"hid:USER",&blocked)==kResultMaxConnectionsReached);
    CHECK(f.kernel.CloseHandle(duplicate)==0);CHECK(f.router.ConnectToService(f.kernel,"hid:USER",&f.session)==0);
    f.Get();
    Fixture full;auto filler=std::make_shared<EventObject>(ResetType::OneShot);Handle last=0;std::vector<Handle> filled;
    while(full.kernel.handles().OpenHandleCount()<HandleTable::kMaxCount){CHECK(full.kernel.handles().Create(&last,filler)==0);filled.push_back(last);}
    CHECK(full.router.ConnectToService(full.kernel,"hid:USER",&blocked)==kResultOutOfHandles);
    // A failed connector allocation must not retain the temporary session lease.
    for(unsigned i=0;i<5;++i){CHECK(full.kernel.handles().Close(filled[filled.size()-1-i])==0);}
    for(unsigned i=0;i<5;++i)CHECK(full.router.ConnectToService(full.kernel,"hid:USER",&blocked)==0);
    CHECK(full.router.ConnectToService(full.kernel,"hid:USER",&blocked)==kResultMaxConnectionsReached);
    try{ServiceSharedMemoryObject bad(2);CHECK(false);}catch(const std::invalid_argument&){}
}
}
int main(){
    ExportsAndLifetime();MappingAndRealEvents();MalformedAndProtected();BatchAllocationFailures();SessionsAndFailedConnection();
    if(failures)return EXIT_FAILURE;
    std::cout<<"PASS: HID six-object export, persistent read-only backing, real one-shot events, atomic allocation and strict guards\n";
    return EXIT_SUCCESS;
}
