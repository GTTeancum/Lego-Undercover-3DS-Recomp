#include "services/gsp_gpu_service.h"
#include "runtime/ctr_svc_bridge.h"
#include <cstdlib>
#include <iostream>

namespace {
using namespace lego::ctr;
int failures=0;
#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}}while(0)
struct Fixture {
    Kernel kernel;GuestMemory memory;IpcRouter ipc;SvcBridge bridge{kernel,&ipc};
    std::shared_ptr<GspGpuService> endpoint=std::make_shared<GspGpuService>();
    Handle session{},event{};a32::GuestState cpu{};
    Fixture(){CHECK(memory.EnsureTlsMappings(kernel));CHECK(ipc.RegisterService("gsp::Gpu",endpoint)==0);Connect();CHECK(kernel.CreateEvent(&event,0)==0);}
    void Connect(){CHECK(ipc.ConnectToService(kernel,"gsp::Gpu",&session)==0);}
    std::shared_ptr<GspGpuService> Session()const{
        auto p=std::dynamic_pointer_cast<ClientSessionObject>(kernel.handles().Get(session));
        return p?std::dynamic_pointer_cast<GspGpuService>(p->service):nullptr;
    }
    std::uint32_t cb()const{return kernel.current_thread()->tls_address+kIpcCommandBufferOffset;}
    void Put(const IpcCommandBuffer& q){for(unsigned i=0;i<q.size();++i)CHECK(memory.Write32(cb()+4*i,q[i]));for(unsigned i=0;i<16;++i)cpu.r[i]=0x11220000+i;cpu.r[0]=session;cpu.r[15]=0x25947C;cpu.cpsr=0x60000010;}
    IpcCommandBuffer Read(){IpcCommandBuffer q{};for(unsigned i=0;i<q.size();++i)CHECK(memory.Read32(cb()+4*i,&q[i]));return q;}
    a32::ExecutionResult Call(GuestMemory* m=nullptr){return bridge.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcSendSyncRequest},cpu,m?m:&memory);}
    Handle Register(Result expected,std::uint32_t slot,std::uint32_t flags=1){
        auto ev=std::dynamic_pointer_cast<EventObject>(kernel.handles().Get(event));const bool signaled=ev->signaled();
        const auto count=kernel.handles().OpenHandleCount(),threads=kernel.threads().size();const auto time=kernel.now_ns();
        const auto rights=endpoint->rights_held();Put({0x00130042,flags,0,event});auto before=cpu;
        CHECK(Call().kind==a32::ExitKind::Fallthrough && cpu.r[0]==0 && cpu.r[15]==0x259480);
        for(unsigned i=1;i<15;++i)CHECK(cpu.r[i]==before.r[i]);CHECK(cpu.cpsr==before.cpsr);
        const auto q=Read();CHECK(q[0]==0x00130082 && q[1]==expected && q[2]==slot && q[3]==0 && q[4]!=0);
        for(unsigned i=5;i<q.size();++i)CHECK(q[i]==0);
        CHECK(kernel.handles().Get(q[4])==endpoint->shared_memory());
        CHECK(Session()->registered() && Session()->relay_event()==ev && Session()->relay_flags()==flags);
        CHECK(Session()->relay_slot()==slot && !endpoint->relay_slot() && !endpoint->registered());
        CHECK(ev->signaled()==signaled && kernel.handles().Get(event)==ev);
        CHECK(count+1==kernel.handles().OpenHandleCount() && threads==kernel.threads().size() && time==kernel.now_ns());
        CHECK(endpoint->rights_held()==rights && !kernel.current_thread()->pending_wake);
        CHECK(!memory.IsMapped(0x10000000)); // Registration itself is not mapping.
        return q[4];
    }
};
void RegistrationAndLifetime(){
    Fixture f;CHECK(f.endpoint->first_registration_pending());
    for(auto b:f.endpoint->shared_memory()->bytes())CHECK(b==0);
    const auto first=f.Register(kResultGspFirstInitialization,0);
    const std::array<std::uint8_t,1> marker{0x5A};CHECK(f.endpoint->shared_memory()->Write(0xFF0,marker));
    Handle copy=0;CHECK(f.kernel.DuplicateHandle(&copy,first)==0);CHECK(f.kernel.handles().Get(copy)==f.kernel.handles().Get(first));
    CHECK(f.kernel.SignalEvent(f.event)==0);const auto repeat=f.Register(0,0,0);CHECK(repeat!=first);
    CHECK(f.endpoint->shared_memory()->bytes()[0xFF0]==0x5A); // Never zero the page on repeat registration.
    CHECK(f.kernel.CloseHandle(first)==0 && f.kernel.handles().Get(copy));
    const auto s0=f.session;f.Connect();const auto s1=f.session;CHECK(f.Session()->relay_slot()==1);f.Register(0,1);
    auto old_event=f.Session()->relay_event();std::weak_ptr<EventObject> weak=old_event;old_event.reset();
    CHECK(f.kernel.CloseHandle(f.event)==0 && !weak.expired()); // Stored copied event is owned by both sessions.
    Handle duplicate_session=0;CHECK(f.kernel.DuplicateHandle(&duplicate_session,s1)==0);
    CHECK(f.kernel.CloseHandle(s1)==0 && !weak.expired());
    CHECK(f.kernel.CloseHandle(s0)==0 && !weak.expired());
    CHECK(f.kernel.CloseHandle(duplicate_session)==0 && weak.expired());
    f.Connect();CHECK(f.Session()->relay_slot()==0);CHECK(f.kernel.CreateEvent(&f.event,0)==0);f.Register(0,0);
    CHECK(!f.endpoint->first_registration_pending());
    Fixture independent;independent.Register(kResultGspFirstInitialization,0);
}
void SlotsAndFailedConnection(){
    Fixture f;std::array<Handle,4> sessions{};sessions[0]=f.session;
    for(unsigned i=1;i<4;++i){f.Connect();sessions[i]=f.session;CHECK(f.Session()->relay_slot()==i);}
    f.Register(kResultGspFirstInitialization,3); // First registration is module-wide, not slot-zero-specific.
    Handle out=0xFADE;const auto count=f.kernel.handles().OpenHandleCount();
    CHECK(f.ipc.ConnectToService(f.kernel,"gsp::Gpu",&out)==kResultMaxConnectionsReached && out==0xFADE);
    CHECK(f.kernel.handles().OpenHandleCount()==count);
    Handle duplicate=0;CHECK(f.kernel.DuplicateHandle(&duplicate,sessions[1])==0);CHECK(f.kernel.CloseHandle(sessions[1])==0);
    CHECK(f.ipc.ConnectToService(f.kernel,"gsp::Gpu",&out)==kResultMaxConnectionsReached);
    CHECK(f.kernel.CloseHandle(duplicate)==0);f.Connect();CHECK(f.Session()->relay_slot()==1);
    CHECK(f.ipc.ConnectToService(f.kernel,"gsp::Gpu",nullptr)==kResultInvalidPointer);
    CHECK(f.endpoint->CreateSessionHandler(nullptr)==kResultInvalidPointer);
    // A failed kernel handle insertion must release its freshly reserved GSP slot.
    Fixture exhausted;std::vector<Handle> filler;
    for(;;){Handle h=0;auto rc=exhausted.kernel.handles().Create(&h,exhausted.kernel.current_process());if(rc){CHECK(rc==kResultOutOfHandles);break;}filler.push_back(h);}
    out=0xFADE;CHECK(exhausted.ipc.ConnectToService(exhausted.kernel,"gsp::Gpu",&out)==kResultOutOfHandles && out==0xFADE);
    CHECK(exhausted.kernel.CloseHandle(filler.back())==0);exhausted.Connect();CHECK(exhausted.Session()->relay_slot()==1);
}
void FailedRegistrationIsAtomic(){
    Fixture f;const IpcCommandBuffer good{0x00130042,1,0,f.event};
    for(auto q:{IpcCommandBuffer{0x00130043,1,0,f.event},IpcCommandBuffer{0x00130042,2,0,f.event},IpcCommandBuffer{0x00130042,1,0x10,f.event}}){
        f.Put(q);const auto before=f.cpu;CHECK(f.Call().kind==a32::ExitKind::Svc && f.cpu.r==before.r && f.Read()==q);
        CHECK(!f.Session()->registered() && f.endpoint->first_registration_pending());
    }
    for(auto h:{Handle(0),f.session,kCurrentThreadPseudoHandle,kCurrentProcessPseudoHandle,Handle(0xDEADBEEF)}){
        auto q=good;q[3]=h;f.Put(q);CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidHandle && f.Read()==q);
        CHECK(!f.Session()->registered() && f.endpoint->first_registration_pending());
    }
    for(bool partial:{false,true}){
        f.Put(good);GuestMemory blocked;const auto n=partial?16U:unsigned(sizeof(good));
        CHECK(blocked.Map(f.cb(),n,partial?MemoryPermission::Read|MemoryPermission::Write:MemoryPermission::Read));
        CHECK(blocked.LoadBytes(f.cb(),{reinterpret_cast<const std::uint8_t*>(good.data()),n}));
        const auto count=f.kernel.handles().OpenHandleCount();CHECK(f.Call(&blocked).kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer);
        CHECK(f.kernel.handles().OpenHandleCount()==count && !f.Session()->registered() && f.endpoint->first_registration_pending());
    }
    std::vector<Handle> filler;
    for(;;){Handle h=0;auto rc=f.kernel.handles().Create(&h,f.kernel.current_process());if(rc){CHECK(rc==kResultOutOfHandles);break;}filler.push_back(h);}
    f.Put(good);CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultOutOfHandles && f.Read()==good);
    CHECK(!f.Session()->registered() && !f.Session()->relay_event() && f.endpoint->first_registration_pending());
    CHECK(f.kernel.CloseHandle(filler.back())==0);f.Register(kResultGspFirstInitialization,0);
}
}
int main(){RegistrationAndLifetime();SlotsAndFailedConnection();FailedRegistrationIsAtomic();if(failures)return EXIT_FAILURE;std::cout<<"PASS: real GSP relay slots, copy ownership, atomic registration, no event synthesis\n";return EXIT_SUCCESS;}
