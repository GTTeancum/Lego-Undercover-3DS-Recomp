#include "host/sha256_stream.h"
#include "host/sha256.h"
#include <cstdlib>
#include <iostream>
#include <vector>
using lego::host::Sha256Stream;
int main() {
    auto require=[](bool ok){if(!ok){std::cerr<<"SHA-256 streaming regression failed\n";std::exit(1);}};
    Sha256Stream empty;
    require(empty.Final()=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    const std::array<std::uint8_t,3> abc{'a','b','c'};
    Sha256Stream a;for(auto byte:abc)a.Update({&byte,1});
    require(a.Final()=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    require(a.Final()==a.Final());
    for(auto length:{0U,1U,55U,56U,63U,64U,65U,127U,128U,129U,4096U,10001U}) {
        std::vector<std::uint8_t> bytes(length);
        for(unsigned i=0;i<length;++i)bytes[i]=static_cast<std::uint8_t>(i*37U+11U);
        for(auto stride:{1U,7U,64U,65U,4096U}) {
            Sha256Stream hash;
            for(unsigned p=0;p<length;p+=stride)
                hash.Update(std::span(bytes).subspan(p,std::min(stride,length-p)));
            hash.Update({});require(hash.Final()==lego::host::Sha256(bytes));
        }
    }
    Sha256Stream million;
    const std::vector<std::uint8_t> block(1000,'a');
    for(unsigned i=0;i<1000;++i)million.Update(block);
    require(million.Final()=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    std::cout<<"PASS: SHA-256 streaming vectors and chunk/padding boundaries\n";
}
