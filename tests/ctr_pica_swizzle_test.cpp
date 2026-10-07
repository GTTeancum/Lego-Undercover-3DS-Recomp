#include "services/pica_startup.h"
#include <cstdlib>
#include <iostream>
#include <vector>
namespace {
using namespace lego::ctr;
int failures{};
#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}}while(0)
struct List {
    std::vector<std::uint8_t> bytes;
    void Word(std::uint32_t v){for(unsigned i=0;i<4;++i)bytes.push_back(v>>(8*i));}
    void Packet(std::uint32_t id,std::uint32_t value,unsigned mask=15,
                std::initializer_list<std::uint32_t> extra={},bool group=false) {
        Word(value);Word(id|(mask<<16)|(std::uint32_t(extra.size())<<20)|(group?0x80000000U:0));
        for(auto v:extra)Word(v);if(extra.size()&1)Word(0xdeadbeef);
    }
};
struct Fixture {
    PicaGpuRegisters regs{};PicaUploadState uploads{};PicaListPlan plan{};
    bool Run(const List& l){return StagePicaStartupList(l.bytes,regs,uploads,plan);}
    void Stop(const List& l){auto a=regs;auto b=uploads;CHECK(!Run(l)&&plan.result.error);CHECK(regs==a&&uploads==b);}
};
void PortsMasksMirroring() {
    for(bool vs:{false,true})for(unsigned port=0;port<8;++port)for(unsigned mask=0;mask<16;++mask) {
        Fixture f;const unsigned offset=vs?0x2d5:0x2a5,reg=(vs?0x2d6:0x2a6)+port;
        f.regs[0x400+reg]=0xabcdef01;
        List l;l.Packet(offset,17);l.Packet(reg,0x12345678,mask);
        CHECK(f.Run(l));const auto& s=vs?f.plan.uploads.vs:f.plan.uploads.gs;
        CHECK(s.swizzle[17]==0x12345678 && s.swizzle_written.count()==1);
        CHECK(s.program_written.none() && s.floats_written.none());
        CHECK(f.plan.registers[0x400+offset]==18 && f.plan.result.swizzle_words==1 && f.plan.result.irqs==0);
        for(unsigned b=0;b<4;++b)CHECK(((f.plan.registers[0x400+reg]>>(8*b))&255)==(((mask&(1U<<b))?0x12345678U:0xabcdef01U)>>(8*b)&255));
        if(vs)CHECK(f.plan.uploads.gs.swizzle[17]==0x12345678 && f.plan.uploads.gs.swizzle_written[17]);
        else CHECK(f.plan.uploads.vs.swizzle_written.none());
    }
    for(auto id:{0x244U,0x229U}) {
        Fixture f;List l;l.Packet(id,1);l.Packet(0x2d5,7);l.Packet(0x2d6,0x55667788);
        CHECK(f.Run(l));CHECK(f.plan.uploads.vs.swizzle_written[7]);CHECK(f.plan.uploads.gs.swizzle_written.none());
    }
    // Turning mirroring off retains the independent GS table and GS offset.
    Fixture f;f.regs[0x6a5]=77;List l;l.Packet(0x2d6,0x11);l.Packet(0x244,1);l.Packet(0x2d6,0x22);
    CHECK(f.Run(l));CHECK(f.plan.uploads.gs.swizzle[0]==0x11 && !f.plan.uploads.gs.swizzle_written[1]);
    CHECK(f.plan.uploads.vs.swizzle[1]==0x22 && f.plan.registers[0x6a5]==77);
}
void PacketsAndBounds() {
    Fixture f;List l;l.Packet(0x2d5,11);l.Packet(0x2d6,1,0,{2,3,4});l.Packet(0x2d6,5,0,{6,7},true);
    CHECK(f.Run(l));CHECK(f.plan.result.swizzle_words==7 && f.plan.registers[0x6d5]==18);
    for(unsigned i=0;i<7;++i)CHECK(f.plan.uploads.vs.swizzle[11+i]==i+1);
    // Partial masked offset writes are merged; upload ports still consume raw values.
    List masked;masked.Packet(0x2d5,0x12345678);masked.Packet(0x2d5,0x00000f00,14);masked.Packet(0x2d6,0x89abcdef);
    CHECK(f.Run(masked));CHECK(f.plan.uploads.vs.swizzle[0xf78]==0x89abcdef);
    for(bool vs:{false,true}) {
        const unsigned offset=vs?0x2d5:0x2a5,port=vs?0x2d6:0x2a6;
        List last;last.Packet(offset,4095);last.Packet(port,0xff00ff00);
        CHECK(f.Run(last));CHECK(f.plan.registers[0x400+offset]==4096);
        last.Packet(port,1);f.Stop(last);
        for(auto v:{4096U,0x10000U,0xffffffffU}){List bad;bad.Packet(offset,v);bad.Packet(port,0);f.Stop(bad);}
    }
    // Split successful lists persist the table/offset just like one full list.
    List a;a.Packet(0x2d5,4094);a.Packet(0x2d6,0x11111111);
    CHECK(f.Run(a));f.regs=f.plan.registers;f.uploads=f.plan.uploads;
    List b;b.Packet(0x2d6,0x22222222);CHECK(f.Run(b));
    CHECK(f.plan.uploads.vs.swizzle[4094]==0x11111111 && f.plan.uploads.vs.swizzle[4095]==0x22222222);
    // A later unsupported draw leaves the original complete state unchanged.
    List rollback;rollback.Packet(0x2d5,1);rollback.Packet(0x2d6,0xaabbccdd);rollback.Packet(0x22e,1);f.Stop(rollback);
    CHECK(f.uploads.vs.swizzle[1]==0 && !f.uploads.vs.swizzle_written[1]);
}
void CompleteTable() {
    Fixture f;List l;l.Packet(0x2d5,0);
    for(unsigned i=0;i<4096;++i)l.Packet(0x2d6+(i%8),i*0x01010101U);
    CHECK(f.Run(l));CHECK(f.plan.result.swizzle_words==4096);
    CHECK(f.plan.uploads.vs.swizzle_written.all() && f.plan.uploads.gs.swizzle_written.all());
    for(unsigned i=0;i<4096;++i)CHECK(f.plan.uploads.vs.swizzle[i]==i*0x01010101U && f.plan.uploads.gs.swizzle[i]==i*0x01010101U);
    CHECK(f.plan.uploads.vs.program_written.none() && f.plan.result.program_words==0);
}
}
int main(){PortsMasksMirroring();PacketsAndBounds();CompleteTable();if(failures)return EXIT_FAILURE;
std::cout<<"PASS: raw swizzle uploads, masked mirrors, capacity, GS routing, persistence and whole-list rollback\n";return EXIT_SUCCESS;}
