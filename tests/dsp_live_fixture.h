#pragma once
#include "services/dsp_live_device.h"
#include "services/dsp_discovery_service.h"
#include "dsp1_test_fixture.h"
#include <stdexcept>
namespace dsp_live_fixture {
using namespace lego::ctr;
inline void Put16(std::vector<std::uint8_t>& b,std::size_t at,std::uint16_t v){b.at(at)=v&255;b.at(at+1)=v>>8;}
inline std::vector<std::uint8_t> Program(bool fault=false,bool notify=false){
    // Firmware sends real startup words. Its final pipe-base word is 0x42.
    // Next instruction writes 0x7B to DATA word0x100, then sleeps in a branch loop.
    std::vector<std::uint16_t> words{0x2101,0xD4BC,0x80C0,0xD4BC,0x80C4,0xD4BC,0x80C8,
                                   0x2142,0xD4BC,0x80C8};
    if(fault){words.push_back(0xD4B8);words.push_back(0x1200);}
    else if(notify){words.push_back(0xD4BC);words.push_back(0x80C0);}
    else{words.push_back(0x217B);words.push_back(0xD4BC);words.push_back(0x0100);}
    words.push_back(0x57F0);
    std::vector<std::uint8_t> b;for(auto w:words){b.push_back(w&255);b.push_back(w>>8);}return b;
}
inline std::vector<std::uint8_t> Table(){
    std::vector<std::uint8_t> b(160);
    for(unsigned s=0;s<16;++s){Put16(b,s*10,static_cast<std::uint16_t>(0x200+s*8));Put16(b,s*10+2,8);b[s*10+8]=s;}
    return b;
}
inline Dsp1Image Image(bool fault=false,bool notify=false){
    Dsp1Image image;image.receive_startup_replies=true;
    if(!image.program.Stage(0,Program(fault,notify))||!image.data.Stage(0x84,Table()))throw std::runtime_error("fixture stage");
    std::vector<std::uint8_t> payload(16*16);for(unsigned i=0;i<payload.size();++i)payload[i]=(i*17+3)&255;
    if(!image.data.Stage(0x400,payload))throw std::runtime_error("fixture payload");return image;
}
inline std::shared_ptr<DspExecutionProbe> Boot(const Dsp1Image& image){
    const char* error=nullptr;auto p=DspExecutionProbe::Create(image,DspProbeReset::KnownOnly,error);
    if(!p || p->Advance(100)!=DspProbeState::ProtocolComplete)throw std::runtime_error(error?error:"fixture boot");
    return std::shared_ptr<DspExecutionProbe>(std::move(p));
}
inline std::unique_ptr<DspLiveDevice> Device(const std::shared_ptr<DspExecutionProbe>& p,std::uint64_t now=0){
    const char* error=nullptr;auto d=DspLiveDevice::Prepare(p,now,error);if(!d)throw std::runtime_error(error?error:"device prepare");return d;
}
inline std::vector<std::uint8_t> Container(bool fault=false,bool notify=false){
    const auto program=Program(fault,notify);auto data=Table();
    // Only the table is needed for boot; no payload is read before a transfer.
    const auto data_offset=0x300+program.size();std::vector<std::uint8_t> b(data_offset+data.size());
    b[0x100]='D';b[0x101]='S';b[0x102]='P';b[0x103]='1';dsp_fixture::Put32(b,0x104,b.size());
    b[0x108]=1;b[0x109]=1;b[0x10E]=2;b[0x10F]=1;
    dsp_fixture::Put32(b,0x120,0x300);dsp_fixture::Put32(b,0x128,program.size());
    const auto s=0x150;dsp_fixture::Put32(b,s,data_offset);dsp_fixture::Put32(b,s+4,0x42);dsp_fixture::Put32(b,s+8,data.size());b[s+15]=2;
    std::copy(program.begin(),program.end(),b.begin()+0x300);std::copy(data.begin(),data.end(),b.begin()+data_offset);
    dsp_fixture::Rehash(b,0);dsp_fixture::Rehash(b,1);return b;
}
} // namespace dsp_live_fixture
