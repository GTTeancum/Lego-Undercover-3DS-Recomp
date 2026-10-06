#pragma once
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include "services/dsp_special_config.h"

namespace lego::host {
// Read an explicitly selected host file, not a guest-controlled path. The block
// has no authenticity claim: preserve its hash/provenance. All I/O/length failures
// reject launch; none authorizes the DSP module's missing-CFG fallback.
inline ctr::DspSpecialConfig ReadDspSpecialBlock(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if (!file || file.tellg()!=std::streamoff(ctr::kDspSpecialConfigBytes))
        throw std::runtime_error("DSP special block file is missing or not exactly 532 bytes");
    std::array<std::uint8_t,ctr::kDspSpecialConfigBytes> data{};
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(data.data()),data.size()) ||
        file.peek()!=std::char_traits<char>::eof() || file.bad())
        throw std::runtime_error("cannot read an exact complete DSP special block");
    return *ctr::DspSpecialConfig::FromBlock(data);
}
} // namespace lego::host
