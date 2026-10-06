#include "services/dsp_special_config.h"
#include "services/dsp_discovery_service.h"
#include "host/dsp_special_input.h"
#include "runtime/ctr_svc_bridge.h"
#include "dsp1_test_fixture.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>

static long fail_after=-1;
void* operator new(std::size_t n) {
    if (fail_after==0) throw std::bad_alloc();
    if (fail_after>0) --fail_after;
    if (auto p=std::malloc(n?n:1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
namespace {
using namespace lego::ctr;using namespace dsp_fixture;
int failures=0;
#define CHECK(x) do {if (!(x)) {std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}} while (0)
std::unique_ptr<Dsp1Image> Make() {
    std::unique_ptr<Dsp1Image> image;
    if (!StageDsp1Image(Image(true),image)) throw std::runtime_error("invalid fixture");
    return image;
}
std::array<std::uint8_t,kDspSpecialConfigBytes> Pattern() {
    std::array<std::uint8_t,kDspSpecialConfigBytes> p{};
    for (unsigned i=0;i<p.size();++i) p[i]=static_cast<std::uint8_t>(i*37+11);
    return p;
}
void SourcesAndExactPlacement() {
    const auto pattern=Pattern();auto input=pattern;const auto config=DspSpecialConfig::FromBlock(input);
    CHECK(config && config->profile()==DspSpecialProfile::SuppliedBlock);
    std::span<const std::uint8_t> output=input;
    CHECK(DspSpecialConfig{}.Read(output)==DspConfigRead::Unconfigured && output.empty());
    output=input;
    CHECK(DspSpecialConfig::EmptySystemConfig().Read(output)==DspConfigRead::Missing && output.empty());
    input[0]^=255;CHECK(config->Read(output)==DspConfigRead::Available && output[0]==11);
    CHECK(!DspSpecialConfig::FromBlock({}) && !DspSpecialConfig::FromBlock(std::span(input).first(531)));
    std::array<std::uint8_t,533> oversized{};CHECK(!DspSpecialConfig::FromBlock(oversized));
    for (unsigned type=0;type<3;++type) for (bool empty:{false,true}) {
        auto image=Make();image->special.type=static_cast<Dsp1MemoryType>(type);
        Dsp1Image before=*image;DspSpecialReceipt receipt;
        CHECK(StageDspSpecial(empty?DspSpecialConfig::EmptySystemConfig():*config,*image,receipt)==DspSpecialError::None);
        CHECK(receipt.read_status==(empty?DspConfigRead::Missing:DspConfigRead::Available));
        CHECK(receipt.source==(empty?DspSpecialSource::MissingConfigZeroFallback:DspSpecialSource::SuppliedConfigBlock));
        CHECK(receipt.block_id==0x70000 && receipt.bytes==532 && receipt.target_bytes==0x4000);
        for (unsigned bank=0;bank<2;++bank) {
            const auto& a=bank?image->data:image->program;const auto& b=bank?before.data:before.program;
            for (std::uint32_t i=0;i<kDsp1BankBytes;++i) {
                const bool special=(bank==(type==2)) && i>=0x4000 && i<0x4214;
                CHECK(a.known_mask()[i]==(special?1:b.known_mask()[i]));
                CHECK(a.storage()[i]==(special?(empty?0:pattern[(i-0x4000)]):b.storage()[i]));
            }
        }
        const auto prior=receipt;
        CHECK(StageDspSpecial(*config,*image,receipt)==DspSpecialError::AlreadyKnown && receipt==prior);
        CHECK(image->component_sha256==before.component_sha256 && image->segments.size()==before.segments.size());
    }
}
void RejectWithoutMutation() {
    const auto config=DspSpecialConfig::EmptySystemConfig();
    auto reject=[&](std::unique_ptr<Dsp1Image> p,DspSpecialError expected,const DspSpecialConfig& source) {
        const auto before=*p;DspSpecialReceipt receipt;receipt.bytes=0xAABBCCDD;const auto old=receipt;
        CHECK(StageDspSpecial(source,*p,receipt)==expected && receipt==old);
        CHECK(std::equal(p->program.storage().begin(),p->program.storage().end(),before.program.storage().begin()));
        CHECK(std::equal(p->data.storage().begin(),p->data.storage().end(),before.data.storage().begin()));
        CHECK(std::equal(p->program.known_mask().begin(),p->program.known_mask().end(),before.program.known_mask().begin()));
        CHECK(std::equal(p->data.known_mask().begin(),p->data.known_mask().end(),before.data.known_mask().begin()));
    };
    reject(Make(),DspSpecialError::Unresolved,{});
    for (auto n:{0U,530U,534U,0xFFFFFFFFU}) {auto p=Make();p->special.bytes=n;reject(std::move(p),DspSpecialError::UnsupportedShape,config);}
    for (auto w:{0x10000U,0xFFFFFFFFU}) {auto p=Make();p->special.target_words=w;reject(std::move(p),DspSpecialError::Range,config);}
    auto p=Make();p->special.type=static_cast<Dsp1MemoryType>(3);reject(std::move(p),DspSpecialError::UnsupportedShape,config);
    p=Make();p->memory_layout=0;reject(std::move(p),DspSpecialError::Layout,config);
    p=Make();p->special.target_words=0x500;reject(std::move(p),DspSpecialError::AlreadyKnown,config);
    p=Make();p->special.required=false;DspSpecialReceipt receipt;receipt.bytes=99;const auto old=receipt;
    CHECK(StageDspSpecial({},*p,receipt)==DspSpecialError::None && receipt==old && !p->data.IsKnown(0x4000,532));
    bool succeeded=false;unsigned rejected=0;
    for (long n=0;n<64;++n) {
        p=Make();receipt=old;fail_after=n;
        const auto result=StageDspSpecial(config,*p,receipt);fail_after=-1;
        if (result==DspSpecialError::None) {succeeded=true;break;}
        ++rejected;CHECK(result==DspSpecialError::Allocation && receipt==old);
        CHECK(p->program.known_bytes()==16 && p->data.known_bytes()==16 && !p->data.Read(0x4000));
    }
    CHECK(rejected>0 && succeeded);
}
struct Temp {
    std::filesystem::path path;
    Temp() {
        const auto n=std::chrono::steady_clock::now().time_since_epoch().count();
        for (int i=0;i<100;++i) {
            auto p=std::filesystem::temp_directory_path()/("lego-dsp-block-"+std::to_string(n)+"-"+std::to_string(i));
            if (std::filesystem::create_directory(p)) {path=p;return;}
        }
        throw std::runtime_error("cannot create owned fixture directory");
    }
    ~Temp() {std::error_code e;std::filesystem::remove_all(path,e);}
};
void HostFileFailuresNeverFallback() {
    Temp temp;const auto path=temp.path/"block";const auto bytes=Pattern();
    auto rejects=[&](const auto& p) {bool caught=false;try {(void)lego::host::ReadDspSpecialBlock(p);}catch(const std::runtime_error&) {caught=true;}CHECK(caught);};
    rejects(path);rejects(temp.path);
    for (auto size:{0U,531U,533U}) {std::ofstream f(path,std::ios::binary);std::vector<char> b(size,7);f.write(b.data(),b.size());f.close();rejects(path);}
    {std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());}
    const auto config=lego::host::ReadDspSpecialBlock(path);std::span<const std::uint8_t> got;
    CHECK(config.Read(got)==DspConfigRead::Available && std::equal(got.begin(),got.end(),bytes.begin()));
}
void ServiceNeverAcknowledgesBoot() {
    Kernel kernel;GuestMemory memory;IpcRouter router;SvcBridge bridge(kernel,&router);
    auto service=std::make_shared<DspDiscoveryService>(DspSpecialConfig::EmptySystemConfig());Handle handle{};
    CHECK(memory.EnsureTlsMappings(kernel));CHECK(router.RegisterService("dsp::DSP",service)==0);
    CHECK(router.ConnectToService(kernel,"dsp::DSP",&handle)==0);
    const auto bytes=Image(true);constexpr std::uint32_t address=0x08000000;
    CHECK(memory.Map(address,0x1000,MemoryPermission::Read) && memory.LoadBytes(address,bytes));
    const auto cb=kernel.current_thread()->tls_address+kIpcCommandBufferOffset;
    IpcCommandBuffer q{0x001100C2,static_cast<std::uint32_t>(bytes.size()),255,0xE00FF,(static_cast<std::uint32_t>(bytes.size())<<4)|0xA,address};
    auto put=[&]{for(unsigned i=0;i<q.size();++i) CHECK(memory.Write32(cb+4*i,q[i]));};
    auto read=[&]{IpcCommandBuffer v{};for(unsigned i=0;i<v.size();++i) CHECK(memory.Read32(cb+4*i,&v[i]));return v;};
    a32::GuestState cpu{};for(unsigned i=0;i<16;++i)cpu.r[i]=0x43210000+i;
    cpu.r[0]=handle;cpu.r[15]=0x25947C;cpu.cpsr=0x60000010;cpu.fpscr=0x23000010;const auto before=cpu;
    const auto count=kernel.handles().OpenHandleCount();const auto time=kernel.now_ns();
    const a32::ExecutionResult call{a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcSendSyncRequest};
    put();CHECK(bridge.Handle(call,cpu,&memory).kind==a32::ExitKind::Svc);
    CHECK(cpu.r==before.r && cpu.cpsr==before.cpsr && cpu.fpscr==before.fpscr && read()==q);
    CHECK(kernel.handles().OpenHandleCount()==count && kernel.now_ns()==time && kernel.threads().size()==1);
    CHECK(service->inspected_image() && service->inspected_image()->data.IsKnown(0x4000,532));
    CHECK(service->special_receipt().source==DspSpecialSource::MissingConfigZeroFallback);
    CHECK(!memory.IsMapped(0x1FF00000) && router.unsupported_request());
    const auto* saved=service->inspected_image();const auto receipt=service->special_receipt();
    auto corrupt=bytes;corrupt[0x300]^=1;CHECK(memory.LoadBytes(address,corrupt));put();
    CHECK(bridge.Handle(call,cpu,&memory).kind==a32::ExitKind::Svc);
    CHECK(service->inspected_image()==saved && service->special_receipt()==receipt && read()==q);
    for (auto permission:{MemoryPermission::Read,MemoryPermission::Write}) {
        GuestMemory blocked;CHECK(blocked.Map(cb,sizeof(q),permission));
        CHECK(blocked.LoadBytes(cb,{reinterpret_cast<const std::uint8_t*>(q.data()),sizeof(q)}));cpu=before;
        CHECK(bridge.Handle(call,cpu,&blocked).kind==a32::ExitKind::Fallthrough && cpu.r[0]==kResultInvalidPointer);
        CHECK(service->inspected_image()==saved && service->special_receipt()==receipt);
    }
}
}
int main() {
    SourcesAndExactPlacement();RejectWithoutMutation();HostFileFailuresNeverFallback();ServiceNeverAcknowledgesBoot();
    if(failures)return EXIT_FAILURE;
    std::cout<<"PASS: explicit DSP special source/fallback, exact known-byte placement, transactional errors and still-unacknowledged firmware boot\n";
}
