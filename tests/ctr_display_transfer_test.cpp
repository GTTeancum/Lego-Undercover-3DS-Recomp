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
}
int main(){OriginalAndNonzeroPixels();StrictBoundsAndNoPartialPlan();DeviceWritePreparation();
 if(failures)return EXIT_FAILURE;std::cout<<"PASS: real VRAM data, RGBA4 detiling, strict bounds and allocation-free device commits\n";return EXIT_SUCCESS;}
