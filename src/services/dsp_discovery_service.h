#pragma once
#include "runtime/ctr_ipc.h"
#include "services/dsp1_image.h"
#include "services/dsp_special_config.h"
#include "services/dsp_execution_probe.h"
#include "services/dsp_live_device.h"

namespace lego::ctr {
// Default: diagnostic inspection/probe only, preserving the guest load request.
// Explicit live mode boots real firmware, validates its pipe table, maps retained
// DATA storage and schedules bounded continuation before replying to LoadComponent.
// Registration retains real events; other service operations still stop untouched.
class DspDiscoveryService final : public IpcService {
public:
    explicit DspDiscoveryService(DspSpecialConfig config = {}, DspProbeOptions probe = {})
        : config_(config), probe_options_(probe) {}
    bool CanHandle(const IpcCommandBuffer& q) const noexcept override {
        if(probe_options_.live && live_ && (q[0]==IpcMakeHeader(0x16,0,0) || q[0]==IpcMakeHeader(0x17,1,0) || q[0]==IpcMakeHeader(0x7,1,0))) return true;
        if(probe_options_.live && live_ && q[0]==IpcMakeHeader(0x15,2,2))
            return q[1]<3 && q[2]<8 && q[3]==IpcCopyHandleDesc();
        // Observed audio-startup message only; larger payloads/other pipes need
        // separately validated IPC and blocking behavior.
        if(probe_options_.live && live_ && q[0]==IpcMakeHeader(0xD,2,2))
            return q[1]==2 && q[2]==4 && q[3]==0x10402;
        // Bounded audio-pipe reads. Size is Pop<u16> in the pinned service;
        // peer0 is the observed DSP-to-CPU direction. No other pipe/peer is enabled.
        if(probe_options_.live && live_ && q[0]==IpcMakeHeader(0x10,3,0))
            return q[1]==2 && q[2]==0 && (q[3]&0xFFFFU)<=128;
        if(probe_options_.live && live_ && q[0]==IpcMakeHeader(0xC,1,0))
            return q[1]<0x20000; // Bounded word address within the mapped DATA bank.
        const auto bytes=q[1];
        return q[0]==IpcMakeHeader(0x11,3,2) && bytes>=kDsp1HeaderBytes &&
               bytes<=kDsp1MaxBytes && q[4]==((bytes<<4)|0xAU) &&
               (q[2]&0xFFFFU)==0xFFU && (q[3]&0xFFFFU)==0xFFU;
    }
    Result Handle(IpcRouter&,Kernel&,GuestMemory&,ThreadObject&,IpcCommandBuffer&) override;
    const DspLiveDevice* live_device() const noexcept { return live_.get(); }
    std::optional<std::uint64_t> next_deadline_ns() const noexcept {
        return live_?live_->next_deadline_ns():std::nullopt;
    }
    bool RunScheduled(Kernel& kernel,std::uint64_t now) noexcept {
        if(signal_error_ || !live_ || !live_->RunScheduled(now))return false;
        return DeliverPendingInterrupts(kernel);
    }
    bool DeliverPendingInterrupts(Kernel& kernel) noexcept {
        if(signal_error_ || !probe_)return false;
        const auto pending=probe_->TakeLiveInterrupts();
        for(unsigned i=0;i<interrupts_.size();++i)
            if((pending&(1U<<i)) && interrupts_[i] && !kernel.SignalEventObject(*interrupts_[i])) {
                signal_error_=kernel.event_signal_error();return false;
            }
        return true;
    }
    std::shared_ptr<EventObject> interrupt_event(unsigned index) const noexcept {
        return index<interrupts_.size()?interrupts_[index]:nullptr;
    }
    const char* live_error() const noexcept {return signal_error_?signal_error_:(live_?live_->error():nullptr);}
    std::shared_ptr<EventObject> semaphore_event() const noexcept { return semaphore_event_; }
    std::uint16_t preset_semaphore() const noexcept { return semaphore_ ? semaphore_->preset : 0; }
    const DspExecutionProbe* execution_probe() const noexcept { return probe_.get(); }
    const Dsp1Image* inspected_image() const noexcept { return inspected_.get(); }
    const DspSpecialReceipt& special_receipt() const noexcept { return special_receipt_; }
private:
    Result ReadPipeIfPossible(IpcRouter&, Kernel&, GuestMemory&, ThreadObject&, IpcCommandBuffer&);
    struct SemaphoreTarget final : EventSignalTarget {
        std::weak_ptr<DspExecutionProbe> probe;
        std::uint16_t preset{};
        const char* OnSignal() noexcept override {
            auto p=probe.lock();
            if (!p) return "DSP semaphore event target expired; signal not delivered";
            if (!p->SetSemaphore(preset)) return "DSP semaphore target is faulted or inactive; signal not delivered";
            return nullptr;
        }
    };
    std::shared_ptr<SemaphoreTarget> semaphore_;
    std::shared_ptr<EventObject> semaphore_event_;
    const char* signal_error_{};
    std::array<std::shared_ptr<EventObject>,10> interrupts_{};
    const DspSpecialConfig config_;
    const DspProbeOptions probe_options_;
    std::shared_ptr<DspExecutionProbe> probe_;
    std::unique_ptr<DspLiveDevice> live_;
    DspSpecialReceipt special_receipt_{};
    std::unique_ptr<Dsp1Image> inspected_;
};
} // namespace lego::ctr
