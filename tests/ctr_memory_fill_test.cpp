#include "services/gsp_memory_fill.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <new>

// Test-only allocation failure proves preparation does not mutate device state.
namespace { bool fail_alloc=false; }
void* operator new(std::size_t n) { if(fail_alloc)throw std::bad_alloc(); if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc(); }
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
using namespace lego::ctr;
namespace {
int failures=0;
#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}}while(0)
constexpr auto base=kGpuVramVirtualBase;
MemoryFillRequest Request(std::uint32_t size=0xC8000,std::uint32_t value=0,std::uint16_t control=0x201) {
    MemoryFillRequest r; r.channels[0]={base+0x70800,value,base+0x70800+size,control};
    r.channels[1]={0,0,0xC8000,0x201}; return r;
}
void PatternsAndStaging() {
    auto bank=GpuVramBank::ReferenceZero();
    std::vector<std::uint8_t> sentinel(kGpuVramBytes,0xA5);CHECK(bank->Write(0,sentinel));
    PicaGpuRegisters regs{};regs.fill(0x1234ABCD);
    for(auto control:{0x0001U,0x0201U,0x0101U,0x0301U}) {
        const auto width=(control&0x100)?3U:(control&0x200)?4U:2U;
        auto request=Request(24,0xB734129EU,static_cast<std::uint16_t>(control));
        MemoryFillPlan plan;const char* error=nullptr;
        CHECK(StageMemoryFill(request,bank.get(),regs,plan,error)&&!error);
        CHECK(plan.irqs==1&&plan.channels[0].interrupt==0&&!plan.channels[1].enabled);
        CHECK(plan.channels[0].setup[0]==(0x18070800U>>3));
        CHECK(plan.registers[7]==((control&~1U)|2U));
        for(unsigned i=0;i<plan.channels[0].output.size();++i)
            CHECK(plan.channels[0].output[i]==static_cast<std::uint8_t>(request.channels[0].value>>(8*(i%width))));
        for(unsigned i=0;i<regs.size();++i)if(i<4||i>7)CHECK(plan.registers[i]==regs[i]);
        CHECK(std::equal(bank->bytes().begin(),bank->bytes().end(),sentinel.begin()));
        // Commit of validated prepared bytes has no allocation.
        fail_alloc=true;const bool written=bank->Write(plan.channels[0].offset,plan.channels[0].output);fail_alloc=false;
        CHECK(written);CHECK(bank->Write(0,sentinel));
    }
    MemoryFillPlan plan;const char* error=nullptr;
    CHECK(StageMemoryFill(Request(),bank.get(),regs,plan,error));
    CHECK(plan.channels[0].output.size()==819200&&std::all_of(plan.channels[0].output.begin(),plan.channels[0].output.end(),[](auto v){return v==0;}));
    auto r=Request(kMemoryFillMaxBytes,0x3A6CB912);CHECK(StageMemoryFill(r,bank.get(),regs,plan,error));CHECK(plan.channels[0].output.size()==kMemoryFillMaxBytes);
    r.channels[0].start=base+kGpuVramBytes-8;r.channels[0].end=base+kGpuVramBytes;
    CHECK(StageMemoryFill(r,bank.get(),regs,plan,error)&&plan.channels[0].output.size()==8);
}
void ChannelsAndControls() {
    auto bank=GpuVramBank::ReferenceZero();PicaGpuRegisters regs{};regs.fill(0xA5A5A5A5);MemoryFillPlan plan;const char* error=nullptr;
    auto r=Request(8,0xAABBCCDD);r.channels[1]={0,~0U,~0U,0xFFFF};
    CHECK(StageMemoryFill(r,bank.get(),regs,plan,error)&&!plan.channels[1].enabled);
    r.channels[1]={base+0x400,0x55667788,base+0x408,0x201};
    CHECK(StageMemoryFill(r,bank.get(),regs,plan,error));CHECK(plan.irqs==1&&plan.channels[0].interrupt==-1&&plan.channels[1].interrupt==0);
    r.channels[0].start=0;CHECK(StageMemoryFill(r,bank.get(),regs,plan,error));CHECK(plan.irqs==1&&!plan.channels[0].enabled&&plan.channels[1].interrupt==1);
    r=Request(8);r.channels[1]={base+0x400,0x55667788,base+0x408,0x202};
    CHECK(StageMemoryFill(r,bank.get(),regs,plan,error));CHECK(plan.irqs==0&&plan.channels[0].triggered&&!plan.channels[1].triggered&&plan.registers[11]==0x202);
    r.channels[0].control=0x200;r.channels[1].control=0;
    CHECK(StageMemoryFill(r,nullptr,regs,plan,error));CHECK(plan.irqs==0&&plan.channels[0].output.empty()&&plan.registers[7]==0x200);
    r.channels[0].start=0;r.channels[1].start=0;
    CHECK(StageMemoryFill(r,nullptr,regs,plan,error)&&plan.irqs==0&&plan.registers==regs);
}
void FailureAtomicity() {
    auto bank=GpuVramBank::ReferenceZero();PicaGpuRegisters regs{};MemoryFillPlan plan;const char* error=nullptr;
    CHECK(StageMemoryFill(Request(8,0x11223344),bank.get(),regs,plan,error));const auto previous=plan.registers;const auto previous_bytes=plan.channels[0].output;
    for(unsigned test=0;test<10;++test){auto r=Request(8);switch(test){
        case 0:r.channels[0].start++;break;case 1:r.channels[0].end++;break;
        case 2:r.channels[0].end=r.channels[0].start;break;case 3:r.channels[0].end=r.channels[0].start-8;break;
        case 4:r.channels[0].start=0x18000000;break;case 5:r.channels[0].end=0xFFFFFFF8;break;
        case 6:r.channels[0].control=0x401;break;case 7:r.channels[0].end=r.channels[0].start+kMemoryFillMaxBytes+8;break;
        case 8:r.channels[0].control=0x101;break;case 9:r.channels[1]={base+8,0,base+7,0x201};break;}
        CHECK(!StageMemoryFill(r,bank.get(),regs,plan,error)&&error);CHECK(plan.registers==previous&&plan.channels[0].output==previous_bytes);
    }
    CHECK(!StageMemoryFill(Request(),nullptr,regs,plan,error));
    bool threw=false;fail_alloc=true;try{StageMemoryFill(Request(),bank.get(),regs,plan,error);}catch(const std::bad_alloc&){threw=true;}fail_alloc=false;
    CHECK(threw&&plan.registers==previous&&plan.channels[0].output==previous_bytes);
    CHECK(std::all_of(bank->bytes().begin(),bank->bytes().end(),[](auto v){return v==0;}));
}
}
int main(){PatternsAndStaging();ChannelsAndControls();FailureAtomicity();if(failures)return EXIT_FAILURE;std::cout<<"PASS: staged VRAM fill patterns, channel/IRQ selection, boundaries and allocation rollback\n";}
