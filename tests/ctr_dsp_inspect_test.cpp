#include "services/dsp_discovery_service.h"
#include "runtime/ctr_svc_bridge.h"
#include "dsp1_test_fixture.h"
#include <cstdlib>
#include <iostream>
namespace {
using namespace lego::ctr;using namespace dsp_fixture;
int failures=0;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}}while(0)
struct Fixture {
    Kernel kernel;GuestMemory memory;IpcRouter router;SvcBridge bridge{kernel,&router};
    std::shared_ptr<DspDiscoveryService> service=std::make_shared<DspDiscoveryService>();
    Handle handle{};a32::GuestState cpu{};std::vector<std::uint8_t> image=Image(true);
    static constexpr std::uint32_t address=0x08000000;
    std::uint32_t cb() const{return kernel.current_thread()->tls_address+kIpcCommandBufferOffset;}
    Fixture(){CHECK(memory.EnsureTlsMappings(kernel));CHECK(memory.Map(address,0x1000,MemoryPermission::Read));
        CHECK(memory.LoadBytes(address,image));CHECK(router.RegisterService("dsp::DSP",service)==0);
        CHECK(router.ConnectToService(kernel,"dsp::DSP",&handle)==0);}
    IpcCommandBuffer Request(){const auto n=static_cast<std::uint32_t>(image.size());
        return {0x001100C2,n,0xFACE00FF,0x000E00FF,(n<<4)|0xA,address};}
    void Put(const IpcCommandBuffer& q){for(unsigned i=0;i<q.size();++i)CHECK(memory.Write32(cb()+4*i,q[i]));
        for(unsigned i=0;i<16;++i)cpu.r[i]=0x11220000+i;
        cpu.r[0]=handle;cpu.r[15]=0x25947C;cpu.cpsr=0xA0000010;cpu.fpscr=0x23000010;}
    IpcCommandBuffer Read(){IpcCommandBuffer q{};for(unsigned i=0;i<q.size();++i)CHECK(memory.Read32(cb()+4*i,&q[i]));return q;}
    a32::ExecutionResult Call(GuestMemory* m=nullptr){return bridge.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcSendSyncRequest},cpu,m?m:&memory);}
    void Stop(const IpcCommandBuffer& q){Put(q);const auto before=cpu;const auto handles=kernel.handles().OpenHandleCount();
        const auto time=kernel.now_ns();CHECK(Call().kind==a32::ExitKind::Svc);
        CHECK(cpu.r==before.r && cpu.cpsr==before.cpsr && cpu.fpscr==before.fpscr);
        CHECK(Read()==q && router.unsupported_request() && kernel.now_ns()==time);
        CHECK(kernel.handles().OpenHandleCount()==handles && kernel.threads().size()==1);
        CHECK(kernel.current_thread()->status==ThreadStatus::Running && !kernel.current_thread()->pending_wake);
    }
};
void InspectPreservesGuestState(){
    Fixture f;CHECK(!f.service->inspected_image());f.Stop(f.Request());
    const auto* p=f.service->inspected_image();CHECK(p && p->special.required && p->segments.size()==3);
    CHECK(!f.router.last_host_error().empty());
    for(unsigned i=0;i<f.image.size();++i){std::uint8_t v=0;CHECK(f.memory.Read8(f.address+i,&v) && v==f.image[i]);}
    CHECK(!f.memory.IsMapped(0x1FF00000)); // No DSP RAM mapped by inspection.
    const auto prior_hash=p->component_sha256;
    Handle second=0;CHECK(f.router.ConnectToService(f.kernel,"dsp::DSP",&second)==0);f.handle=second;
    f.Stop(f.Request());CHECK(f.service->inspected_image()->component_sha256==prior_hash);
}
void RejectionAndProtectedResponse(){
    Fixture f;f.Stop(f.Request());const auto* previous=f.service->inspected_image();
    f.image[0x300]^=1;CHECK(f.memory.LoadBytes(f.address,f.image));f.Stop(f.Request());
    CHECK(f.service->inspected_image()==previous);
    for(auto location:{0x09000000U,0xFFFFFFF0U,0x08000FF0U}){auto q=f.Request();q[5]=location;f.Stop(q);CHECK(f.service->inspected_image()==previous);}
    for(auto index:{0U,1U,2U,3U,4U}){auto q=f.Request();q[index]^=1;f.Stop(q);CHECK(f.service->inspected_image()==previous);}
    for(auto header:{0x00120000U,0x00160000U,0x00010040U}){auto q=f.Request();q[0]=header;f.Stop(q);CHECK(f.service->inspected_image()==previous);}
    for(auto permissions:{MemoryPermission::Read,MemoryPermission::Write}){
        const auto q=f.Request();f.Put(q);GuestMemory blocked;
        CHECK(blocked.Map(f.cb(),sizeof(q),permissions));
        CHECK(blocked.LoadBytes(f.cb(),{reinterpret_cast<const std::uint8_t*>(q.data()),sizeof(q)}));
        CHECK(f.Call(&blocked).kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer);
        CHECK(f.service->inspected_image()==previous);
    }
}
}
int main(){InspectPreservesGuestState();RejectionAndProtectedResponse();if(failures)return EXIT_FAILURE;
 std::cout<<"PASS: DSP image inspection leaves original request, registers, memory, time and handles untouched; no load-complete response\n";return EXIT_SUCCESS;}
