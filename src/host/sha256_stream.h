#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>

namespace lego::host {
// Bounded-memory SHA-256 for large private game inputs. Final() is repeatable.
class Sha256Stream {
public:
    void Update(std::span<const std::uint8_t> bytes) {
        if (bytes.size() > std::numeric_limits<std::uint64_t>::max()/8 - count_)
            throw std::length_error("SHA-256 input too long");
        count_ += bytes.size();
        if (used_) {
            const auto n=std::min(bytes.size(),buffer_.size()-used_);
            std::copy_n(bytes.begin(),n,buffer_.begin()+used_);
            used_+=n; bytes=bytes.subspan(n);
            if (used_==64) { Compress(buffer_.data()); used_=0; }
        }
        while (bytes.size()>=64) { Compress(bytes.data()); bytes=bytes.subspan(64); }
        if (!bytes.empty()) {
            std::copy(bytes.begin(),bytes.end(),buffer_.begin()); used_=bytes.size();
        }
    }
    [[nodiscard]] std::string Final() const {
        auto state=*this;
        state.buffer_[state.used_++]=0x80;
        if (state.used_>56) {
            std::fill(state.buffer_.begin()+state.used_,state.buffer_.end(),0);
            state.Compress(state.buffer_.data()); state.used_=0;
        }
        std::fill(state.buffer_.begin()+state.used_,state.buffer_.begin()+56,0);
        const auto bits=count_*8;
        for (unsigned i=0;i<8;++i) state.buffer_[63-i]=static_cast<std::uint8_t>(bits>>(i*8));
        state.Compress(state.buffer_.data());
        constexpr char hex[]="0123456789abcdef";
        std::string result; result.reserve(64);
        for (auto word:state.h_)
            for (int shift=28;shift>=0;shift-=4) result+=hex[(word>>shift)&15];
        return result;
    }
private:
    void Compress(const std::uint8_t* data) {
        constexpr std::array<std::uint32_t,64> k{
            0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
            0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
            0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
            0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
            0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
            0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
            0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
            0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
        std::array<std::uint32_t,64> w{};
        for (unsigned i=0;i<16;++i)
            w[i]=std::uint32_t(data[i*4])<<24 | std::uint32_t(data[i*4+1])<<16 |
                 std::uint32_t(data[i*4+2])<<8 | data[i*4+3];
        for (unsigned i=16;i<64;++i) {
            const auto s0=std::rotr(w[i-15],7)^std::rotr(w[i-15],18)^(w[i-15]>>3);
            const auto s1=std::rotr(w[i-2],17)^std::rotr(w[i-2],19)^(w[i-2]>>10);
            w[i]=w[i-16]+s0+w[i-7]+s1;
        }
        auto [a,b,c,d,e,f,g,h]=h_;
        for (unsigned i=0;i<64;++i) {
            const auto s1=std::rotr(e,6)^std::rotr(e,11)^std::rotr(e,25);
            const auto t1=h+s1+((e&f)^(~e&g))+k[i]+w[i];
            const auto s0=std::rotr(a,2)^std::rotr(a,13)^std::rotr(a,22);
            const auto t2=s0+((a&b)^(a&c)^(b&c));
            h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
        }
        const std::array<std::uint32_t,8> v{a,b,c,d,e,f,g,h};
        for (unsigned i=0;i<8;++i) h_[i]+=v[i];
    }
    std::array<std::uint32_t,8> h_{
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    std::array<std::uint8_t,64> buffer_{};
    std::size_t used_{};
    std::uint64_t count_{};
};
} // namespace lego::host
