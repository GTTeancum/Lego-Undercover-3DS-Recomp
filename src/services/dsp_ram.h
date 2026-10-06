#pragma once
#include "runtime/ctr_device_memory.h"
#include <algorithm>
#include <limits>
#include <vector>
#include <cstring>

namespace lego::ctr {
// The ARM view is the DATA half of the same SRAM consumed by Teakra.
// Program storage stays private to the DSP executor. No ownership cycles.
class DspRam final : public DeviceMemory {
public:
    static constexpr std::uint32_t DataOffset=0x40000, TotalBytes=0x80000;
    std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(TotalBytes,0);
    std::vector<std::uint8_t> provenance=std::vector<std::uint8_t>(TotalBytes,0);
    std::vector<std::uint64_t> epochs=std::vector<std::uint64_t>(TotalBytes/8,0);
    std::uint64_t next_epoch{1};
    std::uint32_t known_bytes{};
    bool sealed{};
    std::uint32_t size() const noexcept override{return DataOffset;}
    bool CanRead(std::uint32_t offset,std::uint32_t n) const noexcept override {
        if(sealed || !n || std::uint64_t(offset)+n>DataOffset)return false;
        return std::all_of(provenance.begin()+DataOffset+offset,
                           provenance.begin()+DataOffset+offset+n,[](auto b){return b!=0;});
    }
    bool CanWrite(std::uint32_t offset,std::uint32_t n) const noexcept override {
        return !sealed && n && std::uint64_t(offset)+n<=DataOffset &&
               next_epoch!=std::numeric_limits<std::uint64_t>::max();
    }
    bool Read(std::uint32_t offset,std::span<std::uint8_t> out) const noexcept override {
        if(out.size()>DataOffset || !CanRead(offset,static_cast<std::uint32_t>(out.size())))return false;
        std::memmove(out.data(),bytes.data()+DataOffset+offset,out.size());return true;
    }
    bool Write(std::uint32_t offset,std::span<const std::uint8_t> input) noexcept override {
        if(input.size()>DataOffset || !CanWrite(offset,static_cast<std::uint32_t>(input.size())))return false;
        // Callers may supply an overlapping span of this bank.
        std::memmove(bytes.data()+DataOffset+offset,input.data(),input.size());
        Mark(DataOffset+offset,static_cast<std::uint32_t>(input.size()),4); // Host/ARM write, not firmware.
        return true;
    }
    // Internal preflighted raw store observation, immediately before Teakra's no-throw store.
    bool Mark(std::uint32_t address,std::uint32_t n,std::uint8_t source) noexcept {
        if(sealed || !n || std::uint64_t(address)+n>TotalBytes ||
           next_epoch==std::numeric_limits<std::uint64_t>::max())return false;
        for(std::uint32_t i=address;i<address+n;++i){if(!provenance[i])++known_bytes;provenance[i]=source;}
        const auto epoch=next_epoch++;
        for(auto i=address/8;i<=(address+n-1)/8;++i)epochs[i]=epoch;
        return true;
    }
    std::uint64_t Epoch(std::uint32_t offset) const noexcept override {
        return offset<DataOffset?epochs[(DataOffset+offset)/8]:0;
    }
};
} // namespace lego::ctr
