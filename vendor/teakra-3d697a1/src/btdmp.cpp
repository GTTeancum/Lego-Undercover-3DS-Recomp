#include <stdexcept>
#include "btdmp.h"

namespace Teakra {
Btdmp::Btdmp(CoreTiming& core_timing) { core_timing.RegisterCallbacks(this); }
Btdmp::~Btdmp() = default;
void Btdmp::Reset() {
    transmit_setup = {0,0,0x1FFF,0,0};
    transmit_setup_written = 0;
    transmit_fifo_control = 0;
    transmit_control = 0x0005;
    transmit_clock_config = 0;
    transmit_period = 4096; // Inherited external-clock diagnostic period.
    transmit_timer = 0;
    transmit_enable = 0;
    transmit_empty = true;
    transmit_full = false;
    transmit_queue = {};
}
void Btdmp::Tick() {
    if (!transmit_enable) return;
    if (++transmit_timer < transmit_period) return;
    transmit_timer = 0;
    // Default underrun behavior is an explicit fault. The opt-in reference
    // fallback below tags absent words separately from genuine FIFO output.
    if (transmit_queue.size() < 2 && !reference_underflow_silence)
        throw std::runtime_error("BTDMP transmit underrun output is unmodeled");
    if (!audio_callback)
        throw std::runtime_error("BTDMP transmitter has no audio consumer");
    std::array<std::int16_t,2> sample{};
    u8 fifo_mask = 0;
    for (unsigned channel=0; channel<2; ++channel) {
        if (!transmit_queue.empty()) {
            sample[channel] = static_cast<s16>(transmit_queue.front());
            transmit_queue.pop();
            fifo_mask |= static_cast<u8>(1U << channel);
        }
        // A zero without its fifo_mask bit is explicit reference underflow,
        // NOT firmware output. Do not signal an empty transition without a pop.
    }
    transmit_empty = transmit_queue.empty();
    transmit_full = false;
    // In the selected reference preset, preserve the PINNED empty-transition
    // IRQ. Control-field hardware decoding remains unresolved, not guessed.
    if (fifo_mask && transmit_empty && interrupt_handler)
        interrupt_handler();
    audio_callback(sample,fifo_mask);
}
u64 Btdmp::GetMaxSkip() const {
    if (!transmit_enable) return Infinity;
    // Never skip an audio callback, including an empty-FIFO fault. The previous
    // infinite skip on empty suppressed underflow and could hide missing output.
    return transmit_timer < transmit_period ? transmit_period-transmit_timer-1 : 0;
}
void Btdmp::Skip(u64 ticks) {
    if (!transmit_enable) return;
    if (ticks > GetMaxSkip())
        throw std::logic_error("BTDMP skip crosses an observable sample deadline");
    transmit_timer = static_cast<u16>(transmit_timer + ticks);
}
} // namespace Teakra
