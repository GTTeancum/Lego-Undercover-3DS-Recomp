#include "runtime/ctr_memory.h"

#include <array>

namespace lego::ctr {
namespace {

template <std::size_t N>
void Put64(std::array<std::uint8_t, N>& bytes, std::size_t offset,
           std::uint64_t value) noexcept {
    for (unsigned i = 0; i < 8; ++i)
        bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8U));
}

}  // namespace

bool GuestMemory::EnsureSharedClockPage(std::uint64_t now_ns,
                                        std::uint64_t rtc_epoch_ms) {
    if (!ValidRtcEpoch(rtc_epoch_ms)) return false;

    if (shared_clock_initialized_) {
        // Refuse a clock rebase or rollback rather than silently changing the
        // reader's epoch. Region ownership/permissions must still be intact.
        const Region* region = FindRegion(kSharedPageBase, kSharedPageSize);
        if (rtc_epoch_ms != shared_clock_epoch_ms_ ||
            now_ns < shared_clock_last_seen_ns_ || !region ||
            region->base != kSharedPageBase || region->size != kSharedPageSize ||
            region->permissions != MemoryPermission::Read) return false;
        shared_clock_last_seen_ns_ = now_ns;
        if (now_ns / kSharedClockRefreshNs ==
            shared_clock_last_update_ns_ / kSharedClockRefreshNs) return true;
    } else {
        // Map rejects *any* overlap. Do not adopt an arbitrary pre-existing
        // writable/partial mapping or erase another host component's bytes.
        if (!Map(kSharedPageBase, kSharedPageSize, MemoryPermission::Read)) return false;
        shared_clock_epoch_ms_ = rtc_epoch_ms;
        shared_clock_counter_ = 0;
        shared_clock_initialized_ = true;
    }

    // Pinned shared_page.cpp alternates to the inactive record, populates all
    // fields, then publishes ++counter. The guest selects (counter & 1).
    // A late refresh publishes the time actually supplied at this boundary;
    // missed hourly callbacks coalesce, not a loop that invents elapsed work.
    const std::uint32_t next_counter = shared_clock_counter_ + 1U;
    const std::uint32_t record_address = kSharedPageBase + kSharedClockSnapshot0Offset +
        (next_counter & 1U) * kSharedClockSnapshotBytes;
    std::array<std::uint8_t, kSharedClockSnapshotBytes> record{};
    Put64(record, 0x00, rtc_epoch_ms + now_ns / kNanosecondsPerMillisecond);
    Put64(record, 0x08, SystemTicksFromNanoseconds(now_ns));
    Put64(record, 0x10, kArm11TicksPerSecond);
    Put64(record, 0x18, 0); // No clock-adjustment drift/offset policy implemented.

    std::array<std::uint8_t, 4> counter{};
    for (unsigned i = 0; i < 4; ++i)
        counter[i] = static_cast<std::uint8_t>(next_counter >> (i * 8U));

    // LoadBytes is the loader/host path and intentionally bypasses guest write
    // permissions. Both extents were preflighted above; publication is complete
    // before this single-threaded host re-enters guest dispatch.
    if (!LoadBytes(record_address, record) ||
        !LoadBytes(kSharedPageBase + kSharedClockCounterOffset, counter)) return false;
    shared_clock_counter_ = next_counter;
    shared_clock_last_seen_ns_ = now_ns;
    shared_clock_last_update_ns_ = now_ns;
    return true;
}

}  // namespace lego::ctr
