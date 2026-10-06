#include "services/dsp_execution_probe.h"
#include "services/dsp_discovery_service.h"
#include "runtime/ctr_svc_bridge.h"
#include "dsp1_test_fixture.h"
#include "../vendor/teakra-3d697a1/src/shared_memory.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>

static bool fail_next=false;
void* operator new(std::size_t n) {
    if(fail_next){fail_next=false;throw std::bad_alloc();}
    if(auto p=std::malloc(n?n:1))return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
namespace {
using namespace lego::ctr;
int failures{};
#define CHECK(x) do{if(!(x)){++failures;std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";}}while(0)
std::vector<std::uint8_t> Bytes(std::initializer_list<std::uint16_t> words) {
    std::vector<std::uint8_t> b;for(auto w:words){b.push_back(w&255);b.push_back(w>>8);}return b;
}
Dsp1Image Image(std::initializer_list<std::uint16_t> words,bool replies=false) {
    Dsp1Image image;image.receive_startup_replies=replies;const auto b=Bytes(words);
    CHECK(image.program.Stage(0,b));return image;
}
std::unique_ptr<DspExecutionProbe> Probe(const Dsp1Image& image,DspProbeReset reset=DspProbeReset::KnownOnly) {
    const char* error=nullptr;auto p=DspExecutionProbe::Create(image,reset,error);
    if(!p)throw std::runtime_error(error?error:"probe creation failed");return p;
}
void KnownWritesAndFaults() {
    // Real Teak instructions: mov #0x7B,a0l; mov a0l,[0x100]; mov [0x100],a0.
    auto image=Image({0x217B,0xD4BC,0x0100,0xD4B8,0x0100});
    auto p=Probe(image);const auto before=p->summary();
    CHECK(p->Advance(0)==DspProbeState::Paused && p->summary()==before);
    CHECK(p->Advance(3)==DspProbeState::Paused);
    CHECK(p->summary().completed_steps==3 && p->summary().written_words==1);
    CHECK(p->memory()[0x40200]==0x7B && p->memory()[0x40201]==0);
    CHECK(p->provenance()[0x40200]==3 && p->provenance()[0x40201]==3);
    CHECK(p->provenance()[0x401FF]==0 && p->provenance()[0x40202]==0);
    CHECK(!image.data.Read(0x200)); // Original staging image is immutable.
    CHECK(p->Advance(10)==DspProbeState::Fault);
    CHECK(p->summary().fault==DspProbeFault::UnknownSram && p->summary().fault_address==10);
    CHECK(p->summary().completed_steps==3 && p->summary().attempted_steps==4);
    auto sealed=p->summary();auto memory=std::vector(p->memory().begin(),p->memory().end());
    CHECK(p->Advance(100)==DspProbeState::Fault && p->summary()==sealed);
    CHECK(std::equal(memory.begin(),memory.end(),p->memory().begin()));
    auto read=Image({0xD4B8,0x0100});p=Probe(read);
    CHECK(p->Advance(1)==DspProbeState::Fault && p->summary().fault_address==0x40200);
    CHECK(p->summary().completed_steps==0 && p->summary().pc_before==0 && p->summary().pc_after==2);
    // A half-known instruction word is not executable.
    Dsp1Image half;const std::array<std::uint8_t,1> zero{0};CHECK(half.program.Stage(0,zero));p=Probe(half);
    CHECK(p->Advance(1)==DspProbeState::Fault && p->summary().fault_address==1);
}
void ExplicitReferenceReset() {
    auto image=Image({0xD4B8,0x0100});const std::array<std::uint8_t,2> supplied{0xEF,0xBE};
    CHECK(image.data.Stage(0x300,supplied));auto p=Probe(image,DspProbeReset::ReferenceZeroData);
    CHECK(p->memory()[0x40300]==0xEF && p->provenance()[0x40300]==1);
    CHECK(p->memory()[0x40200]==0 && p->provenance()[0x40200]==2);
    CHECK(p->provenance()[0]==1 && p->provenance()[4]==0);
    CHECK(p->Advance(1)==DspProbeState::Paused && p->summary().completed_steps==1);
    CHECK(p->Advance(1)==DspProbeState::Fault && p->summary().fault_address==4);
    image.special={true,Dsp1MemoryType::Data,0x1000,532};
    const char* error=nullptr;
    CHECK(!DspExecutionProbe::Create(image,DspProbeReset::ReferenceZeroData,error) && error);
    CHECK(!DspExecutionProbe::Create(image,static_cast<DspProbeReset>(99),error));
}
void RealMailboxInstructions() {
    // The test PROGRAM writes the mailboxes. No host handler supplies ready words.
    auto image=Image({0x2101,0xD4BC,0x80C0,0xD4BC,0x80C4,0xD4BC,0x80C8,
                      0x2142,0xD4BC,0x80C8},true);
    auto a=Probe(image),b=Probe(image);
    CHECK(a->Advance(100)==DspProbeState::ProtocolComplete);
    for(unsigned i=0;i<6;++i)b->Advance(1);
    CHECK(b->summary()==a->summary());
    CHECK(a->summary().completed_steps==6 && a->summary().reply_count==4);
    CHECK(a->summary().pipe_base==0x42 && a->summary().has_pipe_base);
    const std::array<DspProbeReply,4> expected{{{0,1,2},{1,1,3},{2,1,4},{2,0x42,6}}};
    CHECK(std::equal(expected.begin(),expected.end(),a->replies().begin(),a->replies().end()));
    auto s=a->summary();CHECK(a->Advance(100)==DspProbeState::ProtocolComplete && a->summary()==s);
    CHECK(std::equal(a->memory().begin(),a->memory().end(),b->memory().begin()));
}
void LimitsAndUnmodeledHardware() {
    auto image=Image({0xD4B8,0x820E});auto p=Probe(image,DspProbeReset::ReferenceZeroData);
    CHECK(p->Advance(10)==DspProbeState::Fault);
    CHECK(p->summary().fault==DspProbeFault::Backend && p->summary().completed_steps==0);
    CHECK(std::string(p->summary().error.data()).find("unmodeled MMIO read")!=std::string::npos);
    // These unmodeled timer cells are assigned from temporaries during construction.
    // Diagnostics must use the actual access address, never a captured dead Cell.
    for (auto address : {0x802CU,0x803EU}) {
        auto moved=Image({0xD4B8,static_cast<std::uint16_t>(address)});
        auto check=Probe(moved,DspProbeReset::ReferenceZeroData);
        CHECK(check->Advance(1)==DspProbeState::Fault);
        CHECK(std::string(check->summary().error.data())==
              "Teakra unmodeled MMIO read at "+std::to_string(address-0x8000));
    }
    Dsp1Image nops;std::vector<std::uint8_t> program(kDsp1BankBytes,0);CHECK(nops.program.Stage(0,program));p=Probe(nops);
    CHECK(p->Advance(0xFFFFFFFF)==DspProbeState::Paused);
    CHECK(p->summary().completed_steps==DspExecutionProbe::kMaxStepsPerCall && p->summary().reply_count==0);
    std::vector<std::uint8_t> raw(0x80000);Teakra::SharedMemory bank(raw.data());bool failed=false;
    try{(void)bank.ReadWord(0x40000);}catch(const std::exception&){failed=true;}CHECK(failed);
    failed=false;try{bank.WriteWord(0x80000000,1);}catch(const std::exception&){failed=true;}CHECK(failed);
    CHECK(std::all_of(raw.begin(),raw.end(),[](auto b){return b==0;}));
    const char* error=nullptr;fail_next=true;auto allocation=DspExecutionProbe::Create(image,DspProbeReset::KnownOnly,error);fail_next=false;
    CHECK(!allocation && error);
}
std::vector<std::uint8_t> FirmwareContainer() {
    const auto program=Bytes({0x2101,0xD4BC,0x80C0,0xD4BC,0x80C4,0xD4BC,0x80C8,0x2142,0xD4BC,0x80C8});
    std::vector<std::uint8_t> b(0x300+program.size(),0);
    b[0x100]='D';b[0x101]='S';b[0x102]='P';b[0x103]='1';
    dsp_fixture::Put32(b,0x104,b.size());b[0x108]=1;b[0x10E]=1;b[0x10F]=1;
    dsp_fixture::Put32(b,0x120,0x300);dsp_fixture::Put32(b,0x128,program.size());
    std::copy(program.begin(),program.end(),b.begin()+0x300);dsp_fixture::Rehash(b,0);return b;
}
void HostOnlyIpc() {
    Kernel kernel;GuestMemory memory;IpcRouter router;SvcBridge bridge(kernel,&router);
    auto service=std::make_shared<DspDiscoveryService>(DspSpecialConfig{},DspProbeOptions{true});
    CHECK(memory.EnsureTlsMappings(kernel));constexpr std::uint32_t input=0x08000000;
    CHECK(memory.Map(input,0x1000,MemoryPermission::Read));auto image=FirmwareContainer();CHECK(memory.LoadBytes(input,image));
    CHECK(router.RegisterService("dsp::DSP",service)==0);Handle handle=0;CHECK(router.ConnectToService(kernel,"dsp::DSP",&handle)==0);
    const auto cb=kernel.current_thread()->tls_address+kIpcCommandBufferOffset;
    const auto size=static_cast<std::uint32_t>(image.size());const IpcCommandBuffer q{0x001100C2,size,0xFF,0xE00FF,(size<<4)|10,input};
    for(unsigned i=0;i<q.size();++i)CHECK(memory.Write32(cb+i*4,q[i]));
    a32::GuestState cpu{};for(unsigned i=0;i<16;++i)cpu.r[i]=0xAB0000+i;cpu.r[0]=handle;cpu.r[15]=0x25947C;
    const auto before=cpu;const auto handles=kernel.handles().OpenHandleCount(),threads=kernel.threads().size();const auto now=kernel.now_ns();
    auto stop=bridge.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcSendSyncRequest},cpu,&memory);
    CHECK(stop.kind==a32::ExitKind::Svc && cpu.r==before.r && cpu.cpsr==before.cpsr);
    CHECK(router.unsupported_request() && service->execution_probe());
    CHECK(service->execution_probe()->summary().state==DspProbeState::ProtocolComplete);
    for(unsigned i=0;i<q.size();++i){std::uint32_t word;CHECK(memory.Read32(cb+4*i,&word) && word==q[i]);}
    for(unsigned i=0;i<image.size();++i){std::uint8_t byte;CHECK(memory.Read8(input+i,&byte) && byte==image[i]);}
    CHECK(kernel.handles().OpenHandleCount()==handles && kernel.threads().size()==threads && kernel.now_ns()==now);
    CHECK(!memory.IsMapped(0x1FF00000));
    const auto* old=service->execution_probe();
    GuestMemory protected_memory;CHECK(protected_memory.Map(cb,sizeof(q),MemoryPermission::Read));
    CHECK(protected_memory.LoadBytes(cb,{reinterpret_cast<const std::uint8_t*>(q.data()),sizeof(q)}));
    CHECK(bridge.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcSendSyncRequest},cpu,&protected_memory).kind==a32::ExitKind::Fallthrough);
    CHECK(cpu.r[0]==kResultInvalidPointer && service->execution_probe()==old);
}
} // namespace
int main(){
 try{KnownWritesAndFaults();ExplicitReferenceReset();RealMailboxInstructions();LimitsAndUnmodeledHardware();HostOnlyIpc();}
 catch(const std::exception&e){std::cerr<<"exception: "<<e.what()<<"\n";return EXIT_FAILURE;}
 if(failures)return EXIT_FAILURE;
 std::cout<<"PASS: guarded real DSP instructions, memory provenance, actual mailbox stores, bounded continuation and no guest load response\n";
 return EXIT_SUCCESS;
}
