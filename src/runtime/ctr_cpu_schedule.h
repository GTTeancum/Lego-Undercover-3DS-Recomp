#pragma once
#include <cstdint>
#include <limits>
#include <optional>
#include "runtime/ctr_clock.h"

namespace lego::ctr {
enum class CpuExecutionMode { Strict, DiagnosticDual };
// Explicit diagnostic model: both logical cores can issue one recorded A32
// instruction per ARM11 tick, in core-0 then core-1 order. This is NOT measured
// instruction latency, bus/cache timing, or a physical parallel host execution.
inline constexpr std::uint64_t kCpuQuotaPeriodTicks = 536223; // floor(nsToCycles(2ms))
inline std::optional<std::uint64_t> CpuTickDeadline(std::uint64_t tick,
                                                  std::uint64_t origin=0) noexcept {
    constexpr auto max=std::numeric_limits<std::uint64_t>::max();
    const auto sec=tick/kArm11TicksPerSecond, rem=tick%kArm11TicksPerSecond;
    if(sec>max/kNanosecondsPerSecond) return std::nullopt;
    const auto whole=sec*kNanosecondsPerSecond;
    const auto part=(rem*kNanosecondsPerSecond+kArm11TicksPerSecond-1)/kArm11TicksPerSecond;
    if(part>max-whole || origin>max-whole-part) return std::nullopt;
    return origin+whole+part;
}
// Pinned HLE Multi limiter (also used by its unimplemented Single alias).
// No sysmodule process is executed here; its window denies application core 1.
// Core 0 is never gated. Zero is the reference PREEMPTION_DISABLED sentinel.
class Core1Quota final {
public:
    bool Update(std::uint32_t percent,std::uint64_t now) noexcept {
        if(percent>100) return false;
        if(percent==0){percent_=0;deadline_.reset();return true;}
        const bool next_app=!app_phase_;
        const auto app=kCpuQuotaPeriodTicks*percent/100;
        const auto duration=next_app?app:kCpuQuotaPeriodTicks-app;
        if(now>std::numeric_limits<std::uint64_t>::max()-duration) return false;
        percent_=percent;app_phase_=next_app;deadline_=now+duration;return true;
    }
    bool Consume(std::uint64_t now) noexcept {
        if(!deadline_||now<*deadline_)return false;
        const auto app=kCpuQuotaPeriodTicks*percent_/100;
        const bool next_app=!app_phase_;
        const auto duration=next_app?app:kCpuQuotaPeriodTicks-app;
        if(*deadline_>std::numeric_limits<std::uint64_t>::max()-duration)return false;
        deadline_=*deadline_+duration;app_phase_=next_app;return true;
    }
    [[nodiscard]] bool allows_application()const noexcept{return !deadline_||app_phase_;}
    [[nodiscard]] std::optional<std::uint64_t> deadline()const noexcept{return deadline_;}
    [[nodiscard]] std::uint32_t percent()const noexcept{return percent_;}
private:
    std::uint32_t percent_{};
    bool app_phase_{true}; // First enabled update switches to SYS, as the pin.
    std::optional<std::uint64_t> deadline_;
};
} // namespace lego::ctr
