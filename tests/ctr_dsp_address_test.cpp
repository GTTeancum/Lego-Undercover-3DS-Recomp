#include "dsp_live_fixture.h"
#include <iostream>
using namespace lego::ctr;
namespace {
#define CHECK(x) do{if(!(x))throw std::runtime_error("line "+std::to_string(__LINE__)+": " #x);}while(0)
void Test(){
    Kernel k;GuestMemory m;IpcRouter ipc;auto endpoint=std::make_shared<DspDiscoveryService>(DspSpecialConfig{},DspProbeOptions{true,DspProbeReset::KnownOnly,100,true});
    CHECK(m.EnsureTlsMappings(k));CHECK(m.Map(0x08000000,4096,MemoryPermission::Read|MemoryPermission::Write));auto input=dsp_live_fixture::Container();CHECK(m.LoadBytes(0x08000000,input));
    Handle h{};CHECK(ipc.RegisterService("dsp::DSP",endpoint)==0&&ipc.ConnectToService(k,"dsp::DSP",&h)==0);
    const auto cb=k.current_thread()->tls_address+0x80;
    auto put=[&](const IpcCommandBuffer& q){for(unsigned i=0;i<q.size();++i)CHECK(m.Write32(cb+4*i,q[i]));};
    auto get=[&]{IpcCommandBuffer q{};for(unsigned i=0;i<q.size();++i)CHECK(m.Read32(cb+4*i,&q[i]));return q;};
    auto n=static_cast<std::uint32_t>(input.size());put({0x001100C2,n,255,255,(n<<4)|10,0x08000000});CHECK(ipc.SendSyncRequest(k,m,h)==0);
    const auto* p=endpoint->execution_probe();const auto before=p->summary();std::vector<std::uint8_t> bank(p->memory().begin(),p->memory().end()),known(p->provenance().begin(),p->provenance().end());
    const auto now=k.now_ns(),count=k.handles().OpenHandleCount();
    // Exhaust the bounded 17-bit word-address space. Unknown words can be
    // translated, but translation must never initialize them or dereference them.
    for(std::uint32_t a=0;a<0x20000;++a){IpcCommandBuffer q{0x000C0040,a};CHECK(endpoint->CanHandle(q));
        CHECK(endpoint->Handle(ipc,k,m,*k.current_thread(),q)==0);CHECK(q==IpcCommandBuffer({0x000C0080,0,DspLiveDevice::DataAddress+2*a}));}
    CHECK(std::equal(bank.begin(),bank.end(),p->memory().begin())&&std::equal(known.begin(),known.end(),p->provenance().begin()));
    CHECK(p->summary()==before&&now==k.now_ns()&&count==k.handles().OpenHandleCount());
    for(auto a:{0U,0xBFFFU,0x1BFFFU,0x1FFFFU}){put({0x000C0040,a});CHECK(ipc.SendSyncRequest(k,m,h)==0);CHECK(get()==IpcCommandBuffer({0x000C0080,0,DspLiveDevice::DataAddress+2*a}));}
    for(auto a:{0x20000U,0x80000000U,0xFFFFFFFFU}){IpcCommandBuffer q{0x000C0040,a};put(q);CHECK(!ipc.SendSyncRequest(k,m,h)&&get()==q);}
    for(auto header:{0x000C0000U,0x000C0080U,0x000C0042U}){IpcCommandBuffer q{header,7};put(q);CHECK(!ipc.SendSyncRequest(k,m,h)&&get()==q);}
    IpcCommandBuffer q{0x000C0040,0xBFFF};GuestMemory ro;CHECK(ro.Map(cb,256,MemoryPermission::Read));CHECK(ro.LoadBytes(cb,{reinterpret_cast<const std::uint8_t*>(q.data()),sizeof(q)}));
    CHECK(ipc.SendSyncRequest(k,ro,h)==kResultInvalidPointer);
    CHECK(p->summary()==before&&now==k.now_ns());CHECK(!m.IsReadable(DspLiveDevice::DataAddress+0x38000,2));
}
}
int main(){try{Test();}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
std::cout<<"PASS: complete bounded DSP DATA word-address translation, no initialization, no wrap and protected replies\n";}
