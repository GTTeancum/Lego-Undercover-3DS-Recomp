#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

#include "recomp/a32_runtime.h"
#include "runtime/ctr_kernel.h"
#include "runtime/ctr_clock.h"
#include "runtime/ctr_shared_memory.h"
#include "runtime/ctr_device_memory.h"

namespace lego::ctr {
namespace a32 = oot3d::recomp::a32;

enum class MemoryPermission : std::uint8_t {
    None = 0,
    Read = 1U << 0,
    Write = 1U << 1,
    Execute = 1U << 2,
};

constexpr MemoryPermission operator|(MemoryPermission left,
                                     MemoryPermission right) noexcept {
    return static_cast<MemoryPermission>(
        static_cast<std::uint8_t>(left) |
        static_cast<std::uint8_t>(right));
}

constexpr bool HasPermission(MemoryPermission value,
                             MemoryPermission required) noexcept {
    return (static_cast<std::uint8_t>(value) &
            static_cast<std::uint8_t>(required)) ==
           static_cast<std::uint8_t>(required);
}

inline constexpr std::uint32_t kTextBase = 0x00100000U;
inline constexpr std::uint32_t kTextAllocatedBytes = 0x00257000U;
inline constexpr std::uint32_t kRodataBase = 0x00357000U;
inline constexpr std::uint32_t kRodataAllocatedBytes = 0x00014000U;
inline constexpr std::uint32_t kDataBase = 0x0036B000U;
inline constexpr std::uint32_t kDataAllocatedBytes = 0x0001C000U;
inline constexpr std::uint32_t kBssBegin = 0x00386670U;
inline constexpr std::uint32_t kBssEnd = 0x005A45D8U;
inline constexpr std::uint32_t kBssMapEnd = 0x005A5000U;
inline constexpr std::uint32_t kPreparedCodeBytes = 0x00287000U;

inline constexpr std::uint32_t kMainStackTop = 0x10000000U;
inline constexpr std::uint32_t kMainStackBytes = 0x00010000U;
inline constexpr std::uint32_t kPageSize = 0x1000U;
inline constexpr std::uint32_t kConfigMemoryBase = 0x1FF80000U;
inline constexpr std::uint32_t kConfigMemorySize = 0x1000U;

class GuestMemory final : public a32::MemoryBus {
public:
    GuestMemory() = default;
    // One logical address space owns private epoch identity. Copying it must not
    // accidentally share private bytes while detaching reservation metadata.
    GuestMemory(const GuestMemory&) = delete;
    GuestMemory& operator=(const GuestMemory&) = delete;
    GuestMemory(GuestMemory&&) = default;
    GuestMemory& operator=(GuestMemory&&) = default;

    enum class PrivateState { Private, Aliased, Alias };
    struct UserAlias {
        std::uint32_t source{}, target{}, size{};
        MemoryPermission permissions{};
        MemoryPermission source_permissions{MemoryPermission::Read|MemoryPermission::Write};
    };
    // Bounded unprivileged ControlMemory(Map). nullopt is an explicit host stop,
    // never a fabricated guest Result. Mapping shares bytes AND exclusive epochs.
    // Source-state intervals are recorded without fragmenting its backing region.
    std::optional<Result> MapUserAlias(std::uint32_t target, std::uint32_t source,
                                        std::uint32_t size, std::uint32_t permissions) noexcept;
    // Bounded Protect for a complete recorded alias source or target. The
    // opposite view's permissions and the shared bytes/reservations are retained.
    std::optional<Result> ProtectUserAlias(std::uint32_t address, std::uint32_t size,
                                           std::uint32_t permissions) noexcept;
    [[nodiscard]] std::optional<PrivateState> private_state(std::uint32_t address) const noexcept;
    [[nodiscard]] std::span<const UserAlias> user_aliases() const noexcept { return user_aliases_; }
    static constexpr std::uint32_t kMaxUserAliasBytes = 0x00100000; // Host policy.
    static constexpr std::size_t kMaxUserAliases = 128; // Host policy, not 3DS capacity.

    bool Map(std::uint32_t base, std::uint32_t size,
             MemoryPermission permissions);
    // Only the bounded service-allocated page kind is currently modeled.
    // Retain the backing object; mappings never receive a detached zero-filled copy.
    bool MapSharedServicePage(std::uint32_t base,
                              std::shared_ptr<ServiceSharedMemoryObject> object,
                              MemoryPermission permissions);
    // No copy of device bytes. Exactly one retained object per mapping.
    bool MapDeviceMemory(std::uint32_t base, std::shared_ptr<DeviceMemory> object,
                         MemoryPermission permissions);
    bool UnmapDeviceMemory(std::uint32_t base, const DeviceMemory& object) noexcept;
    bool IsMapped(std::uint32_t address, std::uint32_t size = 1) const noexcept;
    bool IsReadable(std::uint32_t address, std::uint32_t size = 1) const noexcept;
    bool IsWritable(std::uint32_t address, std::uint32_t size = 1) const noexcept;

    // True when mapped spans overlap physically, including distinct aliases of
    // a service-owned shared page. Invalid/unmapped spans return false; callers
    // must preflight permissions and bounds before relying on this predicate.
    bool SpansAlias(std::uint32_t a,std::uint32_t a_size,
                    std::uint32_t b,std::uint32_t b_size) const noexcept;

    // Used when a service accesses a shared object directly rather than through
    // a known guest VA. Callers preflight the complete span first. No mapping,
    // memory or reservation state is modified by this identity query.
    bool UsesSharedBacking(std::uint32_t address, std::uint32_t size,
                           const ServiceSharedMemoryObject& object) const noexcept;

    // Service/device commit path. Prepare validates a private writable region
    // and allocates reservation metadata only (epochs/bytes remain unchanged).
    // Commit performs no allocation and refuses an unprepared span. No mapping
    // may change between these calls in the synchronous service handler.
    bool PrepareDeviceWrite(std::uint32_t address, std::uint32_t size);
    bool CommitDeviceWrite(std::uint32_t address, std::span<const std::uint8_t> data) noexcept;

    bool LoadBytes(std::uint32_t address, std::span<const std::uint8_t> data);
    bool ZeroBytes(std::uint32_t address, std::uint32_t size);

    bool LoadLegoCodeImage(std::span<const std::uint8_t> code);
    bool EnsureMainStack();
    bool EnsureTlsMappings(const Kernel& kernel);

    // Dedicated host-owned read-only page. Unknown non-clock fields remain
    // unmodeled (zero); this does not claim hardware/battery/network state.
    // Refresh only observes supplied guest time; it never advances the kernel.
    bool EnsureSharedClockPage(std::uint64_t now_ns,
                              std::uint64_t rtc_epoch_ms = kDefaultRtcMsSince1900);

    bool Read8(std::uint32_t address, std::uint8_t* value) override;
    bool Read16(std::uint32_t address, std::uint16_t* value) override;
    bool Read32(std::uint32_t address, std::uint32_t* value) override;
    bool Write8(std::uint32_t address, std::uint8_t value) override;
    bool Write16(std::uint32_t address, std::uint16_t value) override;
    bool Write32(std::uint32_t address, std::uint32_t value) override;

    bool Read64(std::uint32_t address, std::uint64_t* value,
                std::uint32_t* fault_address) override;
    bool Write64(std::uint32_t address, std::uint64_t value,
                 std::uint32_t* fault_address) override;

    bool LoadExclusive(std::uint32_t address, std::uint8_t size,
                       std::uint64_t* value, std::uint64_t* token,
                       std::uint32_t* fault_address) override;
    a32::ExclusiveStoreResult StoreExclusive(
        std::uint32_t address, std::uint8_t size, std::uint64_t value,
        std::uint64_t token, std::uint32_t* fault_address) override;
    bool AtomicSwap(std::uint32_t address, std::uint8_t size,
                    std::uint32_t replacement, std::uint32_t* previous,
                    std::uint32_t* fault_address) override;

private:
    struct PrivateBacking {
        std::uint32_t origin{}; // Stable canonical key for this bank's epochs.
        std::vector<std::uint8_t> bytes;
        PrivateBacking(std::uint32_t address, std::uint32_t size) : origin(address), bytes(size, 0) {}
    };
    struct Region {
        std::uint32_t base{};
        std::uint32_t size{};
        MemoryPermission permissions{MemoryPermission::None};
        std::shared_ptr<PrivateBacking> backing;
        std::shared_ptr<ServiceSharedMemoryObject> shared;
        std::uint32_t backing_offset{};
        bool user_alias{};
        std::shared_ptr<DeviceMemory> device;
        [[nodiscard]] std::span<const std::uint8_t> Data() const noexcept {
            return shared ? shared->bytes() : std::span<const std::uint8_t>(backing->bytes).subspan(backing_offset, size);
        }
        [[nodiscard]] std::span<std::uint8_t> MutableData() noexcept {
            return std::span<std::uint8_t>(backing->bytes).subspan(backing_offset, size);
        }
        [[nodiscard]] std::uint32_t EpochAddress(std::uint32_t address) const noexcept {
            return backing->origin + backing_offset + (address - base);
        }
    };

    Region* FindRegion(std::uint32_t address, std::uint32_t size) noexcept;
    const Region* FindRegion(std::uint32_t address,
                             std::uint32_t size) const noexcept;
    bool CanAccess(std::uint32_t address, std::uint32_t size,
                   MemoryPermission permission) const noexcept;
    bool ReadSized(std::uint32_t address, std::uint8_t size,
                   std::uint64_t* value) noexcept;
    bool WriteSized(std::uint32_t address, std::uint8_t size,
                    std::uint64_t value) noexcept;
    void TouchExclusiveEpochs(std::uint32_t address, std::uint32_t size) noexcept;
    std::uint64_t EpochFor(std::uint32_t address) const noexcept;

    std::vector<Region> regions_{};
    std::vector<UserAlias> user_aliases_{};
    std::unordered_map<std::uint32_t, std::uint64_t> exclusive_epochs_{};
    std::uint64_t next_epoch_{1};
    bool shared_clock_initialized_{};
    std::uint64_t shared_clock_epoch_ms_{};
    std::uint64_t shared_clock_last_seen_ns_{};
    std::uint64_t shared_clock_last_update_ns_{};
    std::uint32_t shared_clock_counter_{};
};

inline bool GuestMemory::UsesSharedBacking(std::uint32_t address, std::uint32_t size,
                                     const ServiceSharedMemoryObject& object) const noexcept {
    if (size == 0) return false;
    const auto* region = FindRegion(address, size);
    return region && region->shared.get() == &object;
}

}  // namespace lego::ctr
