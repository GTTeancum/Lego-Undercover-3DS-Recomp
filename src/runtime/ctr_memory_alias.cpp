#include "runtime/ctr_memory.h"
#include <algorithm>
#include <new>

namespace lego::ctr {
namespace {
// Pinned kernel/errors.h; not the similarly named Kernel-module pointer codes.
constexpr Result InvalidAddress = 0xE0E01BF5U;
constexpr Result InvalidState = 0xE0A01BF5U;
constexpr std::uint32_t HeapBegin = 0x08000000U, HeapEnd = 0x10000000U;
bool Overlap(std::uint32_t a, std::uint32_t n, std::uint32_t b, std::uint32_t m) noexcept {
    return std::uint64_t(a) < std::uint64_t(b)+m && std::uint64_t(b) < std::uint64_t(a)+n;
}
}

std::optional<GuestMemory::PrivateState> GuestMemory::private_state(std::uint32_t address) const noexcept {
    const auto* region=FindRegion(address,1);
    if (!region || !region->backing) return std::nullopt;
    if (region->user_alias) return PrivateState::Alias;
    for (const auto& alias:user_aliases_)
        if (address>=alias.source && std::uint64_t(address)<std::uint64_t(alias.source)+alias.size)
            return PrivateState::Aliased;
    return PrivateState::Private;
}

std::optional<Result> GuestMemory::MapUserAlias(std::uint32_t target, std::uint32_t source,
                                                std::uint32_t size, std::uint32_t permissions) noexcept {
    if ((target&0xFFFU) || (source&0xFFFU)) return kResultMisalignedAddress;
    if (size&0xFFFU) return kResultMisalignedSize;
    if (permissions&~3U) return kResultInvalidCombination;
    // The pin asserts on zero-size VM carving. Do not invent hardware semantics.
    if (!size || size>kMaxUserAliasBytes) return std::nullopt;
    if (source<HeapBegin || std::uint64_t(source)+size>HeapEnd) return InvalidAddress;
    if (Overlap(source,size,target,size)) return InvalidState;
    // Target range is not settled by pinned Process::Map (its TODO). This slice
    // deliberately supports heap aliases only. Other ranges stop, not guess.
    if (target<HeapBegin || std::uint64_t(target)+size>HeapEnd) return std::nullopt;
    for (const auto& region:regions_)
        if (Overlap(target,size,region.base,region.size)) return InvalidState;
    const auto* source_region=FindRegion(source,size);
    if (!source_region) {
        // Missing/private-state-invalid spans are real guest errors; a valid
        // multi-region source is merely outside this single-backing slice.
        std::uint64_t at=source, end=std::uint64_t(source)+size;
        for (const auto& region:regions_) {
            if (std::uint64_t(region.base)+region.size<=at) continue;
            if (region.base>at) break;
            if (!region.backing || region.user_alias ||
                !HasPermission(region.permissions,MemoryPermission::Read|MemoryPermission::Write)) return InvalidState;
            at=std::min(end,std::uint64_t(region.base)+region.size);
            if (at==end) return std::nullopt;
        }
        return InvalidState;
    }
    if (!source_region->backing || source_region->user_alias ||
        !HasPermission(source_region->permissions,MemoryPermission::Read|MemoryPermission::Write)) return InvalidState;
    for (const auto& alias:user_aliases_)
        if (Overlap(source,size,alias.source,alias.size)) return InvalidState;
    // RWX sources need partial reprotection not modeled here. Ordinary RW source
    // permissions stay RW exactly as in the unprivileged reference path.
    if (source_region->permissions!=(MemoryPermission::Read|MemoryPermission::Write)) return std::nullopt;
    if (user_aliases_.size()>=kMaxUserAliases) return std::nullopt;
    try {
        // Copy only address-space metadata/shared_ptrs, NEVER the backing bytes.
        // Both allocations complete before either mapping or source state changes.
        auto regions=regions_;
        auto aliases=user_aliases_;
        const auto offset=source_region->backing_offset+source-source_region->base;
        regions.push_back({target,size,static_cast<MemoryPermission>(permissions),source_region->backing,{},offset,true});
        aliases.push_back({source,target,size,static_cast<MemoryPermission>(permissions)});
        std::sort(regions.begin(),regions.end(),[](const Region& a,const Region& b){return a.base<b.base;});
        regions_.swap(regions);
        user_aliases_.swap(aliases);
        return kResultSuccess;
    } catch (const std::bad_alloc&) {
        return std::nullopt; // No partial map, byte change, Result or CPU update.
    }
}
std::optional<Result> GuestMemory::UnmapUserAlias(std::uint32_t target, std::uint32_t source,
                                                  std::uint32_t size, std::uint32_t permissions) noexcept {
    if ((target&0xFFFU) || (source&0xFFFU)) return kResultMisalignedAddress;
    if (size&0xFFFU) return kResultMisalignedSize;
    if (permissions&~3U) return kResultInvalidCombination;
    if (!size || size>kMaxUserAliasBytes) return std::nullopt;
    if (source<HeapBegin || std::uint64_t(source)+size>HeapEnd) return InvalidAddress;
    if (Overlap(source,size,target,size)) return InvalidState;
    if (target<HeapBegin || std::uint64_t(target)+size>HeapEnd) return std::nullopt;
    // Pinned Process::Unmap uses the REQUESTED source permissions, not the
    // remembered ones. This exact-pair slice restores only RW; other modes need
    // persistent private protection after the alias overlay is removed.
    constexpr auto rw=MemoryPermission::Read|MemoryPermission::Write;
    if (permissions!=static_cast<unsigned>(rw)) return std::nullopt;
    const auto alias=std::find_if(user_aliases_.begin(),user_aliases_.end(),
        [&](const UserAlias& item){return item.target==target && item.source==source && item.size==size;});
    // The pin's wrong-pair Result is only a TODO. Unproved pairs and partial
    // ranges remain host stops instead of borrowing that unverified error.
    if (alias==user_aliases_.end()) return std::nullopt;
    const auto* original=FindRegion(source,size);
    const auto* view=FindRegion(target,size);
    if (!original || !view || !original->backing || original->user_alias ||
        original->shared || original->device || original->permissions!=rw ||
        !view->user_alias || view->base!=target || view->size!=size ||
        view->shared || view->device || view->permissions!=alias->permissions ||
        view->backing!=original->backing ||
        std::uint64_t(view->backing_offset)!=std::uint64_t(original->backing_offset)+source-original->base)
        return std::nullopt; // Broken host metadata is not a guest Result.
    const auto view_index=static_cast<std::size_t>(view-regions_.data());
    // All checks precede publication. Erasing these metadata records cannot
    // allocate or fail. The original region retains the same backing and epoch
    // keys; removing the source overlay restores its established RW permissions.
    regions_.erase(regions_.begin()+view_index);
    user_aliases_.erase(alias);
    return kResultSuccess;
}

std::optional<Result> GuestMemory::ProtectUserAlias(std::uint32_t address, std::uint32_t size,
                                                    std::uint32_t permissions) noexcept {
    if (address&0xFFFU) return kResultMisalignedAddress;
    if (size&0xFFFU) return kResultMisalignedSize;
    if (permissions&~3U) return kResultInvalidCombination;
    if (!size || size>kMaxUserAliasBytes || address<HeapBegin ||
        std::uint64_t(address)+size>HeapEnd) return std::nullopt;
    for (auto& alias:user_aliases_) {
        if (alias.source==address && alias.size==size) {
            alias.source_permissions=static_cast<MemoryPermission>(permissions);
            return kResultSuccess;
        }
        if (alias.target==address && alias.size==size) {
            auto* region=FindRegion(address,size);
            if (!region || !region->user_alias || region->base!=address || region->size!=size)
                return std::nullopt; // Broken host metadata is not a guest Result.
            region->permissions=static_cast<MemoryPermission>(permissions);
            alias.permissions=region->permissions;
            return kResultSuccess;
        }
    }
    // The original sequence immediately reprotects its restored private source
    // to the RW permissions it already has. A single real private region with
    // matching permissions needs no subdivision or state/byte/epoch change.
    const auto* region=FindRegion(address,size);
    if (!region || !region->backing || region->user_alias || region->shared || region->device ||
        region->permissions!=static_cast<MemoryPermission>(permissions)) return std::nullopt;
    for (const auto& alias:user_aliases_)
        if (Overlap(address,size,alias.source,alias.size)) return std::nullopt;
    // Changes to private permissions, multi-region spans and partial alias
    // overlays remain explicit host stops; no general reprotection is inferred.
    return kResultSuccess;
}
} // namespace lego::ctr
