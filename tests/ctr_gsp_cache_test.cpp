#include "services/gsp_gpu_service.h"
#include <iostream>
#include <stdexcept>
using namespace lego::ctr;
namespace {
#define CHECK(x) do { if (!(x)) throw std::runtime_error("line " + std::to_string(__LINE__) + ": " #x); } while (0)
void Test() {
    Kernel k;GuestMemory m;IpcRouter ipc;
    constexpr auto RW=MemoryPermission::Read|MemoryPermission::Write;
    auto endpoint=std::make_shared<GspGpuService>();Handle h{};
    CHECK(m.EnsureTlsMappings(k)&&m.Map(0x14000000,8192,RW));
    CHECK(ipc.RegisterService("gsp::Gpu",endpoint)==0&&ipc.ConnectToService(k,"gsp::Gpu",&h)==0);
    auto session=std::dynamic_pointer_cast<ClientSessionObject>(k.handles().Get(h));
    auto gsp=std::dynamic_pointer_cast<GspGpuService>(session->service);CHECK(gsp);
    const auto cb=k.current_thread()->tls_address+0x80;
    auto put=[&](const IpcCommandBuffer& q){for(unsigned i=0;i<q.size();++i)CHECK(m.Write32(cb+4*i,q[i]));};
    auto get=[&]{IpcCommandBuffer q{};for(unsigned i=0;i<q.size();++i)CHECK(m.Read32(cb+4*i,&q[i]));return q;};
    auto stop=[&](IpcCommandBuffer q){put(q);CHECK(!ipc.SendSyncRequest(k,m,h));CHECK(get()==q);};
    for(unsigned i=0;i<8192;++i)CHECK(m.Write8(0x14000000+i,(i*59+77)&255));
    std::array<std::uint32_t,kPicaGpuWords> regs{};
    for(unsigned i=0;i<regs.size();++i)regs[i]=*gsp->register_word(0x400000+4*i);
    const auto page=gsp->shared_memory();std::vector<std::uint8_t> before(page->bytes().begin(),page->bytes().end());
    const auto now=k.now_ns(),handles=k.handles().OpenHandleCount();
    std::uint64_t value{},epoch{};std::uint32_t fault{};
    CHECK(m.LoadExclusive(0x14000000,4,&value,&epoch,&fault));
    Handle copy{};CHECK(k.DuplicateHandle(&copy,kCurrentProcessPseudoHandle)==0);
    for(auto process:{kCurrentProcessPseudoHandle,copy}) for(auto size:{0U,1U,31U,32U,128U,2048U,8192U}){
        put({0x00080082,0x14000000,size,0,process});CHECK(ipc.SendSyncRequest(k,m,h)==0);CHECK(get()==IpcCommandBuffer({0x00080040,0}));}
    CHECK(k.CloseHandle(copy)==0);
    put({0x00080082,0x14000001,127,0,kCurrentProcessPseudoHandle});CHECK(ipc.SendSyncRequest(k,m,h)==0);
    put({0x00080082,0xFFFFFFFF,0,0,kCurrentProcessPseudoHandle});CHECK(ipc.SendSyncRequest(k,m,h)==0);
    for(unsigned i=0;i<8192;++i){std::uint8_t b{};CHECK(m.Read8(0x14000000+i,&b)&&b==((i*59+77)&255));}
    std::uint64_t v{},ep{};CHECK(m.LoadExclusive(0x14000000,4,&v,&ep,&fault)&&v==value&&ep==epoch);
    CHECK(std::equal(before.begin(),before.end(),page->bytes().begin()));
    for(unsigned i=0;i<regs.size();++i)CHECK(regs[i]==gsp->register_word(0x400000+4*i));
    CHECK(k.now_ns()==now&&k.handles().OpenHandleCount()==handles&&!gsp->rights_held()&&!gsp->registered());
    CHECK(m.Map(0x08000000,8192,RW)&&m.MapUserAlias(0x0E000000,0x08000000,8192,3)==0);
    CHECK(m.ProtectUserAlias(0x08000000,8192,0)==0);
    put({0x00080082,0x0E000000,8192,0,kCurrentProcessPseudoHandle});CHECK(ipc.SendSyncRequest(k,m,h)==0);
    stop({0x00080082,0x08000000,1,0,kCurrentProcessPseudoHandle});
    CHECK(m.Map(0x15000000,4096,MemoryPermission::Read));
    put({0x00080082,0x15000000,4096,0,kCurrentProcessPseudoHandle});CHECK(ipc.SendSyncRequest(k,m,h)==0);
    for(auto [a,n]:{std::pair{0U,1U},{0x14001FFFU,2U},{0xFFFFFFFFU,2U},{0x14000000U,0xFFFFFFFFU}})
        stop({0x00080082,a,n,0,kCurrentProcessPseudoHandle});
    CHECK(m.MapSharedServicePage(0x10000000,page,RW));
    stop({0x00080082,0x10000000,1,0,kCurrentProcessPseudoHandle});
    for(auto bad:{0U,kCurrentThreadPseudoHandle,h,0x12345678U}) {
        const IpcCommandBuffer q{0x00080082,0x14000000,128,0,bad};put(q);
        CHECK(ipc.SendSyncRequest(k,m,h)==kResultInvalidHandle&&get()==q);
    }
    Handle foreign{};CHECK(k.handles().Create(&foreign,std::make_shared<ProcessObject>(k.current_process()->process_id))==0);
    stop({0x00080082,0x14000000,128,0,foreign});
    for(auto header:{0x00080000U,0x00080080U,0x000800C2U,0x00090082U})
        stop({header,0x14000000,128,0,kCurrentProcessPseudoHandle});
    for(auto desc:{1U,16U,0x04000000U}) stop({0x00080082,0x14000000,128,desc,kCurrentProcessPseudoHandle});
    IpcCommandBuffer q{0x00080082,0x14000000,128,0,kCurrentProcessPseudoHandle};
    CHECK(!endpoint->CanHandle(q));
    GuestMemory ro;CHECK(ro.Map(cb,sizeof(q),MemoryPermission::Read));
    CHECK(ro.LoadBytes(cb,{reinterpret_cast<const std::uint8_t*>(q.data()),sizeof(q)}));
    CHECK(ipc.SendSyncRequest(k,ro,h)==kResultInvalidPointer);
}
}
int main(){try{Test();}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
std::cout<<"PASS: bounded GSP cacheless flush, process identity, private aliases and unchanged GPU/queue/time/reservations\n";}
