#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

#include "recomp/a32_runtime.h"
#include "runtime/ctr_kernel.h"
#include "runtime/ctr_clock.h"

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

    bool Map(std::uint32_t base, std::uint32_t size,
             MemoryPermission permissions);
    bool IsMapped(std::uint32_t address, std::uint32_t size = 1) const noexcept;
    bool IsReadable(std::uint32_t address, std::uint32_t size = 1) const noexcept;
    bool IsWritable(std::uint32_t address, std::uint32_t size = 1) const noexcept;

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
    struct Region {
        std::uint32_t base{};
        std::uint32_t size{};
        MemoryPermission permissions{MemoryPermission::None};
        std::vector<std::uint8_t> bytes{};
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
    std::unordered_map<std::uint32_t, std::uint64_t> exclusive_epochs_{};
    std::uint64_t next_epoch_{1};
    bool shared_clock_initialized_{};
    std::uint64_t shared_clock_epoch_ms_{};
    std::uint64_t shared_clock_last_seen_ns_{};
    std::uint64_t shared_clock_last_update_ns_{};
    std::uint32_t shared_clock_counter_{};
};

}  // namespace lego::ctr
