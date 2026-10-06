#include "runtime/ctr_memory.h"
#include <cstring>
#include <limits>

namespace lego::ctr {
bool GuestMemory::PrepareDeviceWrite(std::uint32_t address, std::uint32_t size) {
    if (!size) return true;
    const auto* region=FindRegion(address,size);
    // GPU physical linear-heap writes cannot target service-shared pages. General
    // GPU physical aliasing is not reconstructed by granting arbitrary VA access.
    if (!region || region->shared || !IsWritable(address,size))
        return false;
    const auto canonical=region->EpochAddress(address);
    const auto last=(canonical+size-1U)&~7U;
    for (auto granule=canonical&~7U;;granule+=8U) {
        exclusive_epochs_.try_emplace(granule,0U); // Preserve every existing token.
        if (granule==last) break;
    }
    return true;
}

bool GuestMemory::CommitDeviceWrite(std::uint32_t address,
                                    std::span<const std::uint8_t> data) noexcept {
    if (data.empty()) return true;
    if (data.size()>std::numeric_limits<std::uint32_t>::max()) return false;
    const auto size=static_cast<std::uint32_t>(data.size());
    auto* region=FindRegion(address,size);
    if (!region || region->shared || !IsWritable(address,size))
        return false;
    const auto canonical=region->EpochAddress(address);
    const auto first=canonical&~7U, last=(canonical+size-1U)&~7U;
    for (auto granule=first;;granule+=8U) {
        if (!exclusive_epochs_.contains(granule)) return false;
        if (granule==last) break;
    }
    std::memmove(region->MutableData().data()+(address-region->base),data.data(),size);
    for (auto granule=first;;granule+=8U) {
        exclusive_epochs_.find(granule)->second=next_epoch_++;
        if (granule==last) break;
    }
    return true;
}

} // namespace lego::ctr
