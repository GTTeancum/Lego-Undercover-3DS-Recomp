#include "runtime/ctr_memory.h"

#include <algorithm>
#include <limits>

namespace lego::ctr {
namespace {

constexpr std::uint32_t AlignDown(std::uint32_t value,
                                  std::uint32_t alignment) noexcept {
    return value & ~(alignment - 1U);
}

bool ValidSize(std::uint8_t size) noexcept {
    return size == 1U || size == 2U || size == 4U || size == 8U;
}

}  // namespace

bool GuestMemory::Map(std::uint32_t base, std::uint32_t size,
                      MemoryPermission permissions) {
    if (size == 0U) {
        return false;
    }
    const std::uint64_t end =
        static_cast<std::uint64_t>(base) + static_cast<std::uint64_t>(size);
    if (end > 0x1'0000'0000ULL) {
        return false;
    }
    for (const Region& region : regions_) {
        const std::uint64_t region_end =
            static_cast<std::uint64_t>(region.base) + region.size;
        if (base < region_end && region.base < end) {
            return false;
        }
    }
    regions_.push_back({base, size, permissions,
                        std::make_shared<PrivateBacking>(base, size)});
    std::sort(regions_.begin(), regions_.end(),
              [](const Region& left, const Region& right) {
                  return left.base < right.base;
              });
    return true;
}

bool GuestMemory::MapSharedServicePage(std::uint32_t base,
                                        std::shared_ptr<ServiceSharedMemoryObject> object,
                                        MemoryPermission permissions) {
    if (!object || (base & (kPageSize-1U)) != 0 ||
        static_cast<unsigned>(permissions) == 0 ||
        (static_cast<unsigned>(permissions) & ~object->other_permissions()) != 0)
        return false;
    const std::uint64_t end=std::uint64_t(base)+object->size();
    if (end>0x100000000ULL) return false;
    for (const auto& region:regions_)
        if (base<std::uint64_t(region.base)+region.size && region.base<end) return false;
    regions_.push_back({base,object->size(),permissions,{},std::move(object)});
    std::sort(regions_.begin(),regions_.end(),[](const Region& a,const Region& b){return a.base<b.base;});
    return true;
}

GuestMemory::Region* GuestMemory::FindRegion(
    std::uint32_t address, std::uint32_t size) noexcept {
    const std::uint64_t end =
        static_cast<std::uint64_t>(address) + size;
    if (size == 0U || end > 0x1'0000'0000ULL) {
        return nullptr;
    }
    for (Region& region : regions_) {
        const std::uint64_t region_end =
            static_cast<std::uint64_t>(region.base) + region.size;
        if (address >= region.base && end <= region_end) {
            return &region;
        }
    }
    return nullptr;
}

const GuestMemory::Region* GuestMemory::FindRegion(
    std::uint32_t address, std::uint32_t size) const noexcept {
    const std::uint64_t end =
        static_cast<std::uint64_t>(address) + size;
    if (size == 0U || end > 0x1'0000'0000ULL) {
        return nullptr;
    }
    for (const Region& region : regions_) {
        const std::uint64_t region_end =
            static_cast<std::uint64_t>(region.base) + region.size;
        if (address >= region.base && end <= region_end) {
            return &region;
        }
    }
    return nullptr;
}

bool GuestMemory::CanAccess(std::uint32_t address, std::uint32_t size,
                            MemoryPermission permission) const noexcept {
    const Region* region = FindRegion(address, size);
    if (!region || !HasPermission(region->permissions, permission)) return false;
    if (region->backing && !region->user_alias) {
        // Source aliases are interval overlays, so a protected subrange does not
        // fragment the underlying allocation or accidentally protect neighbours.
        for (const auto& alias:user_aliases_)
            if (std::uint64_t(address)<std::uint64_t(alias.source)+alias.size &&
                std::uint64_t(alias.source)<std::uint64_t(address)+size &&
                !HasPermission(alias.source_permissions, permission)) return false;
    }
    return true;
}

bool GuestMemory::IsMapped(std::uint32_t address,
                           std::uint32_t size) const noexcept {
    return FindRegion(address, size) != nullptr;
}

bool GuestMemory::IsReadable(std::uint32_t address,
                             std::uint32_t size) const noexcept {
    return CanAccess(address, size, MemoryPermission::Read);
}

bool GuestMemory::IsWritable(std::uint32_t address,
                             std::uint32_t size) const noexcept {
    return CanAccess(address, size, MemoryPermission::Write);
}

bool GuestMemory::SpansAlias(std::uint32_t a,std::uint32_t a_size,
                             std::uint32_t b,std::uint32_t b_size) const noexcept {
    if (!a_size || !b_size) return false;
    const auto* ar=FindRegion(a,a_size);
    const auto* br=FindRegion(b,b_size);
    if (!ar || !br) return false;
    if (ar==br) return std::uint64_t(a)<std::uint64_t(b)+b_size &&
                       std::uint64_t(b)<std::uint64_t(a)+a_size;
    if (ar->shared && ar->shared == br->shared) {
        const auto ao=std::uint64_t(a-ar->base), bo=std::uint64_t(b-br->base);
        return ao<bo+b_size && bo<ao+a_size;
    }
    if (!ar->backing || ar->backing != br->backing) return false;
    const auto ao=std::uint64_t(ar->backing_offset)+(a-ar->base);
    const auto bo=std::uint64_t(br->backing_offset)+(b-br->base);
    return ao<bo+b_size && bo<ao+a_size;
}

bool GuestMemory::LoadBytes(std::uint32_t address,
                            std::span<const std::uint8_t> data) {
    if (data.empty()) {
        return true;
    }
    Region* region = FindRegion(address, static_cast<std::uint32_t>(data.size()));
    if (region == nullptr) {
        return false;
    }
    const std::size_t offset = address - region->base;
    if (region->shared) return region->shared->Write(static_cast<std::uint32_t>(offset),data);
    std::copy(data.begin(), data.end(), region->MutableData().begin() + offset);
    return true;
}

bool GuestMemory::ZeroBytes(std::uint32_t address, std::uint32_t size) {
    if (size == 0U) {
        return true;
    }
    Region* region = FindRegion(address, size);
    if (region == nullptr) {
        return false;
    }
    const std::size_t offset = address - region->base;
    if (region->shared) {
        const std::array<std::uint8_t,ServiceSharedMemoryObject::kSize> zero{};
        return region->shared->Write(static_cast<std::uint32_t>(offset),std::span(zero).first(size));
    }
    std::fill(region->MutableData().begin() + offset,
              region->MutableData().begin() + offset + size, 0);
    return true;
}

bool GuestMemory::LoadLegoCodeImage(std::span<const std::uint8_t> code) {
    if (code.size() != kPreparedCodeBytes) {
        return false;
    }
    if (!Map(kTextBase, kTextAllocatedBytes,
             MemoryPermission::Read | MemoryPermission::Execute) ||
        !Map(kRodataBase, kRodataAllocatedBytes, MemoryPermission::Read) ||
        !Map(kDataBase, kBssMapEnd - kDataBase,
             MemoryPermission::Read | MemoryPermission::Write) ||
        !EnsureMainStack()) {
        return false;
    }

    const std::size_t rodata_offset = kTextAllocatedBytes;
    const std::size_t data_offset =
        kTextAllocatedBytes + kRodataAllocatedBytes;

    if (!LoadBytes(kTextBase,
                   code.subspan(0, kTextAllocatedBytes)) ||
        !LoadBytes(kRodataBase,
                   code.subspan(rodata_offset, kRodataAllocatedBytes)) ||
        !LoadBytes(kDataBase,
                   code.subspan(data_offset, kDataAllocatedBytes)) ||
        !ZeroBytes(kBssBegin, kBssEnd - kBssBegin)) {
        return false;
    }

    // Retail Old-3DS configuration page. Standard kernel/firmware fields
    // follow the Citra/Azahar HLE defaults. Memory mode 0 is independently
    // corroborated by the recovered LEGO Stage-2 run: a 64 MiB application
    // budget and the exact 0x124B000 + 0x2900000 heap allocations.
    std::array<std::uint8_t, kConfigMemorySize> config{};
    const auto put32 = [&](std::size_t offset, std::uint32_t value) {
        for (unsigned byte = 0; byte < 4; ++byte) {
            config[offset + byte] =
                static_cast<std::uint8_t>(value >> (byte * 8U));
        }
    };
    const auto put64 = [&](std::size_t offset, std::uint64_t value) {
        for (unsigned byte = 0; byte < 8; ++byte) {
            config[offset + byte] =
                static_cast<std::uint8_t>(value >> (byte * 8U));
        }
    };
    config[0x02] = 0x3A;
    config[0x03] = 0x02;
    put64(0x08, 0x0004013000008002ULL);
    put32(0x10, 0x00000002U);
    config[0x14] = 0x01;  // retail unit
    config[0x16] = 0x01;
    put32(0x18, 0x0000F450U);
    put32(0x30, 0x00000000U);  // Old-3DS memory mode 0
    put32(0x40, 0x04000000U);  // APPLICATION
    put32(0x44, 0x02C00000U);  // SYSTEM
    put32(0x48, 0x01400000U);  // BASE
    config[0x62] = 0x3A;
    config[0x63] = 0x02;
    put32(0x64, 0x00000002U);
    put32(0x68, 0x0000F450U);

    if (!Map(kConfigMemoryBase, kConfigMemorySize, MemoryPermission::Read) ||
        !LoadBytes(kConfigMemoryBase, config)) {
        return false;
    }
    return true;
}

bool GuestMemory::EnsureMainStack() {
    const std::uint32_t base = kMainStackTop - kMainStackBytes;
    if (IsMapped(base, kMainStackBytes)) {
        return IsWritable(base, kMainStackBytes);
    }
    return Map(base, kMainStackBytes,
               MemoryPermission::Read | MemoryPermission::Write);
}

bool GuestMemory::EnsureTlsMappings(const Kernel& kernel) {
    for (const auto& thread : kernel.threads()) {
        const std::uint32_t page =
            AlignDown(thread->tls_address, kPageSize);
        if (IsMapped(page, kPageSize)) {
            if (!IsWritable(page, kPageSize)) {
                return false;
            }
            continue;
        }
        if (!Map(page, kPageSize,
                 MemoryPermission::Read | MemoryPermission::Write)) {
            return false;
        }
    }
    return true;
}

bool GuestMemory::Read8(std::uint32_t address, std::uint8_t* value) {
    const Region* region = FindRegion(address, 1);
    if (value == nullptr || region == nullptr ||
        !CanAccess(address, 1, MemoryPermission::Read)) {
        return false;
    }
    *value = region->Data()[address - region->base];
    return true;
}

bool GuestMemory::Read16(std::uint32_t address, std::uint16_t* value) {
    if (value == nullptr || !CanAccess(address, 2, MemoryPermission::Read)) {
        return false;
    }
    std::uint64_t result = 0;
    if (!ReadSized(address, 2, &result)) {
        return false;
    }
    *value = static_cast<std::uint16_t>(result);
    return true;
}

bool GuestMemory::Read32(std::uint32_t address, std::uint32_t* value) {
    if (value == nullptr || !CanAccess(address, 4, MemoryPermission::Read)) {
        return false;
    }
    std::uint64_t result = 0;
    if (!ReadSized(address, 4, &result)) {
        return false;
    }
    *value = static_cast<std::uint32_t>(result);
    return true;
}

bool GuestMemory::Write8(std::uint32_t address, std::uint8_t value) {
    return WriteSized(address, 1, value);
}

bool GuestMemory::Write16(std::uint32_t address, std::uint16_t value) {
    return WriteSized(address, 2, value);
}

bool GuestMemory::Write32(std::uint32_t address, std::uint32_t value) {
    return WriteSized(address, 4, value);
}

bool GuestMemory::ReadSized(std::uint32_t address, std::uint8_t size,
                            std::uint64_t* value) noexcept {
    if (value == nullptr || !ValidSize(size) ||
        !CanAccess(address, size, MemoryPermission::Read)) {
        return false;
    }
    const Region* region = FindRegion(address, size);
    if (region == nullptr) {
        return false;
    }
    const std::size_t offset = address - region->base;
    std::uint64_t result = 0;
    for (std::uint8_t index = 0; index < size; ++index) {
        result |= static_cast<std::uint64_t>(region->Data()[offset + index])
                  << (index * 8U);
    }
    *value = result;
    return true;
}

bool GuestMemory::WriteSized(std::uint32_t address, std::uint8_t size,
                             std::uint64_t value) noexcept {
    if (!ValidSize(size) ||
        !CanAccess(address, size, MemoryPermission::Write)) {
        return false;
    }
    Region* region = FindRegion(address, size);
    if (region == nullptr) {
        return false;
    }
    const std::size_t offset = address - region->base;
    if (region->shared) {
        std::array<std::uint8_t,8> data{};
        for (std::uint8_t n=0;n<size;++n) data[n]=static_cast<std::uint8_t>(value>>(n*8U));
        return region->shared->Write(static_cast<std::uint32_t>(offset),std::span(data).first(size));
    }
    for (std::uint8_t index = 0; index < size; ++index) {
        region->MutableData()[offset + index] =
            static_cast<std::uint8_t>(value >> (index * 8U));
    }
    TouchExclusiveEpochs(address, size);
    return true;
}

bool GuestMemory::Read64(std::uint32_t address, std::uint64_t* value,
                         std::uint32_t* fault_address) {
    if (!ReadSized(address, 8, value)) {
        if (fault_address != nullptr) {
            *fault_address = address;
        }
        return false;
    }
    return true;
}

bool GuestMemory::Write64(std::uint32_t address, std::uint64_t value,
                          std::uint32_t* fault_address) {
    if (!WriteSized(address, 8, value)) {
        if (fault_address != nullptr) {
            *fault_address = address;
        }
        return false;
    }
    return true;
}

std::uint64_t GuestMemory::EpochFor(std::uint32_t address) const noexcept {
    const auto* region=FindRegion(address,1);
    if (region && region->shared) return region->shared->Epoch(address-region->base);
    if (region && region->backing) address = region->EpochAddress(address);
    const std::uint32_t granule = address & ~7U;
    const auto it = exclusive_epochs_.find(granule);
    return it == exclusive_epochs_.end() ? 0U : it->second;
}

void GuestMemory::TouchExclusiveEpochs(std::uint32_t address,
                                       std::uint32_t size) noexcept {
    if (size == 0U) {
        return;
    }
    const auto* region = FindRegion(address, size);
    if (region && region->backing) address = region->EpochAddress(address);
    const std::uint32_t first = address & ~7U;
    const std::uint32_t last =
        (address + size - 1U) & ~7U;
    for (std::uint32_t granule = first;; granule += 8U) {
        exclusive_epochs_[granule] = next_epoch_++;
        if (granule == last) {
            break;
        }
    }
}

bool GuestMemory::LoadExclusive(std::uint32_t address, std::uint8_t size,
                                std::uint64_t* value, std::uint64_t* token,
                                std::uint32_t* fault_address) {
    if (token == nullptr || !ReadSized(address, size, value)) {
        if (fault_address != nullptr) {
            *fault_address = address;
        }
        return false;
    }
    *token = EpochFor(address);
    return true;
}

a32::ExclusiveStoreResult GuestMemory::StoreExclusive(
    std::uint32_t address, std::uint8_t size, std::uint64_t value,
    std::uint64_t token, std::uint32_t* fault_address) {
    if (!ValidSize(size) ||
        !CanAccess(address, size, MemoryPermission::Write)) {
        if (fault_address != nullptr) {
            *fault_address = address;
        }
        return a32::ExclusiveStoreResult::MemoryFault;
    }
    if (EpochFor(address) != token) {
        return a32::ExclusiveStoreResult::ReservationLost;
    }
    if (!WriteSized(address, size, value)) {
        if (fault_address != nullptr) {
            *fault_address = address;
        }
        return a32::ExclusiveStoreResult::MemoryFault;
    }
    return a32::ExclusiveStoreResult::Success;
}

bool GuestMemory::AtomicSwap(std::uint32_t address, std::uint8_t size,
                             std::uint32_t replacement,
                             std::uint32_t* previous,
                             std::uint32_t* fault_address) {
    if (previous == nullptr || (size != 1U && size != 4U)) {
        if (fault_address != nullptr) {
            *fault_address = address;
        }
        return false;
    }
    std::uint64_t old = 0;
    if (!ReadSized(address, size, &old) ||
        !WriteSized(address, size, replacement)) {
        if (fault_address != nullptr) {
            *fault_address = address;
        }
        return false;
    }
    *previous = static_cast<std::uint32_t>(old);
    return true;
}

}  // namespace lego::ctr
