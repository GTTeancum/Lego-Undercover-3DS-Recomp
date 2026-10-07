#include "services/dsp_execution_probe.h"
#include "runtime/ctr_runner.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using namespace lego::ctr;
int failures{};
#define CHECK(x) do { if(!(x)){ ++failures; std::cerr<<"FAIL "<<__LINE__<<": " #x "\n"; } } while(0)
Dsp1Image Image(const std::vector<std::uint16_t>& words,bool replies=false){
    Dsp1Image image;image.receive_startup_replies=replies;
    std::vector<std::uint8_t> bytes;
    for(auto w:words){bytes.push_back(w&255);bytes.push_back(w>>8);}
    if(!image.program.Stage(0,bytes))throw std::runtime_error("test program stage failed");
    return image;
}
std::unique_ptr<DspExecutionProbe> Start(const Dsp1Image& image,DspBootMode mode,
                                       bool capture=false,bool transmit=false){
    const char* error{};
    auto p=DspExecutionProbe::Create(image,DspProbeReset::KnownOnly,error,capture,false,transmit,mode);
    if(!p)throw std::runtime_error(error?error:"probe create failed");
    return p;
}
void ExactBoundaryAndPartitions(){
    // Real synthetic DSP instructions publish a pipe word BEFORE the next
    // initialization store. This is not an original-game instruction fixture.
    auto image=Image({0x2142,0xD4BC,0x80C8,0x217B,0xD4BC,0x0100,0x57F0});
    auto early=Start(image,DspBootMode::Immediate);
    CHECK(early->Advance(100)==DspProbeState::ProtocolComplete);
    CHECK(early->summary().completed_steps==2&&early->provenance()[0x40200]==0);
    for(auto chunk:{1U,7U,97U,1000U,16383U,16384U,100000U}){
        auto p=Start(image,DspBootMode::ReferenceSlice);const auto before=p->summary();
        CHECK(p->Advance(0)==DspProbeState::Paused&&p->summary()==before);
        while(p->summary().state==DspProbeState::Paused){
            const auto remaining=DspExecutionProbe::kReferenceBootSlice-p->summary().completed_steps;
            if(chunk<remaining){CHECK(p->Advance(chunk)==DspProbeState::Paused);CHECK(p->replies().empty());}
            else{CHECK(p->Advance(chunk)==DspProbeState::ProtocolComplete);}
        }
        CHECK(p->summary().completed_steps==16384&&p->summary().pipe_base==0x42);
        CHECK(p->summary().reply_count==1&&p->replies()[0].after_steps==16384);
        CHECK(p->memory()[0x40200]==0x7B&&p->provenance()[0x40200]==3);
        CHECK(p->provenance()[0x40202]==0); // Unwritten gaps did not become known.
        const auto final=p->summary();CHECK(p->Advance(1)==DspProbeState::ProtocolComplete&&p->summary()==final);
    }
    auto a=Start(image,DspBootMode::ReferenceSlice),b=Start(image,DspBootMode::ReferenceSlice);
    CHECK(a->Advance(16383)==DspProbeState::Paused&&a->summary().reply_count==0);
    CHECK(a->Advance(1)==DspProbeState::ProtocolComplete);
    CHECK(b->Advance(100000)==DspProbeState::ProtocolComplete);
    CHECK(a->summary()==b->summary());
    CHECK(std::equal(a->memory().begin(),a->memory().end(),b->memory().begin()));
    CHECK(std::equal(a->provenance().begin(),a->provenance().end(),b->provenance().begin()));
}
void FaultsAndSeparatePipeWord(){
    // An available mailbox word does not excuse a fault later in that boot batch.
    auto image=Image({0x2142,0xD4BC,0x80C8,0xD4B8,0x0100});
    auto p=Start(image,DspBootMode::ReferenceSlice);
    CHECK(p->Advance(16384)==DspProbeState::Fault);
    CHECK(p->summary().completed_steps==2&&p->summary().attempted_steps==3);
    CHECK(p->summary().fault==DspProbeFault::UnknownSram&&p->replies().empty());
    auto final=p->summary();CHECK(p->Advance(100000)==DspProbeState::Fault&&p->summary()==final);
    auto ready=Image({0x2101,0xD4BC,0x80C0,0xD4BC,0x80C4,0xD4BC,0x80C8,0x57F0},true);
    p=Start(ready,DspBootMode::ReferenceSlice);
    CHECK(p->Advance(16384)==DspProbeState::Paused);
    CHECK(p->summary().reply_count==3&&!p->summary().has_pipe_base);
    for(auto reply:p->replies())CHECK(reply.word==1&&reply.after_steps==16384);
    CHECK(p->Advance(16384)==DspProbeState::Paused&&p->summary().reply_count==3);
    CHECK(!p->summary().has_pipe_base); // reg2's readiness is NOT also a pipe base.
    auto loop=Image({0x57F0});p=Start(loop,DspBootMode::ReferenceSlice);
    CHECK(p->Advance(0xFFFFFFFF)==DspProbeState::Paused);
    CHECK(p->summary().completed_steps==100000&&p->replies().empty());
    const char* error{};
    CHECK(!DspExecutionProbe::Create(loop,DspProbeReset::KnownOnly,error,false,false,false,
                                    static_cast<DspBootMode>(9))&&error);
}
void RealOutputDuringBatch(){
    std::vector<std::uint16_t> words;
    auto store=[&](std::uint16_t a,std::uint16_t v){words.insert(words.end(),{0x5E18,v,0xD4BC,a});};
    store(0x80C8,0x42);store(0x82A0,15);store(0x82A2,0x1004);
    for(unsigned i=0;i<5;++i)store(0x82A4+2*i,i==0?4:i==1?0x21:0);
    for(unsigned i=0;i<16;++i)store(0x82C6,0x1200+i);
    store(0x82BE,0x8000);words.push_back(0x57F0);const auto image=Image(words);
    auto p=Start(image,DspBootMode::ReferenceSlice,true,true);
    CHECK(p->Advance(16384)==DspProbeState::ProtocolComplete);
    CHECK(p->captured_audio().size()==3);
    for(unsigned i=0;i<p->captured_audio().size();++i){
        const auto& f=p->captured_audio()[i];CHECK(f.fifo_mask==3);
        CHECK(f.samples[0]==0x1200+2*i&&f.samples[1]==0x1201+2*i);
        CHECK(f.during_run_call<16384);
    }
    auto no_sink=Start(image,DspBootMode::ReferenceSlice,false,true);
    CHECK(no_sink->Advance(16384)==DspProbeState::Fault);
    CHECK(no_sink->summary().fault==DspProbeFault::Audio&&no_sink->replies().empty());
    CHECK(no_sink->captured_audio().empty());
}
void RunnerGuards(){
    for(unsigned variant=0;variant<4;++variant){
        Kernel k;GuestMemory m;const a32::Registry registry{};DspProbeOptions o{};
        o.boot_mode=variant==3?static_cast<DspBootMode>(99):DspBootMode::ReferenceSlice;
        o.enabled=variant!=0;o.live=variant!=1;
        bool threw=false;
        try{NativeRunner r(registry,m,k,kDefaultRtcMsSince1900,{},PtmStepMode::Unconfigured,{},
                           GpuVramMode::Unconfigured,DisplayClockMode::Disabled,CfgProfile::Unconfigured,
                           variant==2?CpuExecutionMode::Strict:CpuExecutionMode::DiagnosticDual,{},o);}
        catch(const std::invalid_argument&){threw=true;}
        CHECK(threw&&k.now_ns()==0&&k.threads().size()==1);
    }
}
}
int main(){
    try{ExactBoundaryAndPartitions();FaultsAndSeparatePipeWord();RealOutputDuringBatch();RunnerGuards();}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
    if(failures)return 1;
    std::cout<<"PASS: reference-sized boot polls, partial budgets, real writes/output, distinct replies and fail-closed modes\n";
    return 0;
}
