#include "services/fs_user_service.h"
#include "runtime/ctr_svc_bridge.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
using namespace lego::ctr;
int failures=0;
#define CHECK(x) do { if (!(x)) { std::cerr<<"FAIL "<<__LINE__<<": " #x "\n"; ++failures; } } while (0)
struct Fixture {
    Kernel kernel;
    GuestMemory memory;
    IpcRouter router;
    SvcBridge bridge{kernel,&router};
    std::shared_ptr<FsUserService> endpoint=std::make_shared<FsUserService>(0x00040000000AD500ULL);
    Handle handle{};
    a32::GuestState cpu{};
    Fixture() {
        CHECK(memory.EnsureTlsMappings(kernel));
        CHECK(router.RegisterService("fs:USER",endpoint)==0);
        Connect();
    }
    void Connect() { CHECK(router.ConnectToService(kernel,"fs:USER",&handle)==0); }
    std::shared_ptr<FsUserService> Session() {
        auto s=std::dynamic_pointer_cast<ClientSessionObject>(kernel.handles().Get(handle));
        if(!s) throw std::runtime_error("missing FS session");
        auto fs=std::dynamic_pointer_cast<FsUserService>(s->service);
        if(!fs) throw std::runtime_error("wrong FS session");
        return fs;
    }
    std::uint32_t cb() const { return kernel.current_thread()->tls_address+kIpcCommandBufferOffset; }
    void Put(const IpcCommandBuffer& q) {
        for(unsigned i=0;i<q.size();++i) CHECK(memory.Write32(cb()+4*i,q[i]));
        for(unsigned i=0;i<16;++i) cpu.r[i]=0xAA220000+i;
        cpu.r[0]=handle;cpu.r[15]=0x25947C;cpu.cpsr=0x60000010;cpu.fpscr=0x23000010;
    }
    IpcCommandBuffer Read() {
        IpcCommandBuffer q{};for(unsigned i=0;i<q.size();++i) CHECK(memory.Read32(cb()+4*i,&q[i]));return q;
    }
    a32::ExecutionResult Call(GuestMemory* m=nullptr) {
        return bridge.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcSendSyncRequest},cpu,m?m:&memory);
    }
    void Initialize() { Put({0x08610042,0x040203C8,0x20,0});CHECK(Call().kind==a32::ExitKind::Fallthrough && cpu.r[0]==0); }
    void Set(std::uint32_t value) { Put({0x08620040,value});CHECK(Call().kind==a32::ExitKind::Fallthrough && cpu.r[0]==0);CHECK(Read()==(IpcCommandBuffer{0x08620040,0})); }
    void Get(std::uint32_t expected) {
        const auto session=Session();const auto init=session->initialized();const auto sdk=session->sdk_version();
        const auto pid=session->process_id();const auto program=session->program_id();
        const auto handles=kernel.handles().OpenHandleCount();const auto time=kernel.now_ns();
        const auto threads=kernel.threads().size();const auto thread_priority=kernel.current_thread()->priority;
        // Stale words are not additional arguments when the header declares none.
        IpcCommandBuffer q{};q.fill(0xFEED1234);q[0]=0x08630000;
        Put(q);const auto before=cpu;
        CHECK(Call().kind==a32::ExitKind::Fallthrough && cpu.r[0]==0 && cpu.r[15]==0x259480);
        for(unsigned i=1;i<15;++i)CHECK(cpu.r[i]==before.r[i]);
        CHECK(cpu.cpsr==before.cpsr && cpu.fpscr==before.fpscr);
        CHECK(Read()==(IpcCommandBuffer{0x08630080,0,expected}));
        CHECK(router.last_request()==q && router.last_session_name()=="fs:USER");
        CHECK(!router.unsupported_request() && router.last_host_error().empty());
        CHECK(session->priority()==expected && endpoint->priority()==expected);
        CHECK(session->initialized()==init && session->sdk_version()==sdk);
        CHECK(session->process_id()==pid && session->program_id()==program);
        CHECK(kernel.handles().OpenHandleCount()==handles && kernel.now_ns()==time && kernel.threads().size()==threads);
        CHECK(kernel.current_thread()->priority==thread_priority && kernel.current_thread()->status==ThreadStatus::Running);
        CHECK(!kernel.current_thread()->pending_wake && endpoint->archives().size()==0);
    }
    void Stop(const IpcCommandBuffer& q) {
        const auto priority=endpoint->priority();const auto handles=kernel.handles().OpenHandleCount();
        Put(q);const auto before=cpu;CHECK(Call().kind==a32::ExitKind::Svc);
        CHECK(cpu.r==before.r && cpu.cpsr==before.cpsr && Read()==q && router.unsupported_request());
        CHECK(endpoint->priority()==priority && kernel.handles().OpenHandleCount()==handles);
    }
};
void StoredValueAndSessionScope() {
    Fixture f;f.Stop({0x08630000});f.Initialize();f.Get(0xFFFFFFFF);f.Get(0xFFFFFFFF);
    for(auto value:{0U,1U,0x30U,0x80000000U,0x12345678U,0xFFFFFFFFU}) {f.Set(value);f.Get(value);f.Get(value);}
    const auto first=f.handle;Handle copy=0;CHECK(f.kernel.DuplicateHandle(&copy,first)==0);
    const auto first_session=f.Session();f.Connect();const auto second=f.handle;CHECK(f.Session()!=first_session);
    f.Stop({0x08630000});f.Initialize();f.Get(0xFFFFFFFF);f.Set(7);
    f.handle=copy;CHECK(f.Session()==first_session);f.Get(7);
    CHECK(f.kernel.CloseHandle(first)==0);f.Get(7);f.Initialize();f.Get(7);
    CHECK(f.kernel.CloseHandle(copy)==0 && f.kernel.CloseHandle(second)==0);
    f.Connect();f.Stop({0x08630000});f.Initialize();f.Get(7);
    Fixture independent;independent.Initialize();independent.Get(0xFFFFFFFF);
}
void GuardsAndExhaustion() {
    Fixture f;f.Initialize();f.Set(0x31415926);
    for(auto q:{IpcCommandBuffer{0x08630040,1},IpcCommandBuffer{0x08630002,0x20,0},
                IpcCommandBuffer{0x08631000},IpcCommandBuffer{0x08630001},IpcCommandBuffer{0x08630080,0,0}})f.Stop(q);
    const IpcCommandBuffer q{0x08630000};
    for(bool partial:{false,true}) {
        f.Put(q);GuestMemory blocked;const auto n=partial?8U:unsigned(sizeof(q));
        CHECK(blocked.Map(f.cb(),n,partial?MemoryPermission::Read|MemoryPermission::Write:MemoryPermission::Read));
        CHECK(blocked.LoadBytes(f.cb(),{reinterpret_cast<const std::uint8_t*>(q.data()),n}));
        CHECK(f.Call(&blocked).kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer);
        std::uint32_t header=0;CHECK(blocked.Read32(f.cb(),&header) && header==q[0]);
        CHECK(f.Read()==q && f.endpoint->priority()==0x31415926);
    }
    while(f.kernel.handles().OpenHandleCount()<HandleTable::kMaxCount) {
        Handle h=0;if(f.kernel.DuplicateHandle(&h,kCurrentProcessPseudoHandle)!=0)throw std::runtime_error("early exhaustion");
    }
    f.Get(0x31415926); // A getter must still work with no free kernel handles.
}
}
int main() {
    StoredValueAndSessionScope();GuardsAndExhaustion();
    if(failures)return EXIT_FAILURE;
    std::cout<<"PASS: FS GetPriority returns stored shared u32 without defaults, allocations or scheduling changes\n";
    return EXIT_SUCCESS;
}
