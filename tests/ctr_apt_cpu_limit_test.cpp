#include "services/apt_service.h"
#include "services/gsp_gpu_service.h"
#include "runtime/ctr_runner.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using namespace lego::ctr;
int failures{};
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL " << __LINE__ << ": " #x "\n"; ++failures; } } while (0)
struct Fixture {
    Kernel kernel; GuestMemory memory; IpcRouter router; SvcBridge bridge{kernel,&router};
    std::shared_ptr<AptService> apt=std::make_shared<AptService>();
    Handle session{},resource_handle{};
    std::shared_ptr<ResourceLimitObject> resource;
    std::shared_ptr<EventObject> notification,parameter;
    a32::GuestState cpu{};
    Fixture() {
        CHECK(memory.EnsureTlsMappings(kernel));
        CHECK(router.RegisterService("APT:U",apt)==0);
        CHECK(router.ConnectToService(kernel,"APT:U",&session)==0);
        CHECK(kernel.GetResourceLimit(&resource_handle,kCurrentProcessPseudoHandle)==0);
        resource=std::dynamic_pointer_cast<ResourceLimitObject>(kernel.handles().Get(resource_handle));
        if (!resource) throw std::runtime_error("resource missing");
    }
    std::uint32_t Address()const {return kernel.current_thread()->tls_address+kIpcCommandBufferOffset;}
    void Put(const IpcCommandBuffer& q) {
        for (unsigned i=0;i<q.size();++i) CHECK(memory.Write32(Address()+4*i,q[i]));
        for (unsigned i=0;i<16;++i) cpu.r[i]=0x11220000+i;
        cpu.r[0]=session;cpu.r[15]=0x25947C;cpu.cpsr=0x60000010;
        cpu.fpscr=0x83000018;cpu.vfp[3]=0x12345678;
    }
    IpcCommandBuffer Read(GuestMemory* m=nullptr) {
        IpcCommandBuffer q{};
        for (unsigned i=0;i<q.size();++i) CHECK((m?m:&memory)->Read32(Address()+4*i,&q[i]));
        return q;
    }
    a32::ExecutionResult Call(GuestMemory* m=nullptr) {
        return bridge.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcSendSyncRequest},cpu,m?m:&memory);
    }
    void Initialize() {
        Put({0x00020080,0x300,0});CHECK(Call().kind==a32::ExitKind::Fallthrough && cpu.r[0]==0);
        const auto q=Read();
        notification=std::dynamic_pointer_cast<EventObject>(kernel.handles().Get(q[3]));
        parameter=std::dynamic_pointer_cast<EventObject>(kernel.handles().Get(q[4]));
        CHECK(notification && parameter);
    }
    void Reply(const IpcCommandBuffer& q,const IpcCommandBuffer& reply) {
        const auto time=kernel.now_ns(),handles=kernel.handles().OpenHandleCount(),threads=kernel.threads().size();
        const auto priority=kernel.current_thread()->priority;
        const auto deadline=kernel.NextWakeDeadline();
        const auto pending=kernel.current_thread()->pending_wake;
        const auto ns=notification->signaled(),ps=parameter->signaled();
        std::array<std::int32_t,10> before_current{},before_limits{};
        for(unsigned i=0;i<10;++i){before_current[i]=resource->Current(static_cast<ResourceLimitType>(i));before_limits[i]=resource->Limit(static_cast<ResourceLimitType>(i));}
        Put(q);const auto before=cpu;
        CHECK(Call().kind==a32::ExitKind::Fallthrough && cpu.r[0]==0);
        auto expected=before.r;expected[0]=0;expected[15]+=4;
        CHECK(cpu.r==expected && cpu.vfp==before.vfp && cpu.cpsr==before.cpsr && cpu.fpscr==before.fpscr);
        CHECK(Read()==reply && router.last_request()==q && !router.unsupported_request());
        CHECK(kernel.now_ns()==time && kernel.handles().OpenHandleCount()==handles && kernel.threads().size()==threads);
        CHECK(kernel.current_thread()->priority==priority && kernel.current_thread()->pending_wake==pending);
        CHECK(kernel.NextWakeDeadline()==deadline && notification->signaled()==ns && parameter->signaled()==ps);
        for(unsigned i=0;i<10;++i){
            CHECK(resource->Limit(static_cast<ResourceLimitType>(i))==before_limits[i]);
            if(i!=9)CHECK(resource->Current(static_cast<ResourceLimitType>(i))==before_current[i]);
        }
        CHECK(apt->initialized() && apt->registered() && apt->pending_parameter().has_value());
    }
    void Stop(const IpcCommandBuffer& q) {
        const auto current=kernel.app_cpu_time_current();const auto guard=kernel.app_cpu_core0_only();
        const auto time=kernel.now_ns(),handles=kernel.handles().OpenHandleCount();
        Put(q);const auto before=cpu;
        CHECK(Call().kind==a32::ExitKind::Svc && router.unsupported_request());
        CHECK(Read()==q && cpu.r==before.r && cpu.vfp==before.vfp && cpu.cpsr==before.cpsr);
        CHECK(kernel.app_cpu_time_current()==current && kernel.app_cpu_core0_only()==guard);
        CHECK(kernel.now_ns()==time && kernel.handles().OpenHandleCount()==handles);
    }
};
void ResourceReadback() {
    Fixture f;f.Initialize();CHECK(f.kernel.app_cpu_time_current()==0 && f.kernel.app_cpu_time_maximum()==80);
    f.Reply({0x00500040,1},{0x00500080,0,0});
    // Every accepted percentage is an assignment, not a cumulative reservation.
    for(unsigned value=0;value<=80;++value){
        f.Reply({0x004F0080,1,value,0xDEADBEEF},{0x004F0040,0});
        CHECK(f.resource->Current(ResourceLimitType::CpuTime)==static_cast<int>(value));
        CHECK(f.kernel.app_cpu_core0_only() && f.kernel.AppCpuExecutionSupported());
        f.Reply({0x00500040,1,0xFFFFFFFF},{0x00500080,0,value});
    }
    f.Reply({0x004F0080,1,30},{0x004F0040,0});
    for(auto value:{81U,100U,0x7FFFFFFFU,0x80000000U,0xFFFFFFFFU}){
        f.Reply({0x004F0080,1,value},{0x004F0040,0});
        CHECK(f.kernel.app_cpu_time_current()==30);
    }
    // The pre-existing SVC resource handle exposes exactly the same object/value.
    constexpr std::uint32_t base=0x08000000;
    CHECK(f.memory.Map(base,0x1000,MemoryPermission::Read|MemoryPermission::Write));
    CHECK(f.memory.Write32(base,9));
    CHECK(f.kernel.GetResourceLimitValues(&f.memory,true,base+16,f.resource_handle,base,1)==0);
    std::uint32_t lo{},hi{};CHECK(f.memory.Read32(base+16,&lo) && f.memory.Read32(base+20,&hi));CHECK(lo==30 && hi==0);
    CHECK(f.kernel.GetResourceLimitValues(&f.memory,false,base+16,f.resource_handle,base,1)==0);
    CHECK(f.memory.Read32(base+16,&lo) && lo==80);
    Handle duplicate{};CHECK(f.kernel.DuplicateHandle(&duplicate,f.resource_handle)==0);
    CHECK(f.kernel.handles().Get(duplicate)==f.resource);
    // A second connection shares the kernel resource, not a per-session number.
    CHECK(f.router.ConnectToService(f.kernel,"APT:U",&f.session)==0);
    f.Reply({0x00500040,1},{0x00500080,0,30});
    Fixture other;other.Initialize();other.Reply({0x00500040,1},{0x00500080,0,0});
}
void RequestGuards() {
    Fixture f;f.Stop({0x004F0080,1,30});f.Stop({0x00500040,1});f.Initialize();
    for(const auto q:{IpcCommandBuffer{0x004F0000,1,30},IpcCommandBuffer{0x004F0082,1,30},
                     IpcCommandBuffer{0x004F0080,0,30},IpcCommandBuffer{0x004F0080,2,30},
                     IpcCommandBuffer{0x00500080,1},IpcCommandBuffer{0x00500040,0}})f.Stop(q);
    for(const auto q:{IpcCommandBuffer{0x004F0080,1,30},IpcCommandBuffer{0x00500040,1}}){
        for(unsigned kind=0;kind<3;++kind){
            f.Put(q);GuestMemory blocked;
            const unsigned size=kind==1?8:sizeof(q);
            const auto perm=kind==0?MemoryPermission::Read:kind==1?MemoryPermission::Read|MemoryPermission::Write:MemoryPermission::Write;
            CHECK(blocked.Map(f.Address(),size,perm));
            CHECK(blocked.LoadBytes(f.Address(),{reinterpret_cast<const std::uint8_t*>(q.data()),size}));
            CHECK(f.Call(&blocked).kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer);
            CHECK(f.kernel.app_cpu_time_current()==0 && !f.kernel.app_cpu_core0_only());
        }
    }
    // No allocation is needed even when all guest handle slots are exhausted.
    for(;;){Handle h{};if(f.kernel.handles().Create(&h,std::make_shared<GenericObject>(KernelObject::Type::Other))!=0)break;}
    f.Reply({0x004F0080,1,30},{0x004F0040,0});
    f.Reply({0x00500040,1},{0x00500080,0,30});
}
void NonCore0Safety() {
    Fixture f;f.Initialize();Handle t{};
    CHECK(f.kernel.CreateThread(&t,0x100000,0,0x0FFE0000,40,1)==0);
    f.Stop({0x004F0080,1,30});CHECK(!f.router.last_host_error().empty());
    // The reference over-maximum path is a genuine no-op, even here.
    f.Reply({0x004F0080,1,81},{0x004F0040,0});CHECK(!f.kernel.app_cpu_core0_only());
    auto thread=std::dynamic_pointer_cast<ThreadObject>(f.kernel.handles().Get(t));
    CHECK(thread);thread->status=ThreadStatus::Dead;
    f.Reply({0x004F0080,1,30},{0x004F0040,0});
    for(unsigned processor=1;processor<=3;++processor){
        auto cpu=f.cpu;cpu.r[0]=40;cpu.r[1]=0x100000;cpu.r[2]=0;cpu.r[3]=0x0FFD0000;cpu.r[4]=processor;cpu.r[15]=0x100ABC;
        const auto before=cpu;const auto handles=f.kernel.handles().OpenHandleCount(),threads=f.kernel.threads().size();
        const auto stop=f.bridge.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcCreateThread},cpu,&f.memory);
        CHECK(stop.kind==a32::ExitKind::Svc && stop.pc==before.r[15] && cpu.r==before.r);
        CHECK(f.kernel.handles().OpenHandleCount()==handles && f.kernel.threads().size()==threads);
    }
    for(auto processor:{0,kThreadProcessorDefault,kThreadProcessorAll}){
        CHECK(f.kernel.AppCpuThreadCreationSupported(processor));
        Handle h{};CHECK(f.kernel.CreateThread(&h,0x100000,0,0x0FFE0000,40,processor)==0);
        CHECK(std::dynamic_pointer_cast<ThreadObject>(f.kernel.handles().Get(h))->processor_id==0);
    }
    // Also catch an externally created core-1 thread before any guest dispatch or idle clock pump.
    const a32::Registry registry{};NativeRunner runner(registry,f.memory,f.kernel);
    CHECK(runner.InitializeMainThread());
    CHECK(f.kernel.CreateThread(&t,0x100000,0,0x0FFE0000,40,1)==0);
    CHECK(!f.kernel.AppCpuExecutionSupported());const auto before=runner.live_state();const auto time=f.kernel.now_ns();
    const auto stop=runner.Run(10,10);
    CHECK(stop.reason==RunnerStopReason::UnsupportedCpuExecution && stop.dispatch_rounds==0);
    CHECK(runner.live_state().r==before.r && f.kernel.now_ns()==time);
}
void UnimplementedMemoryOperationsStop() {
    Fixture f;f.Initialize();f.Reply({0x004F0080,1,30},{0x004F0040,0});
    constexpr std::uint32_t source=0x08045000,destination=0x0E000000,size=0x8000;
    CHECK(f.memory.Map(source,size,MemoryPermission::Read|MemoryPermission::Write));
    CHECK(f.memory.Write32(source,0x12345678));
    const auto commit=f.resource->Current(ResourceLimitType::Commit);
    for(const auto op:{0x104U,5U,6U,1U,0xFFFFFFFFU}) {
        auto cpu=f.cpu;cpu.r[0]=op;cpu.r[1]=destination;cpu.r[2]=source;cpu.r[3]=size;cpu.r[4]=3;cpu.r[15]=0x25C9F4;
        const auto before=cpu;
        const auto stop=f.bridge.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcControlMemory},cpu,&f.memory);
        CHECK(stop.kind==a32::ExitKind::Svc && stop.pc==before.r[15] && cpu.r==before.r);
        CHECK(f.resource->Current(ResourceLimitType::Commit)==commit && !f.memory.IsReadable(destination,size));
        std::uint32_t word{};CHECK(f.memory.Read32(source,&word) && word==0x12345678);
    }
}
void GpuIsNotTouched() {
    Fixture f;f.Initialize();auto gsp=std::make_shared<GspGpuService>();auto bank=GpuVramBank::ReferenceZero();
    std::vector<std::uint8_t> sentinel(kGpuVramBytes);for(std::size_t n=0;n<sentinel.size();++n)sentinel[n]=(n*13+47)&255;
    CHECK(bank->Write(0,sentinel));CHECK(gsp->ConfigureVram(bank));CHECK(f.router.RegisterService("gsp::Gpu",gsp)==0);
    const auto uploads=gsp->pica_uploads();std::vector<std::uint32_t> regs;
    for(unsigned i=0;i<kPicaGpuWords;++i)regs.push_back(gsp->register_word(0x400000+4*i).value());
    const auto page=gsp->shared_memory();std::vector<std::uint8_t> old_page(page->bytes().begin(),page->bytes().end());
    f.Reply({0x004F0080,1,30},{0x004F0040,0});
    CHECK(gsp->pica_uploads()==uploads && std::equal(bank->bytes().begin(),bank->bytes().end(),sentinel.begin()));
    CHECK(std::equal(page->bytes().begin(),page->bytes().end(),old_page.begin()));
    for(unsigned i=0;i<kPicaGpuWords;++i)CHECK(gsp->register_word(0x400000+4*i).value()==regs[i]);
}
}
int main(){ResourceReadback();RequestGuards();NonCore0Safety();UnimplementedMemoryOperationsStop();GpuIsNotTouched();if(failures)return EXIT_FAILURE;
    std::cout<<"PASS: actual application CPU resource readback, bounded core-0 support and explicit core-1 stops\n";return EXIT_SUCCESS;}
