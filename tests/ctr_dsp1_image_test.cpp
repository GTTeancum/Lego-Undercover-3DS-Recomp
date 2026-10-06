#include "services/dsp1_image.h"
#include "dsp1_test_fixture.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <new>
#include <limits>
// Deterministic allocation faults apply only during an explicitly armed test call.
static long fail_after=-1;
void* operator new(std::size_t n) {
    if(fail_after==0)throw std::bad_alloc();
    if(fail_after>0)--fail_after;
    if(auto p=std::malloc(n?n:1))return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
namespace {
using namespace lego::ctr;using namespace dsp_fixture;
int failures=0;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}}while(0)
void ExactPlacementAndUnknownBytes(){
    auto b=Image(true);std::unique_ptr<Dsp1Image> p;
    CHECK(StageDsp1Image(b,p));CHECK(p && p->segments.size()==3);
    CHECK(p->program.known_bytes()==16 && p->data.known_bytes()==16);
    for(const auto& s:p->segments){const auto& bank=s.type==Dsp1MemoryType::Data?p->data:p->program;
        CHECK(bank.IsKnown(s.target_words*2,s.bytes));
        for(unsigned i=0;i<s.bytes;++i)CHECK(bank.Read(s.target_words*2+i)==b[s.source_offset+i]);}
    CHECK(!p->program.Read(0) && !p->program.Read(0x28));
    CHECK(!p->data.Read(0x4000) && !p->data.IsKnown(0x4000,0x214));
    CHECK(p->receive_startup_replies && p->special.required && p->special.type==Dsp1MemoryType::Data);
    CHECK(p->special.target_words==0x2000 && p->special.bytes==0x214);
    CHECK(!p->data.IsKnown(0xFFFFFFFF,1) && !p->data.IsKnown(0,kDsp1BankBytes+1));
    const auto before=p->data.Read(0xA00);b[0x310]^=255;CHECK(p->data.Read(0xA00)==before);
    // Signature changes are not authenticated by this parser, and cannot imply RSA validation.
    b=Image();b[0]=0xAC;b[0x10B]=0xCD;CHECK(StageDsp1Image(b,p));CHECK(!p->special.required);
    CHECK(p->segments[0].type==Dsp1MemoryType::ProgramA && p->segments[1].type==Dsp1MemoryType::ProgramB);
}
void InvalidInputsAreTransactional(){
    std::unique_ptr<Dsp1Image> p;CHECK(StageDsp1Image(Image(),p));
    const auto identity=p.get();const auto hash=p->component_sha256;
    auto reject=[&](const std::vector<std::uint8_t>& b,Dsp1Error e){
        const auto r=StageDsp1Image(b,p);CHECK(!r && r.error==e);CHECK(p.get()==identity && p->component_sha256==hash);};
    reject({},Dsp1Error::Size);reject(std::vector<std::uint8_t>(0x2FF),Dsp1Error::Size);
    reject(std::vector<std::uint8_t>(kDsp1MaxBytes+1),Dsp1Error::Size);
    auto b=Image();b[0x100]='X';reject(b,Dsp1Error::Magic);
    b=Image();Put32(b,0x104,0xFFFFFFFF);reject(b,Dsp1Error::BinarySize);
    for(auto n:{0,11,255}){b=Image();b[0x10E]=n;reject(b,Dsp1Error::SegmentCount);}
    b=Image();b[0x10F]=4;reject(b,Dsp1Error::Flags);
    b=Image();b[0x12F]=3;reject(b,Dsp1Error::Type);
    for(auto o:{0U,0x2FFU,0x31EU,0xFFFFFFFFU}){b=Image();Put32(b,0x120,o);reject(b,Dsp1Error::SourceRange);}
    for(auto n:{0U,0xFFFFFFFFU}){b=Image();Put32(b,0x128,n);reject(b,Dsp1Error::SourceRange);}
    b=Image();Put32(b,0x120,0x301);reject(b,Dsp1Error::Alignment);
    b=Image();Put32(b,0x128,7);reject(b,Dsp1Error::Alignment);
    for(auto w:{0x20000U,0xFFFFFFFFU}){b=Image();Put32(b,0x124,w);reject(b,Dsp1Error::TargetRange);}
    b=Image();Put32(b,0x154,0x10000);reject(b,Dsp1Error::TargetRange);
    b=Image();b[0x108]=0;reject(b,Dsp1Error::MemoryLayout);
    b=Image();Put32(b,0x150,0x300);reject(b,Dsp1Error::SourceOverlap);
    b=Image();Put32(b,0x154,0x10);reject(b,Dsp1Error::TargetOverlap);
    b=Image();b[0x300]^=1;reject(b,Dsp1Error::SegmentHash);
    b=Image();b[0x130]^=1;reject(b,Dsp1Error::SegmentHash);
    b=Image(true);b[0x10D]=3;reject(b,Dsp1Error::SpecialRange);
    b=Image(true);Put32(b,0x114,0);reject(b,Dsp1Error::SpecialRange);
    b=Image(true);Put32(b,0x110,0x10000);reject(b,Dsp1Error::SpecialRange);
    b=Image(true);Put32(b,0x110,0x8000);reject(b,Dsp1Error::MemoryLayout);
    b=Image(true);Put32(b,0x110,0x500);reject(b,Dsp1Error::SpecialOverlap);
    // Known bank writes cannot overwrite staged bytes or spill past bank end.
    Dsp1Bank bank;const std::array<std::uint8_t,2> two{0xA5,0x5A};
    CHECK(bank.Stage(kDsp1BankBytes-2,two));CHECK(!bank.Stage(kDsp1BankBytes-1,two));
    CHECK(!bank.Stage(kDsp1BankBytes-2,two));CHECK(bank.Read(kDsp1BankBytes-1)==0x5A);
}
void AddressBoundariesAndCounts(){
    std::unique_ptr<Dsp1Image> p;
    for(unsigned type=0;type<3;++type){
        auto b=Image();b.resize(0x302);Put32(b,0x104,b.size());b[0x10E]=1;b[0x108]=b[0x109]=255;
        b[0x12F]=type;Put32(b,0x124,type==0?0x1FFFF:0xFFFF);Put32(b,0x128,2);Rehash(b,0);
        CHECK(StageDsp1Image(b,p));CHECK(p->segments[0].target_words==(type==0?0x1FFFFU:0xFFFFU));
    }
    auto b=Image();b.resize(0x314);Put32(b,0x104,b.size());b[0x10E]=10;
    for(unsigned i=0;i<10;++i){const auto h=0x120+48*i;Put32(b,h,0x300+2*i);Put32(b,h+4,2*i);Put32(b,h+8,2);b[h+15]=0;Rehash(b,i);}
    CHECK(StageDsp1Image(b,p));CHECK(p->segments.size()==10 && p->program.known_bytes()==20);
    // Exactly the maximum component size is accepted; unexplained trailing bytes
    // are kept in the whole-image identity, never mapped as a guessed segment.
    b.resize(kDsp1MaxBytes);Put32(b,0x104,b.size());CHECK(StageDsp1Image(b,p));
}
void AllocationRollback(){
    const auto b=Image(true);std::unique_ptr<Dsp1Image> p;CHECK(StageDsp1Image(b,p));
    const auto identity=p.get();unsigned failed=0;bool succeeded=false;
    for(long n=0;n<64;++n){fail_after=n;const auto r=StageDsp1Image(b,p);fail_after=-1;
        if(r){succeeded=true;break;}
        ++failed;CHECK(r.error==Dsp1Error::Allocation && p.get()==identity);
    }
    CHECK(failed>=7 && succeeded);
}
void DeterministicHeaderMutations(){
    const auto seed=Image(true);std::unique_ptr<Dsp1Image> p;CHECK(StageDsp1Image(seed,p));
    for(unsigned i=0;i<0x200;++i){auto b=seed;b[0x100+i]^=0x81;
        const auto before=p.get();const auto r=StageDsp1Image(b,p);
        if(!r)CHECK(p.get()==before);
        else CHECK(p->program.known_bytes()+p->data.known_bytes()==32);
    }
}
}
int main(){ExactPlacementAndUnknownBytes();InvalidInputsAreTransactional();AddressBoundariesAndCounts();AllocationRollback();DeterministicHeaderMutations();
 if(failures)return EXIT_FAILURE;std::cout<<"PASS: bounded DSP1 image staging, five-dimensional range/hash checks, unknown gaps and allocation rollback; no boot or RSA claim\n";return EXIT_SUCCESS;}
