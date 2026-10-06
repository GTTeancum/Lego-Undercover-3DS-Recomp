#pragma once

#include <array>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <utility>
#include "common_types.h"
#include "core_timing.h"

namespace Teakra {

// Local guarded-Teakra extension. Logical input/pending semantics follow Martin
// Korth, GBATEK 3.03, DSi Teak ICU (printed pp.363-364). This is NOT a claim of
// measured 3DS pin timing. Existing peripheral notifications are explicit pulses;
// integrations requiring held hardware levels must use SetLine instead.
class ICU final : public CoreTiming::Callbacks {
public:
    explicit ICU(CoreTiming& timing) { Reset(); timing.RegisterCallbacks(this); }
    void Reset() {
        std::lock_guard lock(mutex);
        request = manual = sampled_manual = previous = master_disable = 0;
        trigger_mode = polarity = 0x2000; // GBATEK documented register reset image.
        hardware = 0x2000; // Unconnected SIO held inactive, not simulated SIO activity.
        manual_pipeline.fill(0); enabled.fill(0); reported.fill(0);
        vectored_enabled = reported_vectored = 0;
        vector_low.fill(0xFC00); vector_high.fill(3); vector_context_switch.fill(0);
    }
    u16 GetRequest() const { std::lock_guard lock(mutex); return request; }
    void Acknowledge(u16 bits) {
        Notify notify;
        {
            std::lock_guard lock(mutex);
            // Edge mode clears regardless of input; level mode cannot acknowledge
            // while the (polarity-adjusted, manually ORed, enabled) line is asserted.
            const u16 held = InputLocked() & static_cast<u16>(~trigger_mode);
            request &= static_cast<u16>(~(bits & static_cast<u16>(~held)));
            // Reassert ignored acknowledgements through the inherited core notifier.
            for (auto& seen : reported) seen &= static_cast<u16>(~(bits & held));
            reported_vectored &= static_cast<u16>(~(bits & held));
            notify = RoutesLocked();
        }
        Deliver(notify);
    }
    u16 GetAcknowledge() const { return 0; } // Retained reference read policy.
    void Trigger(u16 bits) { std::lock_guard lock(mutex); manual = bits; }
    u16 GetTrigger() const { std::lock_guard lock(mutex); return manual; }
    void SetLine(u32 irq, bool high) {
        if (irq >= 16) throw std::out_of_range("ICU IRQ index");
        Notify notify;
        {
            std::lock_guard lock(mutex);
            const u16 bit = static_cast<u16>(1U << irq);
            hardware = high ? static_cast<u16>(hardware | bit) : static_cast<u16>(hardware & ~bit);
            notify = SampleLocked();
        }
        Deliver(notify);
    }
    // The pinned Timer/APBP/DMA/BTDMP API reports events, not sustained pins.
    // Adapt each event as a raw high-then-low pulse. No event is synthesized here.
    void TriggerSingle(u32 irq) { SetLine(irq, true); SetLine(irq, false); }
    void SetEnable(u32 index, u16 bits) {
        if (index >= enabled.size()) throw std::out_of_range("ICU core interrupt index");
        Notify notify;
        { std::lock_guard lock(mutex); enabled[index] = bits; notify = RoutesLocked(); }
        Deliver(notify);
    }
    void SetEnableVectored(u16 bits) {
        Notify notify;
        { std::lock_guard lock(mutex); vectored_enabled = bits; notify = RoutesLocked(); }
        Deliver(notify);
    }
    u16 GetEnable(u32 index) const {
        if (index >= enabled.size()) throw std::out_of_range("ICU core interrupt index");
        std::lock_guard lock(mutex); return enabled[index];
    }
    u16 GetEnableVectored() const { std::lock_guard lock(mutex); return vectored_enabled; }
    void SetTriggerMode(u16 bits) { std::lock_guard lock(mutex); trigger_mode = bits; }
    u16 GetTriggerMode() const { std::lock_guard lock(mutex); return trigger_mode; }
    void SetPolarity(u16 bits) { std::lock_guard lock(mutex); polarity = bits; }
    u16 GetPolarity() const { std::lock_guard lock(mutex); return polarity; }
    void SetMasterDisable(u16 bits) { std::lock_guard lock(mutex); master_disable = bits; }
    u16 GetMasterDisable() const { std::lock_guard lock(mutex); return master_disable; }
    u32 GetVector(u32 irq) const {
        if (irq >= 16) throw std::out_of_range("ICU vector index");
        return vector_low[irq] | (static_cast<u32>(vector_high[irq] & 3U) << 16);
    }
    void SetInterruptHandler(std::function<void(u32)> interrupt,
                             std::function<void(u32, bool)> vectored_interrupt) {
        on_interrupt = std::move(interrupt); on_vectored_interrupt = std::move(vectored_interrupt);
    }
    void Tick() override {
        Notify notify;
        {
            std::lock_guard lock(mutex);
            // GBATEK observes manual pending after two intervening NOPs. The
            // interpreter's tick is an approximation, not a verified pipeline.
            sampled_manual = manual_pipeline[1];
            manual_pipeline[1] = manual_pipeline[0]; manual_pipeline[0] = manual;
            notify = SampleLocked();
        }
        Deliver(notify);
    }
    u64 GetMaxSkip() const override {
        std::lock_guard lock(mutex);
        return (manual != manual_pipeline[0] || manual != manual_pipeline[1] ||
                manual != sampled_manual || InputLocked() != previous) ? 0 : Infinity;
    }
    void Skip(u64 ticks) override {
        // When no input or propagation is pending, skipping does not create IRQs.
        if (ticks && GetMaxSkip() == 0) throw std::logic_error("ICU skipped pending input");
    }
    std::array<u16,16> vector_low{}, vector_high{}, vector_context_switch{};
private:
    struct Notify { std::array<bool,3> core{}; u16 vectors{}; };
    u16 InputLocked() const {
        return static_cast<u16>(((hardware ^ polarity) | sampled_manual) & ~master_disable);
    }
    Notify SampleLocked() {
        const u16 now = InputLocked();
        request |= static_cast<u16>(now & ~previous);
        previous = now;
        return RoutesLocked();
    }
    Notify RoutesLocked() {
        Notify n;
        for (unsigned i=0;i<3;++i) {
            const u16 routed = request & enabled[i];
            n.core[i] = (routed & static_cast<u16>(~reported[i])) != 0;
            reported[i] = routed;
        }
        const u16 routed = request & vectored_enabled;
        n.vectors = routed & static_cast<u16>(~reported_vectored);
        reported_vectored = routed;
        return n;
    }
    void Deliver(const Notify& n) {
        // The pinned processor only has one pending vectored slot. Never silently
        // overwrite one simultaneously selected vector with another.
        if (n.vectors && (n.vectors & (n.vectors-1)))
            throw std::runtime_error("ICU simultaneous vectored arbitration unsupported");
        for (unsigned i=0;i<3;++i) if (n.core[i] && on_interrupt) on_interrupt(i);
        for (unsigned i=0;i<16;++i) if ((n.vectors & (1U<<i)) && on_vectored_interrupt)
            on_vectored_interrupt(GetVector(i),vector_context_switch[i]!=0);
    }
    std::function<void(u32)> on_interrupt;
    std::function<void(u32,bool)> on_vectored_interrupt;
    u16 request{}, hardware{}, manual{}, sampled_manual{}, previous{};
    u16 trigger_mode{}, polarity{}, master_disable{}, vectored_enabled{}, reported_vectored{};
    std::array<u16,2> manual_pipeline{};
    std::array<u16,3> enabled{}, reported{};
    mutable std::mutex mutex;
};
} // namespace Teakra
