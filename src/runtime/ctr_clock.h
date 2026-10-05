#pragma once

#include <cstdint>
#include <limits>

namespace lego::ctr {

// One guest-time domain for the shared-page snapshots and svcGetSystemTick.
// Rate/epoch units follow the pinned CTR reference; no host wall clock is read.
inline constexpr std::uint64_t kArm11TicksPerSecond = 268111856ULL;
inline constexpr std::uint64_t kNanosecondsPerSecond = 1000000000ULL;
inline constexpr std::uint64_t kNanosecondsPerMillisecond = 1000000ULL;
inline constexpr std::uint64_t kSharedClockRefreshNs = 3600ULL * kNanosecondsPerSecond;

// Explicit deterministic RTC policy, NOT a recovered console clock setting:
// 2000-01-01 00:00:00, expressed as milliseconds since 1900-01-01.
inline constexpr std::uint64_t kDefaultRtcMsSince1900 = 3155673600000ULL;
inline constexpr std::uint64_t kMaxRtcEpochMs =
    std::numeric_limits<std::uint64_t>::max() -
    std::numeric_limits<std::uint64_t>::max() / kNanosecondsPerMillisecond;

constexpr bool ValidRtcEpoch(std::uint64_t milliseconds) noexcept {
    return milliseconds >= kDefaultRtcMsSince1900 && milliseconds <= kMaxRtcEpochMs;
}

constexpr std::uint64_t SystemTicksFromNanoseconds(std::uint64_t nanoseconds) noexcept {
    // Splitting before multiplication avoids overflow throughout the uint64 ns range.
    return (nanoseconds / kNanosecondsPerSecond) * kArm11TicksPerSecond +
           ((nanoseconds % kNanosecondsPerSecond) * kArm11TicksPerSecond) /
               kNanosecondsPerSecond;
}

inline constexpr std::uint32_t kSharedPageBase = 0x1FF81000U;
inline constexpr std::uint32_t kSharedPageSize = 0x1000U;
inline constexpr std::uint32_t kSharedClockCounterOffset = 0U;
inline constexpr std::uint32_t kSharedClockSnapshot0Offset = 0x20U;
inline constexpr std::uint32_t kSharedClockSnapshotBytes = 0x20U;

}  // namespace lego::ctr
