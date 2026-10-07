#include "recomp/a32_vfp_scalar.h"
#include <bit>
#include <iostream>
#include <stdexcept>
using namespace oot3d::recomp::a32;
namespace {
#define CHECK(x) do { if (!(x)) throw std::runtime_error("line " + std::to_string(__LINE__) + ": " #x); } while (0)
constexpr std::uint32_t PC=0x1000, MODE=0x00370000;
std::uint32_t F(float x) { return std::bit_cast<std::uint32_t>(x); }
std::uint32_t Encode(std::uint32_t op, unsigned d, unsigned n, unsigned m, bool three=true) {
    op |= (d&1U)<<22U | (d>>1U)<<12U | (m&1U)<<5U | (m>>1U);
    if (three) op |= (n>>1U)<<16U | (n&1U)<<7U;
    return op;
}
GuestState Initial(unsigned length, unsigned stride=0) {
    GuestState s{};s.r[15]=PC;s.cpsr=0x20000010;
    s.fpscr=0x23000000U | ((length-1)<<16U) | (stride<<20U);
    for (unsigned i=0;i<s.vfp.size();++i) s.vfp[i]=F(float(i+1));
    return s;
}
void Run(std::uint32_t op, GuestState& s) {
    const auto old_mode=s.fpscr&MODE, cpsr=s.cpsr;
    CHECK(ExecuteVfpScalar(op,PC,s).kind==ExitKind::Fallthrough);
    CHECK(s.r[15]==PC+4 && (s.fpscr&MODE)==old_mode && s.cpsr==cpsr);
}
void Reject(std::uint32_t op, GuestState s) {
    auto old=s;
    CHECK(ExecuteVfpScalar(op,PC,s).kind==ExitKind::Unsupported);
    CHECK(s.vfp==old.vfp && s.fpscr==old.fpscr && s.r==old.r && s.cpsr==old.cpsr);
}
void ExamplesAndScalarBanks() {
    // DDI 0274H bank-wrap example: six FADD iterations, explicit oracle lanes.
    auto s=Initial(6);const auto before=s.vfp;
    Run(Encode(0xEE300A00,11,22,31),s);
    const unsigned dst[]={11,12,13,14,15,8}, left[]={22,23,16,17,18,19}, right[]={31,24,25,26,27,28};
    auto expected=before;
    for (unsigned i=0;i<6;++i) expected[dst[i]]=F(float(left[i]+right[i]+2));
    CHECK(s.vfp==expected);
    // Destination S0..S7 remains scalar even with a nonzero LEN.
    for(unsigned d=0;d<8;++d) {
        auto x=Initial(3);auto old=x.vfp;Run(Encode(0xEE300A40,d,d,d),x);old[d]=F(0);CHECK(x.vfp==old);
    }
    // Fn in bank zero still advances, Fm in bank zero is a broadcast.
    s=Initial(4);Run(Encode(0xEE000A00,16,0,8),s);
    for(unsigned i=0;i<4;++i) CHECK(s.vfp[16+i]==F(float(17+i)+float(1+i)*float(9+i)));
    s=Initial(3);Run(Encode(0xEE200A00,8,16,1),s);
    CHECK(s.vfp[8]==F(34)&&s.vfp[9]==F(36)&&s.vfp[10]==F(38));
    s=Initial(3);Run(Encode(0xEEB10A40,8,0,0,false),s);
    CHECK(s.vfp[8]==F(-1)&&s.vfp[9]==F(-1)&&s.vfp[10]==F(-1));
}
void AllBanksLengthsAndStrides() {
    for(unsigned stride:{0U,3U}) for(unsigned length=2;length<=8;++length) {
        const unsigned step=stride==0?1:2;
        if(length*step>8) continue;
        for(unsigned d=8;d<32;++d) for(unsigned n=0;n<32;++n) for(unsigned m=0;m<32;++m) {
            auto s=Initial(length,stride);const auto old=s;
            bool hazard=false;
            for(unsigned i=0;i<length;++i) for(unsigned j=0;j<length;++j) if(i!=j) {
                const unsigned dest=(d&~7U)|((d+i*step)&7U);
                hazard |= dest==((n&~7U)|((n+j*step)&7U));
                hazard |= m>=8 && dest==((m&~7U)|((m+j*step)&7U));
            }
            const auto op=Encode(0xEE300A00,d,n,m);
            if(hazard){Reject(op,s);continue;}
            auto expected=s.vfp;
            for(unsigned i=0;i<length;++i){
                const unsigned di=(d&~7U)|((d+i*step)&7U), ni=(n&~7U)|((n+i*step)&7U);
                const unsigned mi=m<8?m:((m&~7U)|((m+i*step)&7U));
                expected[di]=F(float(ni+mi+2));
            }
            Run(op,s);CHECK(s.vfp==expected);
        }
    }
}
void OperationsFlagsAndGuards() {
    // Compare every admitted arithmetic/unary opcode against explicit scalar
    // lane calls. This tests lane/control wiring, not a second arithmetic oracle.
    for(auto op:{0xEE000A00U,0xEE000A40U,0xEE100A00U,0xEE100A40U,0xEE200A00U,
                0xEE200A40U,0xEE300A00U,0xEE300A40U,0xEE800A00U}) {
        auto v=Initial(3);auto scalar=v;scalar.fpscr&=~MODE;
        for(unsigned i=0;i<3;++i) CHECK(ExecuteVfpScalar(Encode(op,8+i,16+i,24+i),PC,scalar).kind==ExitKind::Fallthrough);
        Run(Encode(op,8,16,24),v);CHECK(v.vfp==scalar.vfp && (v.fpscr&~MODE)==scalar.fpscr);
    }
    for(auto op:{0xEEB00AC0U,0xEEB10A40U,0xEEB10AC0U}) {
        auto v=Initial(3);auto scalar=v;scalar.fpscr&=~MODE;
        for(unsigned i=0;i<3;++i) CHECK(ExecuteVfpScalar(Encode(op,8+i,0,16+i,false),PC,scalar).kind==ExitKind::Fallthrough);
        Run(Encode(op,8,0,16,false),v);CHECK(v.vfp==scalar.vfp && (v.fpscr&~MODE)==scalar.fpscr);
    }
    for(auto op:{0xEEB80AC0U,0xEEB80A40U,0xEEBD0AC0U,0xEEBC0AC0U,0xEEB40A40U,0xEEB40AC0U}) {
        auto v=Initial(3);auto scalar=v;scalar.fpscr&=~MODE;const auto raw=Encode(op,8,0,16,false);
        CHECK(ExecuteVfpScalar(raw,PC,scalar).kind==ExitKind::Fallthrough);Run(raw,v);
        CHECK(v.vfp==scalar.vfp && (v.fpscr&~MODE)==scalar.fpscr);
    }
    auto flags=Initial(3);flags.vfp[16]=F(0);flags.vfp[17]=F(1);flags.vfp[18]=F(2);
    flags.vfp[24]=F(0);flags.vfp[25]=F(0);flags.vfp[26]=F(1);
    Run(Encode(0xEE800A00,8,16,24),flags);
    CHECK(flags.vfp[8]==0x7FC00000 && flags.vfp[9]==0x7F800000 && flags.vfp[10]==F(2));
    CHECK((flags.fpscr&3)==3); // Invalid operation and divide-by-zero accumulate.
    const auto add=Encode(0xEE300A00,8,16,24);
    Reject(add,Initial(3,1));Reject(add,Initial(3,2));Reject(add,Initial(1,3));
    Reject(add,Initial(5,3));auto trap=Initial(3);trap.fpscr|=0x100;Reject(add,trap);
    Reject(0xFE300A00,Initial(3));Reject(0xEE300B00,Initial(3));Reject(0xEE900A00,Initial(3));
    Reject(Encode(0xEE300A00,8,9,16),Initial(3));
}
}
int main(){try{ExamplesAndScalarBanks();AllBanksLengthsAndStrides();OperationsFlagsAndGuards();}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
std::cout<<"PASS: bounded VFPv2 single-precision short vectors; bank wrap, scalar operands, strides, sticky flags and no partial unsupported writes\n";}
