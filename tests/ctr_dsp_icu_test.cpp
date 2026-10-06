#include "../vendor/teakra-3d697a1/src/icu.h"
#include "teakra/teakra.h"
#include "teakra/impl/register.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using namespace Teakra;
int failures{};
#define CHECK(x) do { if (!(x)) { ++failures; std::cerr << "FAIL " << __LINE__ << ": " #x "\n"; } } while(0)
struct Fixture {
    CoreTiming time;
    ICU icu{time};
    std::array<unsigned,3> calls{};
    std::vector<std::pair<u32,bool>> vectors;
    Fixture() {
        icu.SetInterruptHandler([this](u32 n){ ++calls[n]; CHECK(icu.GetRequest()!=0); },
            [this](u32 a,bool c){ vectors.emplace_back(a,c); CHECK(icu.GetRequest()!=0); });
    }
    void ticks(unsigned n=3) { for(unsigned i=0;i<n;++i)time.Tick(); }
    void clean() {
        icu.SetPolarity(0); icu.SetLine(13,false); icu.Trigger(0); ticks(); icu.Acknowledge(0xFFFF);
    }
};
void ResetAndRegisterBindings() {
    std::vector<u8> raw(0x80000,0); ::Teakra::Teakra dsp{{raw.data()}};
    dsp.Reset();
    CHECK(dsp.MMIORead(0x20E)==0x2000 && dsp.MMIORead(0x210)==0x2000);
    CHECK(dsp.MMIORead(0x252)==0 && dsp.MMIORead(0x204)==0 && dsp.MMIORead(0x200)==0);
    for(unsigned i=0;i<16;++i) {
        CHECK(dsp.MMIORead(0x212+4*i)==3 && dsp.MMIORead(0x214+4*i)==0xFC00);
        dsp.MMIOWrite(0x212+4*i,0xFFFF); dsp.MMIOWrite(0x214+4*i,u16(i*123));
        CHECK(dsp.MMIORead(0x212+4*i)==0x8003 && dsp.MMIORead(0x214+4*i)==i*123);
    }
    dsp.MMIOWrite(0x20E,0xFFFF); dsp.MMIOWrite(0x210,0xABCD); dsp.MMIOWrite(0x252,0xFFFF);
    CHECK(dsp.MMIORead(0x20E)==0xFFFF && dsp.MMIORead(0x210)==0xABCD && dsp.MMIORead(0x252)==0xFFFF);
    dsp.Reset();
    CHECK(dsp.MMIORead(0x20E)==0x2000 && dsp.MMIORead(0x210)==0x2000);
    CHECK(dsp.MMIORead(0x252)==0 && dsp.MMIORead(0x204)==0);
    for(unsigned i=0;i<16;++i)CHECK(dsp.MMIORead(0x212+i*4)==3 && dsp.MMIORead(0x214+i*4)==0xFC00);
    // Explicit manual write has a real readback. Pending is not the same register.
    dsp.MMIOWrite(0x20E,1);dsp.MMIOWrite(0x204,1);
    CHECK(dsp.MMIORead(0x204)==1 && dsp.MMIORead(0x200)==0);
    dsp.Run(2);CHECK(dsp.MMIORead(0x200)==0);
    dsp.Run(1);CHECK(dsp.MMIORead(0x200)==1);
    dsp.MMIOWrite(0x202,1);CHECK(dsp.MMIORead(0x200)==0);
    dsp.Run(4);CHECK(dsp.MMIORead(0x200)==0); // No fabricated repeated edge.
    // Other unknown cells retain fail-closed access, not generic register storage.
    bool threw=false;try{(void)dsp.MMIORead(0x254);}catch(const std::exception&){threw=true;}CHECK(threw);
}
void EveryManualBitAndMode() {
    for(unsigned bit=0;bit<16;++bit)for(bool edge:{false,true}) {
        Fixture f;f.clean();const auto m=u16(1U<<bit);
        f.icu.SetTriggerMode(edge?m:0);f.icu.SetEnable(1,m);
        f.icu.Trigger(m);CHECK(f.icu.GetTrigger()==m && f.icu.GetRequest()==0);
        CHECK(f.icu.GetMaxSkip()==0);f.ticks(2);CHECK(f.icu.GetRequest()==0);
        f.ticks(1);CHECK(f.icu.GetRequest()==m && f.calls[1]==1);
        CHECK(f.icu.GetMaxSkip()==CoreTiming::Callbacks::Infinity);
        f.icu.Acknowledge(m);CHECK(f.icu.GetRequest()==(edge?0:m));
        CHECK(f.calls[1]==(edge?1U:2U)); // Held level cannot be acknowledged.
        f.icu.Trigger(m);f.ticks();CHECK(f.icu.GetRequest()==(edge?0:m));
        f.icu.Trigger(0);CHECK(f.icu.GetTrigger()==0);f.ticks();
        // Falling level never automatically erases a latched pending flag.
        CHECK(f.icu.GetRequest()==(edge?0:m));f.icu.Acknowledge(m);CHECK(f.icu.GetRequest()==0);
        f.icu.Trigger(m);f.ticks();CHECK(f.icu.GetRequest()==m);
        f.icu.Acknowledge(u16(~m));CHECK(f.icu.GetRequest()==m);
    }
}
void PolarityMasterAndPendingRoutes() {
    Fixture f;f.clean();const u16 m=1U<<10;
    f.icu.SetTriggerMode(m);f.icu.SetPolarity(m);f.ticks();
    CHECK((f.icu.GetRequest()&m)!=0); // Inversion is applied before the manual OR.
    f.icu.Acknowledge(0xFFFF);f.icu.Trigger(m);f.ticks();CHECK(f.icu.GetRequest()==0);
    f.icu.SetLine(10,true);CHECK(f.icu.GetRequest()==0); // Manual still holds asserted.
    f.icu.Trigger(0);f.ticks();f.icu.SetLine(10,false);CHECK(f.icu.GetRequest()==m);
    f.icu.SetEnable(2,m);CHECK(f.calls[2]==1); // Route already-pending interrupt.
    f.icu.SetEnable(2,m);CHECK(f.calls[2]==1);
    f.icu.SetEnable(2,0);f.icu.SetEnable(2,m);CHECK(f.calls[2]==2);
    f.icu.SetMasterDisable(m);f.ticks();CHECK(f.icu.GetRequest()==m); // Mask is not ACK.
    f.icu.Acknowledge(m);CHECK(f.icu.GetRequest()==0);
    f.icu.Trigger(m);f.ticks();CHECK(f.icu.GetRequest()==0);
    f.icu.SetMasterDisable(0);f.ticks();CHECK(f.icu.GetRequest()==m);
    Fixture physical;physical.clean();physical.icu.SetTriggerMode(m);physical.icu.SetEnable(0,m);
    physical.icu.SetLine(10,true);CHECK(physical.icu.GetRequest()==m && physical.calls[0]==1);
    physical.icu.Acknowledge(m);physical.icu.SetLine(10,true);CHECK(physical.icu.GetRequest()==0);
    physical.icu.SetLine(10,false);physical.icu.SetLine(10,true);CHECK(physical.calls[0]==2);
    physical.icu.SetLine(10,false);physical.icu.Acknowledge(m);
    physical.icu.TriggerSingle(10);CHECK(physical.icu.GetRequest()==m && physical.calls[0]==3);
}
void PipelineAndRouting() {
    Fixture f;f.clean();f.icu.SetTriggerMode(0xFFFF);f.icu.SetEnable(0,1);f.icu.SetEnable(2,1);
    f.icu.Trigger(1);f.ticks(1);f.icu.Trigger(0);f.ticks(1);CHECK(f.icu.GetRequest()==0);
    f.ticks(1);CHECK(f.icu.GetRequest()==1 && f.calls[0]==1 && f.calls[2]==1);
    f.icu.Acknowledge(1);f.ticks(1);CHECK(f.icu.GetRequest()==0);
    Fixture v;v.clean();v.icu.vector_low[5]=0x1234;v.icu.vector_high[5]=2;v.icu.vector_context_switch[5]=1;
    v.icu.SetEnableVectored(1U<<5);v.icu.Trigger(1U<<5);v.ticks();
    CHECK(v.vectors.size()==1 && v.vectors[0].first==0x21234 && v.vectors[0].second);
    // IRQ arbitration beyond the pinned core's single vector slot remains explicit.
    Fixture collision;collision.clean();collision.icu.SetEnableVectored(3);collision.icu.Trigger(3);
    bool threw=false;try{collision.ticks();}catch(const std::runtime_error&){threw=true;}CHECK(threw);
    for(auto index:{16U,32U,0xFFFFFFFFU}) {
        auto before=f.icu.GetRequest();threw=false;try{f.icu.SetLine(index,true);}catch(const std::out_of_range&){threw=true;}
        CHECK(threw && f.icu.GetRequest()==before);
    }
    bool threw2=false;try{f.icu.SetEnable(3,1);}catch(const std::out_of_range&){threw2=true;}CHECK(threw2);
    f.icu.Trigger(1);threw2=false;try{f.icu.Skip(1);}catch(const std::logic_error&){threw2=true;}CHECK(threw2);
}
void ActualCoreDispatchAndReset() {
    std::vector<u8> raw(0x80000,0); ::Teakra::Teakra dsp{{raw.data()}}; dsp.Reset();
    // At fixed core-int0 vector: a0l=0x55; send actual mailbox0 word.
    const std::array<u16,3> handler{0x2155,0xD4BC,0x80C0};
    for(unsigned i=0;i<handler.size();++i){raw[12+2*i]=handler[i]&255;raw[13+2*i]=handler[i]>>8;}
    auto& regs=dsp.GetRegisterState();regs.pc=0x100;regs.sp=0x800;regs.ie=1;regs.im[0]=1;
    dsp.MMIOWrite(0x20E,1);dsp.MMIOWrite(0x206,1);dsp.MMIOWrite(0x204,1);
    dsp.Run(3);CHECK(regs.pc==0x103 && !dsp.RecvDataIsReady(0));
    dsp.Run(1);CHECK(regs.pc==6 && regs.ie==0 && regs.sp==0x7FE);
    dsp.Run(2);CHECK(dsp.RecvDataIsReady(0) && dsp.RecvData(0)==0x55);
    dsp.MMIOWrite(0x202,1);CHECK(dsp.MMIORead(0x200)==0);
    // Queue notifications but reset before the interpreter consumes them. The
    // ICU and the core's private notification slots must both be reset.
    dsp.Reset();dsp.MMIOWrite(0x206,1);dsp.MMIOWrite(0x204,1);dsp.Run(3);
    dsp.Reset();dsp.Run(1);CHECK(dsp.GetRegisterState().ip[0]==0 && dsp.MMIORead(0x200)==0);
    dsp.Reset();dsp.MMIOWrite(0x20C,1);dsp.MMIOWrite(0x204,1);dsp.Run(3);
    dsp.Reset();dsp.Run(1);CHECK(dsp.GetRegisterState().ipv==0 && dsp.MMIORead(0x200)==0);
    // The pinned core has only one vector slot. A second, separately queued
    // vector must not overwrite the first before consumption.
    dsp.Reset();dsp.MMIOWrite(0x20E,3);dsp.MMIOWrite(0x20C,3);dsp.MMIOWrite(0x204,1);dsp.Run(3);
    dsp.MMIOWrite(0x204,2);dsp.Run(2);
    bool threw=false;try{dsp.Run(1);}catch(const std::runtime_error&){threw=true;}CHECK(threw);
}
void ActualFirmwareInstructionsUseIcu() {
    // This is a synthetic program, never original game data: a0l=1; write manual
    // request; two NOPs; read pending into a0. No ready response is injected.
    std::vector<u8> raw(0x80000,0);const std::array<u16,7> words{0x2101,0xD4BC,0x8204,0,0,0xD4B8,0x8200};
    ::Teakra::Teakra dsp{{raw.data()}};dsp.Reset();
    for(unsigned i=0;i<words.size();++i){raw[2*i]=words[i]&255;raw[2*i+1]=words[i]>>8;}
    dsp.Run(4);CHECK(dsp.MMIORead(0x200)==1 && dsp.MMIORead(0x204)==1);
    dsp.Run(1);CHECK(dsp.GetRegisterState().a[0]==1);
    CHECK(!dsp.RecvDataIsReady(0) && !dsp.RecvDataIsReady(1) && !dsp.RecvDataIsReady(2));
}
}
int main(){
 try{ResetAndRegisterBindings();EveryManualBitAndMode();PolarityMasterAndPendingRoutes();PipelineAndRouting();ActualCoreDispatchAndReset();ActualFirmwareInstructionsUseIcu();}
 catch(const std::exception& e){std::cerr<<"exception: "<<e.what()<<"\n";return EXIT_FAILURE;}
 if(failures)return EXIT_FAILURE;
 std::cout<<"PASS: ICU edge/level, polarity, manual pipeline, acknowledge, routes, reset and actual DSP accesses\n";
}
