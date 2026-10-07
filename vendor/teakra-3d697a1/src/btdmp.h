#pragma once
#include <array>
#include <cstdio>
#include <functional>
#include <utility>
#include <queue>
#include <stdexcept>
#include "common_types.h"
#include "core_timing.h"

namespace Teakra {

class Btdmp : public CoreTiming::Callbacks {
public:
    Btdmp(CoreTiming& core_timing);
    ~Btdmp();

    void Reset();

    // Explicit bounded reference profile, NOT a fully decoded hardware format.
    // GBATEK documents the IRQ nibble as zero=off/nonzero=on, but alternate
    // encodings are uncertain. Support ONLY the now-observed 0x000F/0x010F
    // pair. The FIFO-empty trigger threshold remains a pinned reference model.
    void SetReferenceTransmitProfile(bool enabled) {
        if (transmit_enable) throw std::logic_error("cannot replace active BTDMP profile");
        reference_transmit_profile = enabled;
    }
    void SetTransmitControl(u16 value) {
        if (!reference_transmit_profile) throw std::runtime_error("BTDMP transmit profile is unconfigured");
        if (value != 0x0005 && value != 0x000F && value != 0x010F)
            throw std::runtime_error("BTDMP unsupported transmit control mode");
        if (transmit_enable && value == 0x0005)
            throw std::runtime_error("BTDMP active transmit format change unsupported");
        transmit_control = value;
    }
    u16 GetTransmitControl() const { return transmit_control; }

    // Only the documented external-clock setup is supported. 4096 interpreter
    // ticks/sample remains the upstream diagnostic clock, not a measured divider.
    void SetTransmitClockConfig(u16 value) {
        if (value != 0 && value != 0x1004)
            throw std::runtime_error("BTDMP unsupported transmit clock");
        if (transmit_enable && value != transmit_clock_config)
            throw std::runtime_error("BTDMP active clock reconfiguration unsupported");
        transmit_clock_config = value;
    }

    // The five companion fields have documented read masks, but their full
    // hardware function is unknown. Support ONLY the documented legacy preset,
    // staged while disabled; do not pretend arbitrary formats/filters are modeled.
    void SetTransmitReferenceSetup(unsigned index, u16 value) {
        static constexpr std::array<u16,5> preset{4,0x21,0,0,0};
        static constexpr std::array<u16,5> masks{0x0FE7,3,0x1FFF,0x0FFF,0x3FFF};
        if (index >= preset.size()) throw std::out_of_range("BTDMP setup index");
        if (value != preset[index] || transmit_enable)
            throw std::runtime_error("BTDMP unsupported transmit setup (only disabled reference preset)");
        transmit_setup[index] = value & masks[index];
        transmit_setup_written |= 1U << index;
    }
    u16 GetTransmitReferenceSetup(unsigned index) const {
        if (index >= transmit_setup.size()) throw std::out_of_range("BTDMP setup index");
        return transmit_setup[index];
    }

    u16 GetTransmitClockConfig() const {
        return transmit_clock_config;
    }

    void SetTransmitPeriod(u16 value) {
        if (!value) throw std::invalid_argument("BTDMP transmit period must be nonzero");
        transmit_period = value;
    }

    u16 GetTransmitPeriod() const {
        return transmit_period;
    }

    void SetTransmitEnable(u16 value) {
        const u16 enable = value & 0x8000;
        if (enable && (!reference_transmit_profile || (transmit_control != 0x000F && transmit_control != 0x010F) ||
                       transmit_clock_config != 0x1004 || transmit_setup_written != 0x1F))
            throw std::runtime_error("BTDMP transmit requires complete reference stereo setup");
        transmit_enable = enable;
    }

    u16 GetTransmitEnable() const {
        return transmit_enable;
    }

    u16 GetTransmitEmpty() const {
        return transmit_empty;
    }

    u16 GetTransmitFull() const {
        return transmit_full;
    }

    void Send(u16 value) {
        if (transmit_queue.size() == 16) {
            throw std::runtime_error("BTDMP transmit FIFO overrun");
        } else {
            transmit_queue.push(value);
            transmit_empty = false;
            transmit_full = transmit_queue.size() == 16;
        }
    }

    void SetTransmitFlush(u16 value) {
        // GBATEK: bit 2 is the write-one flush command, bits 0..1 read/write.
        // Neither a zero write nor a read may discard queued samples or signal IRQ.
        transmit_fifo_control = value & 3;
        if (value & 4) {
            transmit_queue = {};
            transmit_empty = true;
            transmit_full = false;
        }
    }

    u16 GetTransmitFlush() const {
        return transmit_fifo_control;
    }

    void Tick() override;
    u64 GetMaxSkip() const override;
    void Skip(u64 ticks) override;

    void SetAudioCallback(std::function<void(std::array<std::int16_t, 2>)> callback) {
        audio_callback = [callback=std::move(callback)](auto samples, u8) { callback(samples); };
    }

    // Opt-in host reference policy; absent FIFO words become tagged silence,
    // never claimed firmware-written samples. The default still faults.
    void SetAudioCaptureCallback(std::function<void(std::array<std::int16_t,2>,u8)> callback,
                                 bool reference_silence) {
        audio_callback = std::move(callback);
        reference_underflow_silence = reference_silence;
    }
    void SetInterruptHandler(std::function<void()> handler) {
        interrupt_handler = std::move(handler);
    }

private:
    // TODO: figure out the relation between clock_config and period.
    // Reference period is 4096; this is not a measured title-specific clock divider.
    std::array<u16,5> transmit_setup{0,0,0x1FFF,0,0};
    unsigned transmit_setup_written = 0;
    u16 transmit_fifo_control = 0;
    u16 transmit_control = 0x0005; // GBATEK reference reset, not console dump.
    u16 transmit_clock_config = 0;
    u16 transmit_period = 4096;
    u16 transmit_timer = 0;
    u16 transmit_enable = 0;
    bool transmit_empty = true;
    bool transmit_full = false;
    std::queue<u16> transmit_queue;
    bool reference_underflow_silence = false;
    bool reference_transmit_profile = false;
    std::function<void(std::array<std::int16_t, 2>,u8)> audio_callback;
    std::function<void()> interrupt_handler;

    class BtdmpTimingCallbacks;
};

} // namespace Teakra
