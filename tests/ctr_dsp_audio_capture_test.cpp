#include "services/dsp_execution_probe.h"
#include "runtime/ctr_runner.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
using namespace lego::ctr;int failures{};
#define CHECK(x) do{if(!(x)){++failures;std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";}}while(0)
Dsp1Image Program(bool full=true){
 std::vector<std::uint16_t> w;
 auto store=[&](std::uint16_t a,std::uint16_t v){w.insert(w.end(),{0x5E18,v,0xD4BC,a});};
 store(0x80C8,0x42); // Synthetic firmware itself publishes its boot word.
 store(0x82A0,15);store(0x82A2,0x1004);store(0x82A4,4);store(0x82A6,0x21);store(0x82A8,0);store(0x82AA,0);store(0x82AC,0);
 store(0x82C6,0x1234);if(full)store(0x82C6,0xFEDC);store(0x82BE,0x8000);w.push_back(0x57F0); // Actual infinite branch, not host-filled NOP space.
 std::vector<std::uint8_t> bytes;for(auto value:w){bytes.push_back(value&255);bytes.push_back(value>>8);}Dsp1Image image;CHECK(image.program.Stage(0,bytes));return image;
}
std::unique_ptr<DspExecutionProbe> Start(bool capture,bool silence,bool ref=true,bool full=true){
 auto image=Program(full);const char* error{};auto p=DspExecutionProbe::Create(image,DspProbeReset::KnownOnly,error,capture,silence,ref);if(!p)throw std::runtime_error(error);
 CHECK(p->Advance(100)==DspProbeState::ProtocolComplete&&p->summary().pipe_base==0x42&&p->summary().reply_count==1);return p;
}
void ActualSamplesAndContinuation(){
 auto a=Start(true,false),b=Start(true,false);
 CHECK(a->ContinueLive(4200));for(unsigned i=0;i<4200;++i)CHECK(b->ContinueLive(1));
 CHECK(a->captured_audio().size()==1&&a->captured_audio()[0].fifo_mask==3);
 CHECK(a->captured_audio()[0].samples[0]==0x1234&&static_cast<std::uint16_t>(a->captured_audio()[0].samples[1])==0xFEDC);
 CHECK(a->captured_audio()[0].during_run_call>4096);
 CHECK(a->summary()==b->summary());CHECK(std::equal(a->captured_audio().begin(),a->captured_audio().end(),b->captured_audio().begin(),b->captured_audio().end()));
 CHECK(std::equal(a->memory().begin(),a->memory().end(),b->memory().begin()));
 CHECK(!a->ContinueLive(5000)&&a->summary().state==DspProbeState::Fault);
 CHECK(a->captured_audio().size()==1);const auto before=a->summary();CHECK(!a->ContinueLive(100)&&a->summary()==before);
 auto disabled=Start(false,false);CHECK(!disabled->ContinueLive(5000));CHECK(disabled->summary().fault==DspProbeFault::Audio&&disabled->captured_audio().empty());
 auto unconfigured=Start(false,false,false);CHECK(!unconfigured->ContinueLive(100));CHECK(unconfigured->summary().completed_steps==3&&unconfigured->captured_audio().empty());
}
void FallbackAndCapacity(){
 auto a=Start(true,true);CHECK(a->ContinueLive(9000));CHECK(a->captured_audio().size()==2&&a->captured_audio()[0].fifo_mask==3&&a->captured_audio()[1].fifo_mask==0);
 CHECK((a->captured_audio()[1].samples==std::array<std::int16_t,2>{}));
 for(unsigned i=0;i<200 && a->summary().state!=DspProbeState::Fault;++i)a->ContinueLive(100000);
 CHECK(a->summary().state==DspProbeState::Fault&&a->summary().fault==DspProbeFault::Audio);
 CHECK(a->captured_audio().size()==DspExecutionProbe::kAudioCaptureCapacity);
 CHECK(std::count_if(a->captured_audio().begin(),a->captured_audio().end(),[](const auto& f){return f.fifo_mask==3;})==1);
 for(std::size_t i=1;i<a->captured_audio().size();++i){CHECK(a->captured_audio()[i].fifo_mask==0);CHECK(a->captured_audio()[i].during_run_call>a->captured_audio()[i-1].during_run_call);}
 auto half=Start(true,true,true,false);CHECK(half->ContinueLive(4200));CHECK(half->captured_audio().size()==1&&half->captured_audio()[0].fifo_mask==1&&half->captured_audio()[0].samples[1]==0);
 auto strict=Start(true,false,true,false);CHECK(!strict->ContinueLive(4200)&&strict->captured_audio().empty());
 const char* error{};CHECK(!DspExecutionProbe::Create(Program(),DspProbeReset::KnownOnly,error,false,true,true)&&error);
}
void RunnerGuards(){
 Kernel k;GuestMemory m;const a32::Registry registry{};
 for(unsigned mode=0;mode<4;++mode){DspProbeOptions o{};o.enabled=true;o.live=true;
 if(mode==0)o.capture_audio=true;
 if(mode==1){o.reference_audio_silence=true;}
 if(mode==2){o.reference_transmit=true;o.live=false;}
 if(mode==3){o.capture_audio=true;o.reference_transmit=true;o.enabled=false;}
 bool threw=false;try{NativeRunner r(registry,m,k,kDefaultRtcMsSince1900,{},PtmStepMode::Unconfigured,{},GpuVramMode::Unconfigured,DisplayClockMode::Disabled,CfgProfile::Unconfigured,CpuExecutionMode::DiagnosticDual,{},o);}catch(const std::invalid_argument&){threw=true;}CHECK(threw);
 }
}
}
int main(){try{ActualSamplesAndContinuation();FallbackAndCapacity();RunnerGuards();}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}if(failures)return 1;std::cout<<"PASS: original synthetic DSP opcodes feed bounded tagged audio; no discarded overflow, false underflow sample or hidden mode selection\n";return 0;}
