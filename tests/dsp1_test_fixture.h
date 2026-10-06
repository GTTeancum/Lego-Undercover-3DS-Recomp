#pragma once
#include "host/sha256.h"
#include <vector>
#include <array>
#include <cstdint>
#include <span>
namespace dsp_fixture {
inline void Put32(std::vector<std::uint8_t>& b,std::size_t at,std::uint32_t v) {
    for(unsigned i=0;i<4;++i)b.at(at+i)=static_cast<std::uint8_t>(v>>(8*i));
}
inline unsigned Nibble(char c){return c<='9'?c-'0':c-'a'+10;}
inline void Rehash(std::vector<std::uint8_t>& b,unsigned segment) {
    const auto at=0x120+segment*48;
    auto u=[&](unsigned p){std::uint32_t n=0;for(unsigned i=0;i<4;++i)n|=std::uint32_t(b.at(p+i))<<(8*i);return n;};
    const auto hash=lego::host::Sha256(std::span(b).subspan(u(at),u(at+8)));
    for(unsigned i=0;i<32;++i)b.at(at+16+i)=static_cast<std::uint8_t>((Nibble(hash[i*2])<<4)|Nibble(hash[i*2+1]));
}
inline std::vector<std::uint8_t> Image(bool special=false) {
    std::vector<std::uint8_t> b(0x320,0);
    b[0x100]='D';b[0x101]='S';b[0x102]='P';b[0x103]='1';Put32(b,0x104,b.size());
    b[0x108]=b[0x109]=1;b[0x10E]=3;b[0x10F]=special?3:1;
    const std::array<unsigned,3> offsets{0x300,0x308,0x310},words{0x10,0x110,0x500},sizes{8,8,16};
    for(unsigned i=0;i<32;++i)b[0x300+i]=static_cast<std::uint8_t>(i*7+3);
    for(unsigned i=0;i<3;++i){const auto at=0x120+i*48;Put32(b,at,offsets[i]);Put32(b,at+4,words[i]);Put32(b,at+8,sizes[i]);b[at+15]=i;Rehash(b,i);}
    if(special){b[0x10D]=2;Put32(b,0x110,0x2000);Put32(b,0x114,0x214);}
    return b;
}
}
