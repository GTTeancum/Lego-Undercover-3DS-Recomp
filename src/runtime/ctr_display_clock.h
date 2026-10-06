#pragma once
#include <cstdint>
#include <limits>
#include <optional>
#include "runtime/ctr_clock.h"

namespace lego::ctr {
enum class DisplayClockMode : std::uint8_t { Disabled, ReferenceIdle };
// Pinned VideoCore::FRAME_TICKS, not an assumed 60 Hz. Nanosecond deadlines
// round UP absolute cycle positions, avoiding early delivery and per-frame drift.
inline constexpr std::uint64_t kDisplayPeriodTicks = 4481136ULL;
class DisplayClock final {
public:
    explicit DisplayClock(std::uint64_t origin_ns=0) noexcept : origin_(origin_ns) {}
    [[nodiscard]] std::optional<std::uint64_t> next_deadline_ns() const noexcept {
        constexpr auto max=std::numeric_limits<std::uint64_t>::max();
        if (period_>max/kDisplayPeriodTicks) return std::nullopt;
        const auto ticks=period_*kDisplayPeriodTicks;
        const auto seconds=ticks/kArm11TicksPerSecond;
        const auto remainder=ticks%kArm11TicksPerSecond;
        if (seconds>max/kNanosecondsPerSecond) return std::nullopt;
        const auto whole=seconds*kNanosecondsPerSecond;
        const auto fraction=(remainder*kNanosecondsPerSecond+kArm11TicksPerSecond-1)/kArm11TicksPerSecond;
        if (fraction>max-whole || origin_>max-whole-fraction) return std::nullopt;
        return origin_+whole+fraction;
    }
    bool Consume(std::uint64_t now_ns) noexcept {
        const auto deadline=next_deadline_ns();
        if (!deadline || now_ns<*deadline) return false;
        ++period_;
        return true;
    }
    [[nodiscard]] std::uint64_t periods_delivered() const noexcept { return period_-1; }
private:
    std::uint64_t origin_{},period_{1};
};
} // namespace lego::ctr
