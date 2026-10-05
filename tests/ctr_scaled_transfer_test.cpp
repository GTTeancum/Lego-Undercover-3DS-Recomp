#include "services/gsp_gpu_service.h"
#include "runtime/ctr_svc_bridge.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace lego::ctr;
namespace {
int failures=0;
#define CHECK(x) do { if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n"; ++failures;} } while(0)
constexpr auto RW=MemoryPermission::Read|MemoryPermission::Write;
constexpr std::uint32_t kPage=0x10000000, kInput=0x1F070800, kOutput=0x1F300000;
DisplayTransferRequest Request(unsigned iw=512,unsigned ih=400,unsigned ow=480,unsigned oh=400){
    return {kInput,kOutput,(ih<<16)|iw,(oh<<16)|ow,0x01001004};
}
struct Pattern { std::vector<std::uint8_t> tiled,expected; };
Pattern MakePattern(unsigned iw,unsigned ih,unsigned ow,unsigned oh){
    Pattern p; std::vector<std::uint8_t> linear(iw*ih*4);p.tiled.resize(linear.size());
    for(unsigned y=0;y<ih;++y)for(unsigned x=0;x<iw;++x){
        const unsigned pair=(y*(iw/2)+x/2)&65535U;
        const unsigned a=pair&255U,b=pair>>8;
        const auto at=4*(y*iw+x);
        linear[at]=static_cast<std::uint8_t>(x*17+y); // alpha ignored, deliberately distinct.
        linear[at+1]=static_cast<std::uint8_t>((x&1)?b:a);
        linear[at+2]=static_cast<std::uint8_t>(255U-((x&1)?b:a));
        linear[at+3]=static_cast<std::uint8_t>((x&1)?255U:a);
    }
    // Inverse Morton mapping, independent of production's x/y lookup tables.
    for(unsigned ty=0;ty<ih/8;++ty)for(unsigned tx=0;tx<iw/8;++tx)for(unsigned q=0;q<64;++q){
        unsigned x=tx*8,y=ty*8;
        for(unsigned bit=0;bit<3;++bit){x|=((q>>(2*bit))&1U)<<bit;y|=((q>>(2*bit+1))&1U)<<bit;}
        const auto src=4*(y*iw+x),dst=4*((ty*(iw/8)+tx)*64+q);
        std::copy_n(linear.begin()+src,4,p.tiled.begin()+dst);
    }
    p.expected.resize((ow/2)*oh*3);
    for(unsigned y=0;y<oh;++y)for(unsigned x=0;x<ow/2;++x)for(unsigned c=0;c<3;++c){
        const auto a=linear[4*(y*iw+2*x)+c+1],b=linear[4*(y*iw+2*x+1)+c+1];
        p.expected[3*(y*(ow/2)+x)+c]=static_cast<std::uint8_t>((unsigned(a)+b)/2);
    }
    return p;
}
void StagePixelsAndBounds(){
    auto bank=GpuVramBank::ReferenceZero();
    // 512x256 contains every pair of byte values; 512x512 reaches the 1 MiB input cap.
    for(auto dims:std::vector<std::array<unsigned,4>>{{512,400,480,400},{512,256,512,256},{512,512,512,512},{24,16,18,11},{8,8,2,1}}){
        auto q=Request(dims[0],dims[1],dims[2],dims[3]);auto pat=MakePattern(dims[0],dims[1],dims[2],dims[3]);
        CHECK(bank->Write(q.input-kGpuVramVirtualBase,pat.tiled));
        const auto before=std::vector<std::uint8_t>(bank->bytes().begin(),bank->bytes().end());
        DisplayTransferPlan p;const char* error=nullptr;
        CHECK(StageDisplayTransfer(q,bank.get(),p,error)&&!error);
        CHECK(p.output==pat.expected&&p.output_vram&&p.input_bytes==pat.tiled.size());
        CHECK(p.bytes==pat.expected.size()&&p.width==dims[2]/2&&p.height==dims[3]);
        CHECK(std::equal(before.begin(),before.end(),bank->bytes().begin()));
    }
    auto q=Request(8,8,8,8);q.input=kGpuVramVirtualBase+kGpuVramBytes-256;q.output=kGpuVramVirtualBase+kGpuVramBytes-256-96;
    DisplayTransferPlan p;const char* error=nullptr;
    CHECK(StageDisplayTransfer(q,bank.get(),p,error)); // Exact end and adjacent nonoverlap.
    q.input=kInput;q.output=kGpuVramVirtualBase+kGpuVramBytes-96;
    CHECK(StageDisplayTransfer(q,bank.get(),p,error));
    for(auto bad:std::vector<DisplayTransferRequest>{
        {kInput+1,kOutput,0x80008,0x80008,0x01001004},
        {kInput,kOutput+1,0x80008,0x80008,0x01001004},
        {kInput,0x14000000,0x80008,0x80008,0x01001004},
        {kInput,kInput+8,0x80008,0x80008,0x01001004},
        {kInput,kInput-88,0x80008,0x80008,0x01001004},
        {kGpuVramVirtualBase+kGpuVramBytes-248,kOutput,0x80008,0x80008,0x01001004},
        {kInput,kGpuVramVirtualBase+kGpuVramBytes-88,0x80008,0x80008,0x01001004},
        Request(16,16,0,16),Request(16,16,15,16),Request(16,16,18,16),Request(16,16,16,17),
        Request(16,16,16,0),Request(15,16,14,16),Request(16,15,16,15),Request(520,512,480,400),
        {kInput,kOutput,0xFFFFFFFF,0xFFFFFFFF,0x01001004}}){
        const auto original=p.output;const auto bytes=p.bytes;
        CHECK(!StageDisplayTransfer(bad,bank.get(),p,error)&&error&&p.output==original&&p.bytes==bytes);
    }
    for(auto flags:{0x01001005U,0x02001004U,0x00001004U,0x01001006U,0x01001024U,0x01001404U}){
        auto bad=Request();bad.flags=flags;CHECK(!StageDisplayTransfer(bad,bank.get(),p,error));
    }
    CHECK(!StageDisplayTransfer(Request(),nullptr,p,error));
}
struct Fixture {
    Kernel k;GuestMemory m;IpcRouter ipc;SvcBridge svc{k,&ipc};
    std::shared_ptr<GspGpuService> g=std::make_shared<GspGpuService>();
    std::shared_ptr<GpuVramBank> bank=GpuVramBank::ReferenceZero();
    Handle h{},event{},nonowner{};std::shared_ptr<EventObject> event_object;unsigned slot{};a32::GuestState cpu{};
    explicit Fixture(bool second=false):slot(second?1:0){
        CHECK(m.EnsureTlsMappings(k));CHECK(g->ConfigureVram(bank));CHECK(ipc.RegisterService("gsp::Gpu",g)==0);
        if(second)CHECK(ipc.ConnectToService(k,"gsp::Gpu",&nonowner)==0);
        CHECK(ipc.ConnectToService(k,"gsp::Gpu",&h)==0);CHECK(k.CreateEvent(&event,0)==0);
        event_object=std::dynamic_pointer_cast<EventObject>(k.handles().Get(event));
        Put({0x00130042,1,0,event});CHECK(Call().kind==a32::ExitKind::Fallthrough);
        Put({0x00160042,0,0,kCurrentProcessPseudoHandle});CHECK(Call().kind==a32::ExitKind::Fallthrough);
        CHECK(m.MapSharedServicePage(kPage,g->shared_memory(),RW));
        CHECK(m.Map(0x14000000,0x10000,RW));
        const auto p=MakePattern(512,400,480,400);CHECK(bank->Write(kInput-kGpuVramVirtualBase,p.tiled));
        std::vector<std::uint8_t> sentinel(300000,0xAB);CHECK(bank->Write(kOutput-kGpuVramVirtualBase,sentinel));
        Packet(0);Word(queue(),0x100);
    }
    unsigned queue()const{return 0x800+slot*0x200;}
    std::uint32_t cb()const{return k.current_thread()->tls_address+kIpcCommandBufferOffset;}
    void Put(const IpcCommandBuffer& c={0xC0000}){for(unsigned i=0;i<c.size();++i)CHECK(m.Write32(cb()+4*i,c[i]));for(unsigned i=0;i<16;++i)cpu.r[i]=0xAAAA0000+i;cpu.r[0]=h;cpu.r[15]=0x25947C;cpu.cpsr=0x60000010;}
    a32::ExecutionResult Call(GuestMemory* other=nullptr){return svc.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcSendSyncRequest},cpu,other?other:&m);}
    void Word(unsigned off,std::uint32_t w){CHECK(m.Write32(kPage+off,w));}
    std::uint32_t Word(unsigned off){std::uint32_t w=0;CHECK(m.Read32(kPage+off,&w));return w;}
    void Packet(unsigned n,DisplayTransferRequest q=Request(),std::uint32_t header=0x01000103){unsigned i=0;for(auto w:{header,q.input,q.output,q.input_size,q.output_size,q.flags,0U,0U})Word(queue()+32+n*32+4*i++,w);}
    auto Page()const{auto b=g->shared_memory()->bytes();return std::vector<std::uint8_t>(b.begin(),b.end());}
    auto Bank()const{auto b=bank->bytes();return std::vector<std::uint8_t>(b.begin(),b.end());}
    auto Regs()const{PicaGpuRegisters r{};for(unsigned i=0;i<r.size();++i)r[i]=*g->register_word(0x400000+i*4);return r;}
    void Stop(){Put();auto page=Page(),bank_before=Bank();auto regs=Regs();auto uploads=g->pica_uploads();auto before=cpu;auto signal=event_object->signaled();auto status=k.current_thread()->status;auto handles=k.handles().OpenHandleCount();
        CHECK(Call().kind==a32::ExitKind::Svc&&ipc.unsupported_request());CHECK(cpu.r==before.r&&cpu.cpsr==before.cpsr&&Word(queue())==((std::uint32_t(page[queue()+1])<<8)|page[queue()]|(std::uint32_t(page[queue()+2])<<16)|(std::uint32_t(page[queue()+3])<<24)));
        CHECK(Page()==page&&Bank()==bank_before&&Regs()==regs&&g->pica_uploads()==uploads);
        CHECK(event_object->signaled()==signal&&k.now_ns()==0&&k.current_thread()->status==status&&k.handles().OpenHandleCount()==handles);}
};
void DeviceCommitAndRealWaiter(){
    Fixture f;auto expected=MakePattern(512,400,480,400).expected;auto bank=f.Bank(),page=f.Page();auto regs=f.Regs();auto uploads=f.g->pica_uploads();
    CHECK(!f.m.IsMapped(kInput)&&!f.m.IsMapped(kOutput));
    // An unrelated CPU mapping at the same numeric VA must never be the device destination.
    CHECK(f.m.Map(kOutput,0x50000,RW));CHECK(f.m.Write32(kOutput,0x87654321));
    std::uint64_t val=0,token=0;std::uint32_t fault=0;CHECK(f.m.LoadExclusive(kOutput,4,&val,&token,&fault));
    CHECK(f.k.WaitSynchronization1(f.event,-1).blocked);CHECK(f.k.CloseHandle(f.event)==0);
    f.Put();CHECK(f.Call().kind==a32::ExitKind::Fallthrough&&f.cpu.r[0]==0);
    std::copy(expected.begin(),expected.end(),bank.begin()+kOutput-kGpuVramVirtualBase);CHECK(f.Bank()==bank);
    page[f.queue()]=1;page[f.queue()+1]=0;page[1]=1;page[12]=4;CHECK(f.Page()==page);
    regs[0x300]=(kInput-kGpuVramVirtualBase+kGpuVramPhysicalBase)>>3;regs[0x301]=(kOutput-kGpuVramVirtualBase+kGpuVramPhysicalBase)>>3;
    regs[0x302]=0x019001E0;regs[0x303]=0x01900200;regs[0x304]=0x01001004;regs[0x306]&=~1U;
    CHECK(f.Regs()==regs&&f.g->pica_uploads()==uploads&&f.k.now_ns()==0);
    CHECK(f.k.current_thread()->status==ThreadStatus::Ready&&f.k.current_thread()->pending_wake&&!f.event_object->signaled());
    CHECK(f.m.StoreExclusive(kOutput,4,0x87654321,token,&fault)==a32::ExclusiveStoreResult::Success);
}
void TransactionsAndDependencies(){
    Fixture f;f.Packet(1,Request(),4);f.Word(f.queue(),0x200);f.Stop();
    // Earlier fill overlaps only source tail beyond *output* length: still a dependency.
    unsigned p=f.queue()+32;for(unsigned i=0;i<8;++i)f.Word(p+4*i,0);
    f.Word(p,2);f.Word(p+4,kInput+600000);f.Word(p+8,0x87654321);f.Word(p+12,kInput+600008);f.Word(p+28,0x201);
    f.Packet(1);f.Stop();
    // Earlier VRAM transfer output must not feed a stale later transfer snapshot.
    f.Packet(0);auto dependent=Request(8,8,8,8);dependent.input=kOutput;dependent.output=0x1F500000;f.Packet(1,dependent);f.Stop();
    // The next read may be the older RGBA4-to-FCRAM path too.
    dependent.output=0x14000000;dependent.flags=0x4400;f.Packet(1,dependent);f.Stop();
    f.Packet(0);f.Word(f.queue(),0x100);
    for(auto header:{0x3400U,0x34U,0x10000U}){f.Word(f.slot*64,header);f.Stop();}
    // Protected IPC reply: transport error and no device effect.
    f.Word(0,0);f.Put();auto before=f.Bank();GuestMemory ro;CHECK(ro.Map(f.cb(),256,MemoryPermission::Read));const std::array<std::uint8_t,4> request{0,0,12,0};CHECK(ro.LoadBytes(f.cb(),request));
    CHECK(f.Call(&ro).kind==a32::ExitKind::Fallthrough&&f.cpu.r[0]==kResultInvalidPointer&&f.Bank()==before);
    // Response backed by GSP must be rejected even at a different VA.
    CHECK(f.m.MapSharedServicePage(0x15000000,f.g->shared_memory(),RW));f.k.current_thread()->tls_address=0x15000000;f.Stop();
}
void OrderedBatchesAndRing(){
    Fixture f(true);f.h=f.nonowner;auto q=Request(8,8,8,8);q.output=kOutput;
    for(unsigned i=0;i<15;++i){q.output=kOutput+96*i;f.Packet(i,q);}
    f.Word(f.queue(),0xF0E);f.Word(64,51);f.Put();CHECK(f.Call().kind==a32::ExitKind::Fallthrough);
    CHECK(f.Word(f.queue())==14&&f.Page()[65]==15&&f.Page()[127]==4&&f.Page()[76]==4);
    CHECK(!f.m.IsMapped(kOutput));
    Fixture stop;stop.Packet(0,Request(),0x01010103);stop.Packet(1,Request(),4);stop.Word(stop.queue(),0x200);stop.Put();
    CHECK(stop.Call().kind==a32::ExitKind::Fallthrough&&stop.Word(stop.queue())==0x10101); // Unsupported tail ineligible.
    Fixture full;for(;;){Handle h=0;auto result=full.k.DuplicateHandle(&h,full.event);if(result){CHECK(result==kResultOutOfHandles);break;}}
    full.Put();CHECK(full.Call().kind==a32::ExitKind::Fallthrough&&full.event_object->signaled());
    // VRAM transfer followed by independent PICA IRQ preserves both effects and order.
    Fixture mix;CHECK(mix.m.Write32(0x14001000,0x12345678)&&mix.m.Write32(0x14001004,0xF0010));
    const auto p=mix.queue()+64;for(unsigned i=0;i<8;++i)mix.Word(p+4*i,0);mix.Word(p,1);mix.Word(p+4,0x14001000);mix.Word(p+8,8);
    mix.Word(mix.queue(),0x200);mix.Put();CHECK(mix.Call().kind==a32::ExitKind::Fallthrough);
    CHECK(mix.Page()[1]==2&&mix.Page()[12]==4&&mix.Page()[13]==5&&*mix.g->register_word(0x400C10)==0x01001004);
}
}
int main(){StagePixelsAndBounds();DeviceCommitAndRealWaiter();TransactionsAndDependencies();OrderedBatchesAndRing();if(failures)return EXIT_FAILURE;std::cout<<"PASS: scaled RGBA8/RGB8 device transfer, all byte averages, crop, bounds, aliases and queue transactions\n";return EXIT_SUCCESS;}
