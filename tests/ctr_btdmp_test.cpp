#include "../vendor/teakra-3d697a1/src/btdmp.h"
#include "teakra/teakra.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
using Teakra::Btdmp;using Teakra::CoreTiming;
int failures{};
#define CHECK(x) do { if(!(x)){++failures;std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";} }while(0)
template<class F> bool Throws(F&& f){try{f();}catch(const std::exception&){return true;}return false;}
void Configure(Btdmp& p){p.SetReferenceTransmitProfile(true);p.SetTransmitControl(15);p.SetTransmitClockConfig(0x1004);for(unsigned i=0;i<5;++i)p.SetTransmitReferenceSetup(i,i==0?4:i==1?0x21:0);}
struct Frame {std::array<std::int16_t,2> samples;unsigned mask;bool operator==(const Frame&)const=default;};
struct F {
 CoreTiming clock; Btdmp p{clock};std::vector<Frame> frames;unsigned irqs{};
 F(){p.Reset();p.SetAudioCaptureCallback([this](auto samples,auto mask){frames.push_back({samples,mask});},false);p.SetInterruptHandler([this]{++irqs;});}
 void Tick(unsigned n){for(unsigned i=0;i<n;++i)clock.Tick();}
};
void SetupAndReset(){
 F f;CHECK(f.p.GetTransmitControl()==5&&f.p.GetTransmitClockConfig()==0&&f.p.GetTransmitEnable()==0);
 CHECK(f.p.GetTransmitEmpty()==1&&f.p.GetTransmitFull()==0&&f.p.GetMaxSkip()==Btdmp::Infinity);
 CHECK(Throws([&]{f.p.SetTransmitControl(15);}));
 CHECK(f.p.GetTransmitControl()==5);
 f.p.SetReferenceTransmitProfile(true);
 for(unsigned v : {0U,1U,0x10FU,0x20FU,0xFFFFU}){CHECK(Throws([&]{f.p.SetTransmitControl(v);}));CHECK(f.p.GetTransmitControl()==5);}
 CHECK(Throws([&]{f.p.SetTransmitEnable(0x8000);}));CHECK(!f.p.GetTransmitEnable());
 f.p.SetTransmitControl(15);f.p.SetTransmitClockConfig(0x1004);
 for(unsigned i=0;i<5;++i){CHECK(Throws([&]{f.p.SetTransmitReferenceSetup(i,0xFFFF);}));f.p.SetTransmitReferenceSetup(i,i==0?4:i==1?0x21:0);}
 CHECK(f.p.GetTransmitReferenceSetup(0)==4&&f.p.GetTransmitReferenceSetup(1)==1);
 for(unsigned i=2;i<5;++i)CHECK(f.p.GetTransmitReferenceSetup(i)==0);
 CHECK(Throws([&]{f.p.SetTransmitReferenceSetup(5,0);}));CHECK(Throws([&]{(void)f.p.GetTransmitReferenceSetup(5);}));
 CHECK(Throws([&]{f.p.SetTransmitClockConfig(0x1005);}));CHECK(f.p.GetTransmitClockConfig()==0x1004);
 CHECK(Throws([&]{f.p.SetTransmitPeriod(0);}));CHECK(f.p.GetTransmitPeriod()==4096);
 f.p.SetTransmitEnable(0xFFFF);CHECK(f.p.GetTransmitEnable()==0x8000);
 CHECK(Throws([&]{f.p.SetTransmitControl(5);}));CHECK(Throws([&]{f.p.SetTransmitClockConfig(0);}));
 CHECK(Throws([&]{f.p.SetTransmitReferenceSetup(0,4);}));CHECK(Throws([&]{f.p.SetReferenceTransmitProfile(false);}));
 f.p.Send(0x1234);f.p.Send(0xFEDC);f.Tick(1);f.p.Reset();
 CHECK(f.p.GetTransmitControl()==5&&f.p.GetTransmitEmpty()&&f.p.GetTransmitEnable()==0&&f.p.GetTransmitPeriod()==4096);
 CHECK(f.p.GetTransmitReferenceSetup(2)==0x1FFF&&f.p.GetTransmitFlush()==0);
 Configure(f.p);f.p.SetTransmitPeriod(1);f.p.Send(7);f.p.Send(8);f.p.SetTransmitEnable(0x8000);f.Tick(1);
 CHECK(f.frames.size()==1&&f.frames[0].samples[0]==7&&f.frames[0].samples[1]==8&&f.irqs==1);
}
void FifoAndNoSilentConsumption(){
 F f;Configure(f.p);f.p.SetTransmitPeriod(2);
 for(unsigned i=0;i<16;++i)f.p.Send(0x8000+i);
 CHECK(f.p.GetTransmitFull()&&!f.p.GetTransmitEmpty());CHECK(Throws([&]{f.p.Send(0);}));
 f.p.SetTransmitFlush(0);CHECK(f.p.GetTransmitFull());f.Tick(20);CHECK(f.frames.empty());
 f.p.SetTransmitEnable(0x8000);f.Tick(15);CHECK(f.frames.size()==7&&f.irqs==0&&!f.p.GetTransmitEmpty());f.Tick(1);
 CHECK(f.frames.size()==8&&f.irqs==1&&f.p.GetTransmitEmpty());
 for(unsigned i=0;i<8;++i){CHECK(f.frames[i].mask==3);CHECK(static_cast<std::uint16_t>(f.frames[i].samples[0])==0x8000+2*i);CHECK(static_cast<std::uint16_t>(f.frames[i].samples[1])==0x8001+2*i);}
 CHECK(Throws([&]{f.Tick(2);}));CHECK(f.frames.size()==8&&f.irqs==1);
 f.p.SetTransmitEnable(0);f.p.Send(3);f.p.SetTransmitFlush(3);CHECK(!f.p.GetTransmitEmpty()&&f.p.GetTransmitFlush()==3);
 f.p.SetTransmitFlush(4);CHECK(f.p.GetTransmitEmpty()&&f.p.GetTransmitFlush()==0&&f.irqs==1);
 f.p.Send(0xBEEF);f.p.SetTransmitEnable(0x8000);CHECK(Throws([&]{f.Tick(2);}));
 CHECK(!f.p.GetTransmitEmpty()); // Half stereo frame preserved on strict underrun.
 f.p.SetAudioCaptureCallback([&](auto samples,auto mask){f.frames.push_back({samples,mask});},true);f.Tick(2);
 CHECK(f.frames.back().mask==1&&static_cast<std::uint16_t>(f.frames.back().samples[0])==0xBEEF&&f.frames.back().samples[1]==0&&f.irqs==2);
 f.Tick(4);CHECK(f.frames.back().mask==0&&f.frames.back().samples[0]==0&&f.irqs==2); // Silence is not an empty-transition IRQ.
 F absent;Configure(absent.p);absent.p.SetTransmitPeriod(1);absent.p.Send(1);absent.p.Send(2);absent.p.SetAudioCaptureCallback({},false);absent.p.SetTransmitEnable(0x8000);
 CHECK(Throws([&]{absent.Tick(1);}));CHECK(!absent.p.GetTransmitEmpty()&&absent.irqs==0);
 absent.p.SetAudioCaptureCallback([](auto,auto){throw std::runtime_error("sink failed");},false);
 CHECK(Throws([&]{absent.Tick(1);}));CHECK(absent.p.GetTransmitEmpty()&&absent.irqs==1); // Partial effects are real, not rollback.
}
void SkipAndAllPixelWords(){
 // Every 16-bit output word, nonzero and sign-bit cases. No game data.
 F a,b;Configure(a.p);Configure(b.p);a.p.SetTransmitPeriod(5);b.p.SetTransmitPeriod(5);a.p.SetTransmitEnable(0x8000);b.p.SetTransmitEnable(0x8000);
 for(unsigned v=0;v<65536;v+=2){a.p.Send(v);a.p.Send(v+1);b.p.Send(v);b.p.Send(v+1);a.Tick(5);CHECK(b.clock.Skip(1000)==4);CHECK(b.clock.Skip(1000)==0);b.Tick(1);}
 CHECK(a.frames==b.frames&&a.irqs==32768&&b.irqs==32768);
 CHECK(Throws([&]{a.p.Skip(5);}));CHECK(a.p.GetMaxSkip()==4);
 CHECK(a.clock.Skip(4)==4);CHECK(Throws([&]{a.Tick(1);}));CHECK(a.frames==b.frames);
}
void MmioAndPortIsolation(){
 std::vector<std::uint8_t> raw(0x80000,0);::Teakra::Teakra dsp{{raw.data()}};dsp.Reset();
 CHECK(Throws([&]{dsp.MMIOWrite(0x2A0,15);}));dsp.SetReferenceTransmitProfile(true);
 for(unsigned port=0;port<2;++port){auto x=port*0x80;dsp.MMIOWrite(0x2A0+x,15);dsp.MMIOWrite(0x2A2+x,0x1004);for(unsigned i=0;i<5;++i)dsp.MMIOWrite(0x2A4+x+2*i,i==0?4:i==1?0x21:0);
 CHECK(dsp.MMIORead(0x2A6+x)==1&&dsp.MMIORead(0x2A0+x)==15);}
 unsigned callbacks{};dsp.SetAudioCaptureCallback([&](auto samples,auto mask){CHECK(samples[0]==0x1234&&samples[1]==0x5678&&mask==3);++callbacks;},false);
 dsp.MMIOWrite(0x2C6,0x1234);dsp.MMIOWrite(0x2C6,0x5678);dsp.MMIOWrite(0x2BE,0x8000);
 dsp.Run(4096);CHECK(callbacks==1);CHECK((dsp.MMIORead(0x200)&(1U<<11))!=0);CHECK((dsp.MMIORead(0x200)&(1U<<12))==0);
 dsp.Reset();CHECK(dsp.MMIORead(0x2BE)==0&&dsp.MMIORead(0x33E)==0&&dsp.MMIORead(0x2C2)==0x10);
}
}
int main(){try{SetupAndReset();FifoAndNoSilentConsumption();SkipAndAllPixelWords();MmioAndPortIsolation();}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}if(failures)return 1;std::cout<<"PASS: explicit reference TX preset, FIFO output, strict underrun, tagged fallback, exact deadlines, reset and IRQ source\n";return 0;}
