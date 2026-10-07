#include "services/pica_startup.h"
#include <cstdlib>
#include <iostream>
#include <span>
#include <vector>
using namespace lego::ctr;
namespace {
int failures{};
#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}}while(0)
struct List {
    std::vector<std::uint8_t> bytes;
    void Word(std::uint32_t v){for(unsigned i=0;i<4;++i)bytes.push_back(v>>(i*8));}
    void Packet(unsigned id,std::uint32_t v,unsigned mask=15,
                std::initializer_list<std::uint32_t> extras={},bool grouped=false) {
        Word(v);Word(id|(mask<<16)|(std::uint32_t(extras.size())<<20)|(grouped?0x80000000U:0));
        for(auto x:extras)Word(x);if(extras.size()&1)Word(0xdeadbeef);
    }
};
constexpr std::array<unsigned,5> types{0,2,3,4,5};
std::span<const std::uint32_t> Words(const PicaUploadState& u,unsigned type) {
    switch(type){case 0:return u.procedural.noise.words;case 2:return u.procedural.color_map.words;
    case 3:return u.procedural.alpha_map.words;case 4:return u.procedural.color.words;
    default:return u.procedural.color_difference.words;}
}
bool Written(const PicaUploadState& u,unsigned type,unsigned i) {
    switch(type){case 0:return u.procedural.noise.written[i];case 2:return u.procedural.color_map.written[i];
    case 3:return u.procedural.alpha_map.written[i];case 4:return u.procedural.color.written[i];
    default:return u.procedural.color_difference.written[i];}
}
struct Fixture {
    PicaGpuRegisters regs{};PicaUploadState uploads{};PicaListPlan plan{};
    bool Run(const List& list){return StagePicaStartupList(list.bytes,regs,uploads,plan);}
    void Stop(const List& list){auto a=regs;auto b=uploads;CHECK(!Run(list));CHECK(plan.result.error);CHECK(a==regs&&b==uploads);}
};
void EveryPortMaskBank() {
    for(auto type:types)for(unsigned port=0;port<8;++port)for(unsigned mask=0;mask<16;++mask) {
        Fixture f;const auto id=0xb0U+port;f.regs[0x400+id]=0xabcdef01;
        List l;l.Packet(0xaf,0xA5B60000U|(type<<8)|127);l.Packet(id,0x98765432,mask);
        CHECK(f.Run(l));CHECK(Words(f.plan.uploads,type)[127]==0x98765432&&Written(f.plan.uploads,type,127));
        CHECK(f.plan.registers[0x4af]==(0xA5B60000U|(type<<8)|128));
        CHECK(f.plan.result.procedural_words==1&&f.plan.result.lut_words==0&&f.plan.result.irqs==0);
        for(unsigned b=0;b<4;++b)CHECK((f.plan.registers[0x400+id]>>(8*b)&255)==
            (((mask&(1U<<b))?0x98765432U:0xabcdef01U)>>(8*b)&255));
        for(auto other:types)for(unsigned i=0;i<Words(f.plan.uploads,other).size();++i)
            CHECK(Written(f.plan.uploads,other,i)==(type==other&&i==127));
        CHECK(f.plan.uploads.gs==f.uploads.gs&&f.plan.uploads.vs==f.uploads.vs);
        CHECK(f.plan.uploads.lighting==f.uploads.lighting);
    }
}
void MaskedConfigAndWrap() {
    for(auto type:types)for(unsigned start:{0U,126U,127U,128U,254U,255U}) {
        Fixture f;List l;l.Packet(0xaf,0xabcd0100U|start);l.Packet(0xaf,type<<8,2);
        for(unsigned i=0;i<520;++i)l.Packet(0xb0+(i%8),0x12340000U+i);
        CHECK(f.Run(l));CHECK(f.plan.registers[0x4af]==(0xabcd0000U|(type<<8)|((start+520)&255)));
        std::vector<std::uint32_t> want(Words(f.plan.uploads,type).size());
        for(unsigned i=0;i<520;++i)want[((start+i)&255)%want.size()]=0x12340000U+i;
        for(unsigned i=0;i<want.size();++i){CHECK(Words(f.plan.uploads,type)[i]==want[i]);CHECK(Written(f.plan.uploads,type,i));}
        CHECK(f.plan.result.procedural_words==520);
    }
    // A mask-zero config write must not change table or cursor.
    Fixture f;List l;l.Packet(0xaf,0x203);l.Packet(0xaf,0x5ff,0);l.Packet(0xb0,0);
    CHECK(f.Run(l));CHECK(f.plan.uploads.procedural.color_map.written[3]);
    CHECK(!f.plan.uploads.procedural.color_difference.written.any());CHECK(f.plan.registers[0x4af]==0x204);
}
void PacketsAndContinuation() {
    Fixture f;List l;l.Packet(0xaf,0x4fe);l.Packet(0xb0,1,0,{2,3,4});l.Packet(0xb0,5,3,{6,7,8,9,10,11,12},true);
    CHECK(f.Run(l));CHECK(f.plan.registers[0x4af]==0x40a&&f.plan.result.procedural_words==12);
    for(unsigned i=0;i<12;++i)CHECK(f.plan.uploads.procedural.color.words[(254+i)&255]==i+1);
    List a;a.Packet(0xaf,0x37e);a.Packet(0xb0,0x89abcdef);
    CHECK(f.Run(a));const auto before=f.plan;
    f.regs=f.plan.registers;f.uploads=f.plan.uploads;
    List b;b.Packet(0xb7,0x76543210);CHECK(f.Run(b));const auto split=f.plan;
    Fixture combined;a.Packet(0xb7,0x76543210);CHECK(combined.Run(a));
    CHECK(split.uploads==combined.plan.uploads&&split.registers==combined.plan.registers);
    CHECK(before.uploads.procedural.alpha_map.written.count()==1);
    // Unknown entries retain explicit unknown flags, even when storage is zero.
    CHECK(!split.uploads.procedural.alpha_map.written[0]&&split.uploads.procedural.alpha_map.words[0]==0);
}
void FailurePreservesInitialAndGuards() {
    for(unsigned type=0;type<16;++type) {
        if(type==0||type==2||type==3||type==4||type==5)continue;
        Fixture f;List l;l.Packet(0xaf,0x200);l.Packet(0xb0,0x12345678);
        l.Packet(0xaf,type<<8);l.Packet(0xb7,0xffffffff);f.Stop(l);
        CHECK(f.plan.result.register_id==0xb7&&f.uploads.procedural.color_map.written.none());
    }
    for(unsigned guard:{0x22eU,0x22fU,0x23cU,0x23dU,0x232U,0x233U,0x234U,0x235U,0xe8U,0xefU}) {
        Fixture f;List l;l.Packet(0xaf,0x400);l.Packet(0xb0,0xdecafbad);l.Packet(guard,0,0);f.Stop(l);
        CHECK(f.plan.result.register_id==guard&&f.uploads.procedural.color.written.none());
    }
    Fixture f;List l;l.Packet(0xaf,0x500);l.Packet(0xb0,0x12345678);l.Word(0);l.Word(0x0ffF00b0);f.Stop(l);
    // Explicit matching IRQ stops before a later invalid table upload; not a draw.
    Fixture stop;stop.regs[0x420]=0x12345678;stop.regs[0x434]=1;
    List end;end.Packet(0xaf,0x500);end.Packet(0xb0,0xff00ff00);end.Packet(0x10,0x12345678);
    end.Packet(0xaf,0x100);end.Packet(0xb0,0);
    CHECK(stop.Run(end));CHECK(stop.plan.result.autostopped&&stop.plan.result.irqs==1);
    CHECK(stop.plan.result.procedural_words==1&&stop.plan.uploads.procedural.color_difference.words[0]==0xff00ff00);
}
}
int main(){EveryPortMaskBank();MaskedConfigAndWrap();PacketsAndContinuation();FailurePreservesInitialAndGuards();
if(failures)return EXIT_FAILURE;std::cout<<"PASS: procedural LUT raw data, selectors, masked config, wrapping, known bytes, transactions and retained draw/fog guards\n";return EXIT_SUCCESS;}
