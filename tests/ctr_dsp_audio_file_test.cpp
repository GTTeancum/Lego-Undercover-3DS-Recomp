#include "host/dsp_audio_file.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <vector>
#ifdef LEGO_TEST_WRAP_STDIO
namespace { int fail_stdio_operation=0; }
extern "C" std::size_t __real_fwrite(const void*,std::size_t,std::size_t,std::FILE*);
extern "C" int __real_fflush(std::FILE*);
extern "C" int __real_fclose(std::FILE*);
extern "C" std::size_t __wrap_fwrite(const void* p,std::size_t size,std::size_t count,std::FILE* f) {
    if(fail_stdio_operation==1){fail_stdio_operation=0;return __real_fwrite(p,size,count/2,f);}
    return __real_fwrite(p,size,count,f);
}
extern "C" int __wrap_fflush(std::FILE* f) {
    if(fail_stdio_operation==2){fail_stdio_operation=0;(void)__real_fflush(f);return EOF;}
    return __real_fflush(f);
}
extern "C" int __wrap_fclose(std::FILE* f) {
    if(fail_stdio_operation==3){fail_stdio_operation=0;(void)__real_fclose(f);return EOF;}
    return __real_fclose(f);
}
#endif
using namespace lego::ctr;
namespace {
int failures{};
#define CHECK(x) do{if(!(x)){++failures;std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";}}while(0)
std::vector<std::uint8_t> Read(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
std::uint64_t Word(const std::vector<std::uint8_t>& b,std::size_t at,unsigned n){std::uint64_t v{};for(unsigned i=0;i<n;++i)v|=std::uint64_t(b.at(at+i))<<(i*8);return v;}
struct TestDirectory {
    std::filesystem::path path;
    TestDirectory(){const auto base=std::filesystem::temp_directory_path();
        for(unsigned i=0;i<100;++i){auto name="lego-dsp-sink-"+std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count())+"-"+std::to_string(i);path=base/name;
            if(std::filesystem::create_directory(path))return;}
        throw std::runtime_error("cannot create own test directory");}
    ~TestDirectory(){std::error_code ec;std::filesystem::remove_all(path,ec);}
};
Dsp1Image Program(){
    std::vector<std::uint16_t> w;auto store=[&](std::uint16_t a,std::uint16_t v){w.insert(w.end(),{0x5e18,v,0xd4bc,a});};
    store(0x80c8,0x42);store(0x82a0,15);store(0x82a2,0x1004);store(0x82a4,4);store(0x82a6,0x21);
    store(0x82a8,0);store(0x82aa,0);store(0x82ac,0);store(0x82c6,0x1234);store(0x82c6,0xfedc);store(0x82be,0x8000);w.push_back(0x57f0);
    std::vector<std::uint8_t>b;for(auto x:w){b.push_back(x);b.push_back(x>>8);}Dsp1Image image;CHECK(image.program.Stage(0,b));return image;
}
struct Counter : DspAudioSink {
    std::uint64_t n{},last{};std::uint64_t limit{5000};
    bool Write(const DspCapturedAudioFrame& f) noexcept override {
        if(n>=limit)return false;
        CHECK(f.during_run_call>last);
        if(n==0){CHECK(f.fifo_mask==3&&f.samples[0]==0x1234&&static_cast<std::uint16_t>(f.samples[1])==0xfedc);}
        else {CHECK(f.fifo_mask==0&&(f.samples==std::array<std::int16_t,2>{}));}
        last=f.during_run_call;++n;return true;
    }
};
void RecordsAndBounds(){
    TestDirectory dir;const auto path=dir.path/"output.dspaud";
    auto sink=lego::host::DspAudioFile::Open(path,5000);
    for(unsigned i=0;i<5000;++i)CHECK(sink->Write({{static_cast<std::int16_t>(i),static_cast<std::int16_t>(65535-i)},static_cast<std::uint8_t>(i&3),0x1234567800000000ULL+i}));
    CHECK(sink->frames()==5000&&!sink->failed());CHECK(sink->Close()&&sink->Close());
    CHECK(!sink->Write({}));auto data=Read(path);CHECK(data.size()==32+5000*16);
    CHECK(std::string(data.begin(),data.begin()+7)=="DSPAUD1"&&Word(data,8,4)==1&&Word(data,12,4)==16);
    for(unsigned i=0;i<5000;++i){const auto p=32+i*16;
        CHECK(Word(data,p,2)==i&&Word(data,p+2,2)==65535-i&&data[p+4]==(i&3));
        CHECK(Word(data,p+5,3)==0&&Word(data,p+8,8)==0x1234567800000000ULL+i);}
    bool threw=false;try{(void)lego::host::DspAudioFile::Open(path);}catch(const std::runtime_error&){threw=true;}
    CHECK(threw&&Read(path)==data);
    auto capped=lego::host::DspAudioFile::Open(dir.path/"capped",1);CHECK(capped->Write({{1,2},3,1}));
    CHECK(!capped->Write({{3,4},3,2})&&capped->failed()&&capped->frames()==1&&!capped->Close());
    CHECK(Read(dir.path/"capped").size()==48);
    auto invalid=lego::host::DspAudioFile::Open(dir.path/"invalid");CHECK(!invalid->Write({{1,2},4,1}));CHECK(!invalid->Close());
    CHECK(Read(dir.path/"invalid").size()==32);
    auto reverse=lego::host::DspAudioFile::Open(dir.path/"reverse");CHECK(reverse->Write({{1,2},3,9}));
    CHECK(!reverse->Write({{1,2},3,8})&&!reverse->Close());
    bool missing=false;try{(void)lego::host::DspAudioFile::Open(dir.path/"missing"/"file");}catch(const std::runtime_error&){missing=true;}CHECK(missing);
#ifndef _WIN32
    // Exclusive creation refuses an existing symlink, not just regular files.
    std::filesystem::create_symlink(path,dir.path/"symlink");bool linked=false;
    try{(void)lego::host::DspAudioFile::Open(dir.path/"symlink");}catch(const std::runtime_error&){linked=true;}
    CHECK(linked&&Read(path)==data);
#endif
}
void StdioFaults(){
#ifdef LEGO_TEST_WRAP_STDIO
    TestDirectory dir;
    for(int op=1;op<=3;++op){
        const auto path=dir.path/("fault-"+std::to_string(op));auto f=lego::host::DspAudioFile::Open(path);
        fail_stdio_operation=op;
        if(op<3){CHECK(!f->Write({{1,2},3,4}));CHECK(f->failed()&&f->frames()==0&&!f->Close());}
        else {CHECK(f->Write({{1,2},3,4}));CHECK(!f->Close()&&f->failed()&&f->frames()==1);}
        CHECK(!f->Write({{3,4},3,5}));CHECK(Read(path).size()==(op==1?40U:48U));
    }
#endif
}
void ActualExecutorAndFailure(){
    auto image=Program();auto sink=std::make_shared<Counter>();const char* error{};
    auto p=DspExecutionProbe::Create(image,DspProbeReset::KnownOnly,error,true,true,true,DspBootMode::Immediate,sink);
    CHECK(p&&p->Advance(100)==DspProbeState::ProtocolComplete);
    for(unsigned i=0;i<180&&sink->n<4200;++i)CHECK(p->ContinueLive(100000));
    CHECK(sink->n>4096&&p->emitted_audio_frames()==sink->n&&p->captured_audio().size()==4096);
    CHECK(p->summary().state==DspProbeState::ProtocolComplete);
    sink->limit=sink->n;
    CHECK(!p->ContinueLive(5000));CHECK(p->summary().fault==DspProbeFault::Audio);
    const auto before=p->summary();CHECK(!p->ContinueLive(5000)&&p->summary()==before);
    CHECK(p->emitted_audio_frames()==sink->n);
    CHECK(!DspExecutionProbe::Create(image,DspProbeReset::KnownOnly,error,false,false,true,DspBootMode::Immediate,sink)&&error);
    // No reference fallback is enabled by attaching a sink.
    auto strict=std::make_shared<Counter>();auto q=DspExecutionProbe::Create(image,DspProbeReset::KnownOnly,error,true,false,true,DspBootMode::Immediate,strict);
    CHECK(q&&q->Advance(100)==DspProbeState::ProtocolComplete);CHECK(!q->ContinueLive(9000));CHECK(strict->n==1&&q->emitted_audio_frames()==1);
}
}
int main(){try{RecordsAndBounds();StdioFaults();ActualExecutorAndFailure();}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
if(failures)return 1;std::cout<<"PASS: lossless tagged file records, exclusive create, terminal errors, real executor beyond prefix and unchanged strict underrun\n";return 0;}
