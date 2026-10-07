#include "services/gsp_gpu_service.h"
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace lego::ctr;
namespace {
int failures=0;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}}while(0)
struct List {
    std::vector<std::uint8_t> data;
    void Word(std::uint32_t v) {for(unsigned i=0;i<4;++i)data.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
    void Packet(std::uint32_t id,std::uint32_t value,std::uint32_t mask=15,
                std::initializer_list<std::uint32_t> extra={},bool sequential=false) {
        Word(value);Word(id|(mask<<16)|(static_cast<std::uint32_t>(extra.size())<<20)|(sequential?0x80000000U:0));
        for(auto v:extra)Word(v);if(extra.size()&1)Word(0xDEADC0DE); // Arbitrary padding is not an opcode.
    }
};
struct Fixture {
    PicaGpuRegisters registers{};PicaUploadState state{};PicaListPlan plan;
    Fixture() {
        GspGpuService gsp;
        for(unsigned i=0;i<registers.size();++i)registers[i]=*gsp.register_word(0x400000+4*i);
    }
    bool Run(const List& list) {return StagePicaStartupList(list.data,registers,state,plan);}
    void Stop(const List& list) {const auto before=registers;const auto upload=state;CHECK(!Run(list));CHECK(plan.result.error);CHECK(registers==before&&state==upload);}
};
void MasksAndSequences() {
    Fixture f;List l;
    l.Packet(0x80,0x12345678);l.Packet(0x80,0xFFEEDDCC,5);
    l.Packet(0x81,7,15,{8,9,10},true);l.Packet(0x85,11,15,{12,13});
    CHECK(f.Run(l));CHECK(f.plan.registers[0x480]==0x12EE56CC);
    for(unsigned i=0;i<4;++i)CHECK(f.plan.registers[0x481+i]==7+i);
    CHECK(f.plan.registers[0x485]==13);CHECK(f.plan.result.packets==4&&f.plan.result.writes==9);
    CHECK(f.plan.uploads.registers_written.count()==6);
    CHECK(f.plan.result.irqs==0&&!f.plan.result.autostopped);
    // All 16 byte masks preserve unselected bytes.
    for(unsigned mask=0;mask<16;++mask){List x;x.Packet(0x40,0xA1B2C3D4);x.Packet(0x40,0x12345678,mask);
        CHECK(f.Run(x));for(unsigned b=0;b<4;++b)CHECK(((f.plan.registers[0x440]>>(8*b))&255)==(((mask&(1U<<b))?0x12345678U:0xA1B2C3D4U)>>(8*b)&255));}
}
void ProgramsAndLuts() {
    Fixture f;List l;
    l.Packet(0x2CB,0);l.Packet(0x2CC,0x12345678,0,{0xAABBCCDD,0x100}); // Ports receive raw values even mask 0.
    l.Packet(0x29B,512);l.Packet(0x29C,0x99999999,15,{0x87654321});
    l.Packet(0x1C5,0x000012FF);l.Packet(0x1C8,0x01020304,1,{0x55667788},true);
    CHECK(f.Run(l));CHECK(f.plan.result.program_words==5&&f.plan.result.lut_words==2);
    CHECK(f.plan.uploads.vs.program[0]==0x12345678&&f.plan.uploads.vs.program[1]==0xAABBCCDD);
    CHECK(f.plan.uploads.gs.program[0]==0x12345678&&f.plan.uploads.gs.program[512]==0x99999999);
    CHECK(f.plan.registers[0x6CB]==3&&f.plan.registers[0x69B]==514);
    CHECK(f.plan.registers[0x6CC]==0);CHECK(f.plan.uploads.gs.program_written.count()==5);
    CHECK(f.plan.uploads.lighting[18][255]==0x01020304&&f.plan.uploads.lighting[18][0]==0x55667788);
    CHECK(f.plan.registers[0x5C5]==0x1201);
    List isolated;isolated.Packet(0x244,1);isolated.Packet(0x2CB,0);isolated.Packet(0x2CC,8);
    CHECK(f.Run(isolated));CHECK(f.plan.uploads.vs.program[0]==8&&!f.plan.uploads.gs.program_written[0]);
    List gs_mode;gs_mode.Packet(0x229,2);gs_mode.Packet(0x2CB,0);gs_mode.Packet(0x2CC,9);
    CHECK(f.Run(gs_mode));CHECK(!f.plan.uploads.gs.program_written[0]);
    for(auto id:{0x29CU,0x2CCU}){List end;end.Packet(id==0x29C?0x29B:0x2CB,id==0x29C?4095:511);end.Packet(id,0x5555);CHECK(f.Run(end));end.Packet(id,1);f.Stop(end);}
    List bad;bad.Packet(0x1C5,24<<8);bad.Packet(0x1C8,7);f.Stop(bad);
}
void UniformTransfers() {
    Fixture f;List l;l.Packet(0x290,0x80000000);l.Packet(0x291,0x3F800000,0,{0x40000000,0x40400000,0x40800000},true);
    l.Packet(0x2B1,0x12345678);l.Packet(0x2B0,0xABCD);
    CHECK(f.Run(l));CHECK(f.plan.result.uniform_vectors==1);
    CHECK((f.plan.uploads.gs.floats[0]==std::array<std::uint32_t,4>{0x40800000,0x40400000,0x40000000,0x3F800000}));
    CHECK(f.plan.uploads.gs.floats_written[0]&&f.plan.registers[0x690]==0x80000001);
    CHECK(f.plan.uploads.vs.integers[0]==0x12345678&&f.plan.uploads.gs.integers[0]==0x12345678);
    CHECK(f.plan.uploads.gs.booleans==0xABCD&&f.plan.uploads.vs.booleans==0xABCD);
    // Packed float24 golden vectors: x=1, y=-0, z=+infinity, w=minimum denormal.
    List raw;raw.Packet(0x2C0,0);raw.Packet(0x2C1,0x0000017F,15,{0x00008000,0x003F0000});
    CHECK(f.Run(raw));CHECK((f.plan.uploads.vs.floats[0]==std::array<std::uint32_t,4>{0x3F800000,0x80000000,0x7F800000,0x18800000}));
    CHECK(f.plan.uploads.gs.floats[0]==f.plan.uploads.vs.floats[0]);
    // A partial vector survives between successfully committed command lists.
    List first;first.Packet(0x290,0x80000005);first.Packet(0x291,0x3F800000);
    CHECK(f.Run(first));CHECK(f.plan.uploads.gs.packed_count==1&&!f.plan.uploads.gs.floats_written[5]);
    f.registers=f.plan.registers;f.state=f.plan.uploads;List next;next.Packet(0x291,2,15,{3,4});
    CHECK(f.Run(next));CHECK(f.plan.uploads.gs.floats[5][3]==0x3F800000&&f.plan.uploads.gs.packed_count==0);
    List bad;bad.Packet(0x290,0x80000060);bad.Packet(0x291,0,15,{0,0,0});f.Stop(bad);
}
void InterruptAndGuards() {
    Fixture f;List irq;irq.Packet(0x80,7);irq.Packet(0x10,0x12345678);
    irq.Packet(0x22E,1); // Unreachable after the genuine matching autostop.
    CHECK(f.Run(irq));CHECK(f.plan.result.irqs==1&&f.plan.result.autostopped&&f.plan.result.writes==2);
    List no;no.Packet(0x10,0xABCDEF00);no.Packet(0x80,1);CHECK(f.Run(no));CHECK(f.plan.result.irqs==0&&f.plan.result.writes==2);
    List one_byte;one_byte.Packet(0x10,0xABCD0078);CHECK(f.Run(one_byte));CHECK(f.plan.result.irqs==1);
    List multiple;multiple.Packet(0x34,0);multiple.Packet(0x10,0x12345678);multiple.Packet(0x10,0x12345678);CHECK(f.Run(multiple));CHECK(f.plan.result.irqs==2&&!f.plan.result.autostopped);
    for(auto id:{0x22EU,0x22FU,0x23CU,0x23DU,0x232U,0x233U,0xE8U,0x300U,0xFFFFU}){
        List bad;bad.Packet(0x80,0x1234);bad.Packet(id,0,0);f.Stop(bad);
    }
    List repeat;repeat.Packet(0x10,0x12345678,15,{0});f.Stop(repeat);
    List truncated;truncated.Word(0);truncated.Word(0x0FFF0080);f.Stop(truncated);
    List unaligned;unaligned.Word(0);f.Stop(unaligned);
    List too_large;too_large.data.resize(kPicaMaxListBytes+8);f.Stop(too_large);
    List empty;CHECK(f.Run(empty));CHECK(f.plan.result.writes==0);
    List reserved;reserved.Word(0x5555);reserved.Word(0x700F0080);CHECK(f.Run(reserved));CHECK(f.plan.registers[0x480]==0x5555);
}
} // namespace
int main(){MasksAndSequences();ProgramsAndLuts();UniformTransfers();InterruptAndGuards();
 if(failures)return EXIT_FAILURE;std::cout<<"PASS: staged PICA masks/packets/programs/uniforms/LUTs and fail-closed unsupported effects\n";return EXIT_SUCCESS;}
