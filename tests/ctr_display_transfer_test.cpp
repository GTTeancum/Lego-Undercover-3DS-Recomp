#include "services/gsp_display_transfer.h"
#include "runtime/ctr_memory.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace lego::ctr;
namespace {
int failures=0;
#define CHECK(x) do {if (!(x)) {std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}} while(0)
constexpr auto RW=MemoryPermission::Read|MemoryPermission::Write;
std::vector<std::uint8_t> TiledPattern(unsigned width,unsigned height,std::vector<std::uint8_t>& linear) {
    std::vector<std::uint8_t> tiled(width*height*2);linear.resize(tiled.size());
    // Independent inverse address construction: deinterleave the local tile
    // index. Do not reuse the implementation's coordinate tables/formula.
    for(unsigned tile=0;tile<(width/8)*(height/8);++tile)for(unsigned q=0;q<64;++q){
        const unsigned x=8*(tile%(width/8))+(q&1)+((q>>1)&2)+((q>>2)&4);
        const unsigned y=8*(tile/(width/8))+((q>>1)&1)+((q>>2)&2)+((q>>3)&4);
        const auto pos=y*width+x;
        const std::uint16_t value=static_cast<std::uint16_t>(pos); // 256x256 covers all RGBA4 encodings.
        tiled[2*(64*tile+q)]=value&255;tiled[2*(64*tile+q)+1]=value>>8;
        linear[2*pos]=value&255;linear[2*pos+1]=value>>8;
    }
    return tiled;
}
void OriginalAndNonzeroPixels(){
    auto bank=GpuVramBank::ReferenceZero();CHECK(bank->bytes().size()==kGpuVramBytes);
    CHECK(std::all_of(bank->bytes().begin(),bank->bytes().end(),[](auto b){return b==0;}));
    CHECK(!bank->Write(kGpuVramBytes+1,{}));CHECK(bank->Write(kGpuVramBytes,{}));
    const std::array<std::uint8_t,1> one{1};CHECK(!bank->Write(kGpuVramBytes,one));
    for(auto dims:std::array<std::array<unsigned,2>,5>{{{8,8},{16,24},{128,128},{256,256},{1024,512}}}){
        const auto width=dims[0],height=dims[1],size=width*height*2;
        std::vector<std::uint8_t> expected;const auto source=TiledPattern(width,height,expected);
        const auto offset=kGpuVramBytes-size;CHECK(bank->Write(offset,source));
        DisplayTransferRequest q{kGpuVramVirtualBase+offset,0x14013950,(height<<16)|width,(height<<16)|width,0x4400};
        DisplayTransferPlan p;const char* error=nullptr;
        CHECK(StageDisplayTransfer(q,bank.get(),p,error)&&!error);
        CHECK(p.output==expected&&p.bytes==size&&p.width==width&&p.height==height);
        CHECK(std::equal(source.begin(),source.end(),bank->bytes().begin()+offset));
    }
    // The source is live bank data, not a per-transfer generated pattern/reset.
    std::vector<std::uint8_t> marker(128,0x9A);CHECK(bank->Write(kGpuVramBytes-128,marker));
    DisplayTransferPlan p;const char* error=nullptr;
    CHECK(StageDisplayTransfer({kGpuVramVirtualBase+kGpuVramBytes-128,0x1BFFFF80,0x80008,0x80008,0x4400},bank.get(),p,error));
    CHECK(p.output==marker);
    auto independent=GpuVramBank::ReferenceZero();CHECK(independent->bytes().back()==0);
}
void StrictBoundsAndNoPartialPlan(){
    auto bank=GpuVramBank::ReferenceZero();
    const DisplayTransferRequest good{0x1F5F8000,0x14013950,0x800080,0x800080,0x4400};
    std::vector<DisplayTransferRequest> bad;
    for(auto v:{0U,0x4401U,0x4402U,0x4404U,0x4408U,0x4420U,0x01004400U,0x00004480U,0x00003300U}){auto q=good;q.flags=v;bad.push_back(q);}
    for(auto v:{0U,0x00080007U,0x00070008U,0xFFFF0008U,0xFFFFFFFFU,0x08000800U}){auto q=good;q.input_size=q.output_size=v;bad.push_back(q);}
    auto q=good;q.output_size+=8;bad.push_back(q);
    for(auto v:{0U,0x18000000U,0x1EFFFFF8U,0x1F5F8008U,0x1F600000U,0x1F000001U,0xFFFFFFF8U}){q=good;q.input=v;bad.push_back(q);}
    for(auto v:{0U,0x13FFFFF8U,0x14000001U,0x1BFF8008U,0x1C000000U,0xFFFFFFF8U}){q=good;q.output=v;bad.push_back(q);}
    DisplayTransferPlan plan;plan.width=999;plan.output={1,2,3};const char* error=nullptr;
    CHECK(!StageDisplayTransfer(good,nullptr,plan,error)&&error&&plan.width==999&&plan.output==std::vector<std::uint8_t>({1,2,3}));
    for(const auto& item:bad){CHECK(!StageDisplayTransfer(item,bank.get(),plan,error)&&error);CHECK(plan.width==999&&plan.output==std::vector<std::uint8_t>({1,2,3}));}
}
void DeviceWritePreparation(){
    GuestMemory m;constexpr auto base=0x14000000U;
    CHECK(m.Map(base,64,RW));const std::vector<std::uint8_t> data(16,0xC7);
    CHECK(!m.CommitDeviceWrite(base,data));std::uint32_t v=1;CHECK(m.Read32(base,&v)&&v==0);
    std::uint64_t value=0,token=0,after=0,other=0;std::uint32_t fault=0;
    CHECK(m.LoadExclusive(base,4,&value,&token,&fault));CHECK(m.LoadExclusive(base+32,4,&value,&other,&fault));
    CHECK(m.PrepareDeviceWrite(base,16));CHECK(m.LoadExclusive(base,4,&value,&after,&fault)&&after==token);
    CHECK(m.CommitDeviceWrite(base,data));CHECK(m.Read32(base,&v)&&v==0xC7C7C7C7);
    CHECK(m.StoreExclusive(base,4,0,token,&fault)==a32::ExclusiveStoreResult::ReservationLost);
    CHECK(m.StoreExclusive(base+32,4,0,other,&fault)==a32::ExclusiveStoreResult::Success);
    CHECK(!m.PrepareDeviceWrite(base+60,8)&&!m.CommitDeviceWrite(base+60,data));
    CHECK(m.Map(base+64,64,RW));CHECK(!m.PrepareDeviceWrite(base+60,8)); // Cross-region is not supported.
    CHECK(m.Map(0x15000000,64,MemoryPermission::Read));CHECK(!m.PrepareDeviceWrite(0x15000000,16));
    CHECK(!m.CommitDeviceWrite(0x15000000,data));CHECK(!m.PrepareDeviceWrite(0xFFFFFFF8,16));
    auto shared=std::make_shared<ServiceSharedMemoryObject>();CHECK(m.MapSharedServicePage(0x16000000,shared,RW));
    CHECK(!m.PrepareDeviceWrite(0x16000000,16)&&!m.CommitDeviceWrite(0x16000000,data));
    CHECK(m.PrepareDeviceWrite(0xFFFFFFFF,0)&&m.CommitDeviceWrite(0xFFFFFFFF,{}));
    CHECK(m.Map(0x17000000,64,MemoryPermission::Write));CHECK(m.PrepareDeviceWrite(0x17000000,16)&&m.CommitDeviceWrite(0x17000000,data));
}

struct RgbCropPattern { std::vector<std::uint8_t> tiled,expected; };
RgbCropPattern MakeRgbCrop(unsigned input_width,unsigned input_height,unsigned width,unsigned height) {
    RgbCropPattern pattern; pattern.tiled.resize(input_width*input_height*4);
    std::vector<std::uint8_t> linear(input_width*input_height*3);
    // Author source bytes in tile-index order and independently deinterleave
    // each local index. Expected output is a crop of the logical BGR image;
    // it never uses the implementation's forward Morton-address formula.
    for (unsigned tile=0;tile<(input_width/8)*(input_height/8);++tile)
        for (unsigned q=0;q<64;++q) {
            const unsigned x=8*(tile%(input_width/8))+(q&1)+((q>>1)&2)+((q>>2)&4);
            const unsigned y=8*(tile/(input_width/8))+((q>>1)&1)+((q>>2)&2)+((q>>3)&4);
            const std::array<std::uint8_t,4> abgr{
                static_cast<std::uint8_t>(3+17*x+29*y),
                static_cast<std::uint8_t>(30+3*x+5*y),
                static_cast<std::uint8_t>(210-x-2*y),
                static_cast<std::uint8_t>(11+11*x+7*y)};
            for (unsigned channel=0;channel<4;++channel) pattern.tiled[4*(tile*64+q)+channel]=abgr[channel];
            for (unsigned channel=0;channel<3;++channel) linear[3*(y*input_width+x)+channel]=abgr[channel+1];
        }
    for (unsigned y=0;y<height;++y)
        pattern.expected.insert(pattern.expected.end(),linear.begin()+3*y*input_width,
                                linear.begin()+3*(y*input_width+width));
    return pattern;
}

bool SamePlan(const DisplayTransferPlan& a,const DisplayTransferPlan& b) {
    return a.request.input==b.request.input && a.request.output==b.request.output &&
           a.request.input_size==b.request.input_size && a.request.output_size==b.request.output_size &&
           a.request.flags==b.request.flags && a.width==b.width && a.height==b.height &&
           a.bytes==b.bytes && a.input_bytes==b.input_bytes && a.output_vram==b.output_vram && a.output==b.output;
}

void UnscaledRgbCropBytesAndAtomicBounds() {
    auto bank=GpuVramBank::ReferenceZero();
    constexpr std::uint32_t source=kGpuVramVirtualBase+0x20000,destination=kGpuVramVirtualBase+0x300000;
    const auto request=[&](unsigned iw,unsigned ih,unsigned ow,unsigned oh) {
        return DisplayTransferRequest{source,destination,(ih<<16)|iw,(oh<<16)|ow,0x1004};
    };
    DisplayTransferPlan plan; const char* error=nullptr;
    for (const auto dims:std::array<std::array<unsigned,4>,4>{{{16,16,9,9},{24,16,19,11},{8,8,1,1},{256,400,240,320}}}) {
        const auto pattern=MakeRgbCrop(dims[0],dims[1],dims[2],dims[3]);
        CHECK(bank->Write(source-kGpuVramVirtualBase,pattern.tiled));
        const auto before=std::vector<std::uint8_t>(bank->bytes().begin(),bank->bytes().end());
        const auto q=request(dims[0],dims[1],dims[2],dims[3]);
        CHECK(StageDisplayTransfer(q,bank.get(),plan,error) && !error);
        CHECK(plan.width==dims[2] && plan.height==dims[3] && plan.output_vram);
        CHECK(plan.input_bytes==pattern.tiled.size() && plan.bytes==dims[2]*dims[3]*3);
        CHECK(plan.output==pattern.expected && std::equal(before.begin(),before.end(),bank->bytes().begin()));
        if (dims[2]>=9 && dims[3]>=9) {
            // Literal neighboring pixels prove no horizontal averaging and
            // alpha omission; the last triplet crosses both tile boundaries.
            CHECK((std::vector<std::uint8_t>(plan.output.begin(),plan.output.begin()+6)==
                   std::vector<std::uint8_t>{30,210,11,33,209,22}));
            const auto row=3*dims[2],crossing=3*(8*dims[2]+8);
            CHECK(plan.output[row]==35 && plan.output[row+1]==208 && plan.output[row+2]==18);
            CHECK(plan.output[crossing]==94 && plan.output[crossing+1]==186 && plan.output[crossing+2]==155);
        }
    }
    // Entire declared input and exact produced output are validated separately.
    // Adjacent spans and an output ending exactly at the bank end are valid.
    auto edge=request(8,8,8,8);
    edge.input=kGpuVramVirtualBase+kGpuVramBytes-256; edge.output=edge.input-192;
    CHECK(StageDisplayTransfer(edge,bank.get(),plan,error));
    edge.input=source; edge.output=kGpuVramVirtualBase+kGpuVramBytes-192;
    CHECK(StageDisplayTransfer(edge,bank.get(),plan,error));
    edge.output=source+256; CHECK(StageDisplayTransfer(edge,bank.get(),plan,error));
    const auto saved=plan;
    const auto before=std::vector<std::uint8_t>(bank->bytes().begin(),bank->bytes().end());
    std::vector<DisplayTransferRequest> rejected;
    auto good=request(16,16,9,9);
    for (auto address:{0U,kGpuVramPhysicalBase,kGpuVramVirtualBase-8,source+1,
                      kGpuVramVirtualBase+kGpuVramBytes-128,0xFFFFFFF8U}) {
        auto q=good;q.input=address;rejected.push_back(q);
    }
    for (auto address:{0U,0x14000000U,destination+1,source+1016,source-8,
                      kGpuVramVirtualBase+kGpuVramBytes-240,0xFFFFFFF8U}) {
        auto q=good;q.output=address;rejected.push_back(q);
    }
    for (auto dims:std::array<std::array<unsigned,4>,8>{{{0,16,1,1},{16,0,1,1},{15,16,9,9},
            {16,15,9,9},{16,16,0,9},{16,16,9,0},{16,16,17,9},{16,16,9,17}}})
        rejected.push_back(request(dims[0],dims[1],dims[2],dims[3]));
    rejected.push_back(request(520,512,1,1)); // Small crop cannot bypass the input-span work cap.
    rejected.push_back(request(65535,65535,65535,65535));
    for (auto flags:{0x1005U,0x1006U,0x1024U,0x1404U,0x02001004U}) {
        auto q=good;q.flags=flags;rejected.push_back(q);
    }
    CHECK(!StageDisplayTransfer(good,nullptr,plan,error) && error && SamePlan(plan,saved));
    for (const auto& q:rejected) {
        CHECK(!StageDisplayTransfer(q,bank.get(),plan,error) && error);
        CHECK(SamePlan(plan,saved) && std::equal(before.begin(),before.end(),bank->bytes().begin()));
    }
}
}
int main(){OriginalAndNonzeroPixels();StrictBoundsAndNoPartialPlan();DeviceWritePreparation();UnscaledRgbCropBytesAndAtomicBounds();
 if(failures)return EXIT_FAILURE;std::cout<<"PASS: real VRAM data, RGBA4 detiling, unscaled BGR crop, strict bounds and atomic device staging\n";return EXIT_SUCCESS;}
