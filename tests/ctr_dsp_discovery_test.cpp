#include "runtime/ctr_runner.h"
#include "services/dsp_discovery_service.h"
#include <cstdlib>
#include <iostream>
namespace {
using namespace lego::ctr;
int failures=0;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}}while(0)
void DiscoveryAndUntouchedRequests(){
    const a32::Registry registry{};Kernel kernel;GuestMemory memory;NativeRunner runner(registry,memory,kernel);
    CHECK(memory.EnsureTlsMappings(kernel));auto& router=runner.ipc();Handle dsp=0;
    CHECK(router.HasService("dsp::DSP") && router.ConnectToService(kernel,"dsp::DSP",&dsp)==0);
    const auto obj=std::dynamic_pointer_cast<ClientSessionObject>(kernel.handles().Get(dsp));
    CHECK(obj && std::dynamic_pointer_cast<DspDiscoveryService>(obj->service));
    Handle copy=0;CHECK(kernel.DuplicateHandle(&copy,dsp)==0);
    CHECK(kernel.handles().Get(copy)==kernel.handles().Get(dsp));
    const auto count=kernel.handles().OpenHandleCount();const auto time=kernel.now_ns();
    SvcBridge bridge(kernel,&router);const auto cb=kernel.current_thread()->tls_address+kIpcCommandBufferOffset;
    for(const auto header:{0x00160000U,0x001100C2U,0x00150082U,0x001F0000U,0xFFFF0FFFU}){
      IpcCommandBuffer q{};q.fill(0xF1234567);q[0]=header;
      for(unsigned i=0;i<q.size();++i)CHECK(memory.Write32(cb+i*4,q[i]));
      a32::GuestState cpu{};for(unsigned i=0;i<16;++i)cpu.r[i]=0x98760000+i;
      cpu.r[0]=dsp;cpu.r[15]=0x25947C;cpu.cpsr=0xA0000010;const auto before=cpu;
      const auto stop=bridge.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcSendSyncRequest},cpu,&memory);
      CHECK(stop.kind==a32::ExitKind::Svc && cpu.r==before.r && cpu.cpsr==before.cpsr);
      CHECK(router.unsupported_request() && router.last_session_name()=="dsp::DSP" && router.last_request()==q);
      for(unsigned i=0;i<q.size();++i){std::uint32_t w=0;CHECK(memory.Read32(cb+i*4,&w)&&w==q[i]);}
      CHECK(count==kernel.handles().OpenHandleCount() && time==kernel.now_ns() && kernel.threads().size()==1);
    }
}
}
int main(){DiscoveryAndUntouchedRequests();if(failures)return EXIT_FAILURE;
 std::cout<<"PASS: DSP discovery only, no successful commands, output data, events or completion\n";return EXIT_SUCCESS;}
