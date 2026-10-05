#include "services/gsp_gpu_service.h"
#include "runtime/ctr_runner.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
using namespace lego::ctr;
int failures=0;
#define CHECK(x) do { if (!(x)) { std::cerr<<"FAIL "<<__LINE__<<": " #x "\n"; ++failures; } } while (0)
struct Fixture {
    Kernel kernel{7,3};GuestMemory memory;IpcRouter router;SvcBridge bridge{kernel,&router};
    std::shared_ptr<GspGpuService> endpoint=std::make_shared<GspGpuService>();Handle handle{};
    a32::GuestState cpu{};
    Fixture() {CHECK(memory.EnsureTlsMappings(kernel));CHECK(router.RegisterService("gsp::Gpu",endpoint)==0);Connect();}
    void Connect(){CHECK(router.ConnectToService(kernel,"gsp::Gpu",&handle)==0);}
    std::shared_ptr<GspGpuService> Session(){
        auto s=std::dynamic_pointer_cast<ClientSessionObject>(kernel.handles().Get(handle));
        if(!s)throw std::runtime_error("missing GSP session");
        auto gsp=std::dynamic_pointer_cast<GspGpuService>(s->service);
        if(!gsp)throw std::runtime_error("wrong service");return gsp;
    }
    std::uint32_t cb()const{return kernel.current_thread()->tls_address+kIpcCommandBufferOffset;}
    void Put(const IpcCommandBuffer& q){
        for(unsigned i=0;i<q.size();++i)CHECK(memory.Write32(cb()+4*i,q[i]));
        for(unsigned i=0;i<16;++i)cpu.r[i]=0x12340000+i;
        cpu.r[0]=handle;cpu.r[15]=0x25947C;cpu.cpsr=0x60000010;cpu.fpscr=0x23000010;
    }
    IpcCommandBuffer Read(){IpcCommandBuffer q{};for(unsigned i=0;i<q.size();++i)CHECK(memory.Read32(cb()+4*i,&q[i]));return q;}
    a32::ExecutionResult Call(GuestMemory* m=nullptr){return bridge.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcSendSyncRequest},cpu,m?m:&memory);}
    void Acquire(Handle process=kCurrentProcessPseudoHandle){
        Handle event=0;CHECK(kernel.CreateEvent(&event,static_cast<std::uint32_t>(ResetType::OneShot))==0);
        const auto ev=std::dynamic_pointer_cast<EventObject>(kernel.handles().Get(event));
        const auto count=kernel.handles().OpenHandleCount();const auto threads=kernel.threads().size();const auto time=kernel.now_ns();
        const IpcCommandBuffer q{0x00160042,0,0,process};Put(q);const auto before=cpu;
        CHECK(Call().kind==a32::ExitKind::Fallthrough && cpu.r[0]==0 && cpu.r[15]==0x259480);
        for(unsigned i=1;i<15;++i)CHECK(cpu.r[i]==before.r[i]);CHECK(cpu.cpsr==before.cpsr && cpu.fpscr==before.fpscr);
        CHECK(Read()==(IpcCommandBuffer{0x00160040,0}));CHECK(!router.unsupported_request());
        CHECK(router.last_request()==q && router.last_session_name()=="gsp::Gpu");
        CHECK(Session()->owns_rights() && endpoint->rights_held() && !endpoint->owns_rights());
        CHECK(endpoint->owner_process_id()==7 && endpoint->active_client_thread_id()==3);
        CHECK(kernel.handles().OpenHandleCount()==count && kernel.threads().size()==threads && kernel.now_ns()==time);
        CHECK(!ev->signaled() && kernel.current_thread()->status==ThreadStatus::Running && !kernel.current_thread()->pending_wake);
        CHECK(kernel.current_thread()->priority==kThreadPriorityDefault);CHECK(kernel.CloseHandle(event)==0);
    }
    void Stop(const IpcCommandBuffer& q){
        const auto owner=endpoint->owner_process_id(),thread=endpoint->active_client_thread_id();const auto count=kernel.handles().OpenHandleCount();
        Put(q);const auto before=cpu;CHECK(Call().kind==a32::ExitKind::Svc);
        CHECK(cpu.r==before.r && cpu.cpsr==before.cpsr && Read()==q && router.unsupported_request());
        CHECK(endpoint->owner_process_id()==owner && endpoint->active_client_thread_id()==thread && kernel.handles().OpenHandleCount()==count);
    }
};
void Discovery(){
    Kernel kernel;GuestMemory memory;const a32::Registry registry{};NativeRunner runner(registry,memory,kernel);
    auto& router=runner.ipc();CHECK(router.HasService("gsp::Gpu"));CHECK(memory.EnsureTlsMappings(kernel));
    constexpr std::uint32_t address=0x08000000;CHECK(memory.Map(address,5,MemoryPermission::Read));
    const std::array<std::uint8_t,5> name{'s','r','v',':',0};CHECK(memory.LoadBytes(address,name));
    Handle srv=0;CHECK(router.ConnectToPort(kernel,memory,address,&srv)==0);
    const auto cb=kernel.current_thread()->tls_address+kIpcCommandBufferOffset;
    const IpcCommandBuffer q{0x00050100,0x3A707367,0x7570473A,8,0};
    for(unsigned i=0;i<q.size();++i)CHECK(memory.Write32(cb+4*i,q[i]));
    CHECK(router.SendSyncRequest(kernel,memory,srv)==0);
    IpcCommandBuffer out{};for(unsigned i=0;i<out.size();++i)CHECK(memory.Read32(cb+4*i,&out[i]));
    CHECK(out[0]==0x00050042 && out[1]==0 && out[2]==IpcMoveHandleDesc() && out[3]!=0);
    auto session=std::dynamic_pointer_cast<ClientSessionObject>(kernel.handles().Get(out[3]));
    CHECK(session && session->name=="gsp::Gpu" && std::dynamic_pointer_cast<GspGpuService>(session->service));
}
void Ownership(){
    Fixture f;f.Acquire();const auto first=f.handle;Handle copy=0;CHECK(f.kernel.DuplicateHandle(&copy,first)==0);
    f.Stop({0x00160042,0,0,kCurrentProcessPseudoHandle}); // Repeat is deliberately not implemented.
    f.Connect();const auto second=f.handle;CHECK(!f.Session()->owns_rights());
    f.Stop({0x00160042,0,0,kCurrentProcessPseudoHandle}); // No invented contention/wakeup.
    CHECK(f.kernel.CloseHandle(first)==0 && f.endpoint->rights_held());
    CHECK(f.kernel.CloseHandle(copy)==0 && !f.endpoint->rights_held());
    CHECK(!f.endpoint->owner_process_id() && !f.endpoint->active_client_thread_id());
    f.Acquire();CHECK(f.kernel.CloseHandle(second)==0 && !f.endpoint->rights_held());
    f.Connect();Handle process=0;CHECK(f.kernel.DuplicateHandle(&process,kCurrentProcessPseudoHandle)==0);
    f.Acquire(process);CHECK(f.kernel.handles().Get(process)==f.kernel.current_process());
    Fixture independent;CHECK(!independent.endpoint->rights_held());independent.Acquire();
}
void Guards(){
    Fixture f;
    for(auto q:{IpcCommandBuffer{0x00160040,0,0,kCurrentProcessPseudoHandle},IpcCommandBuffer{0x00161042,0,0,kCurrentProcessPseudoHandle},
                IpcCommandBuffer{0x00160042,1,0,kCurrentProcessPseudoHandle},IpcCommandBuffer{0x00160042,0,0x10,kCurrentProcessPseudoHandle},
                IpcCommandBuffer{0x00160042,0,0x20,kCurrentProcessPseudoHandle},IpcCommandBuffer{0x00130043,1,0,0},
                IpcCommandBuffer{0x00170000},IpcCommandBuffer{0x00150002,0,kCurrentProcessPseudoHandle},IpcCommandBuffer{0x000C0000}})f.Stop(q);
    for(Handle object:{0U,kCurrentThreadPseudoHandle,f.handle,0xDEADBEEFU}){
        const IpcCommandBuffer q{0x00160042,0,0,object};f.Put(q);
        CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidHandle && f.Read()==q);
        CHECK(!f.endpoint->rights_held());
    }
    Handle other=0;CHECK(f.kernel.handles().Create(&other,std::make_shared<ProcessObject>(7))==0);
    f.Stop({0x00160042,0,0,other});CHECK(!f.router.last_host_error().empty()); // Equal numeric PID is not identity.
    const IpcCommandBuffer q{0x00160042,0,0,kCurrentProcessPseudoHandle};
    for(bool partial:{false,true}){
        f.Put(q);GuestMemory blocked;const auto n=partial?8U:unsigned(sizeof(q));
        CHECK(blocked.Map(f.cb(),n,partial?MemoryPermission::Read|MemoryPermission::Write:MemoryPermission::Read));
        CHECK(blocked.LoadBytes(f.cb(),{reinterpret_cast<const std::uint8_t*>(q.data()),n}));
        CHECK(f.Call(&blocked).kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer && !f.endpoint->rights_held());
        std::uint32_t word=0;CHECK(blocked.Read32(f.cb(),&word) && word==q[0]);
    }
    f.Acquire();
    f.Put({0x00130042,1,0,other});
    CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidHandle);
    CHECK(f.endpoint->rights_held() && !f.Session()->registered());
}
}
int main(){Discovery();Ownership();Guards();if(failures)return EXIT_FAILURE;std::cout<<"PASS: GSP discovery and first uncontended ownership; no interrupt or GPU completion fabricated\n";return EXIT_SUCCESS;}
