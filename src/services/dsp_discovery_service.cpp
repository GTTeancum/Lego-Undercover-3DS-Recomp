#include "services/dsp_discovery_service.h"
#include <new>
#include <algorithm>

namespace lego::ctr {
Result DspDiscoveryService::Handle(IpcRouter& router,Kernel& kernel,GuestMemory& memory,
                                   ThreadObject& thread,IpcCommandBuffer& q) {
    if (!CanHandle(q)) return kResultNotFound;
    if(q[0]==IpcMakeHeader(0x13,2,2)) return FlushDataCache(router,kernel,memory,thread,q);
    if(q[0]==IpcMakeHeader(0xC,1,0)) {
        // Pinned ConvertProcessAddressFromDspDram: DATA-base + word-address*2.
        // This is an address translation, not initialization or a memory read.
        // Unknown DATA remains unknown and fails if/when the game dereferences it.
        const auto address=DspLiveDevice::DataAddress + q[1]*2U;
        if(live_->error() || probe_->summary().state==DspProbeState::Fault ||
           !live_->attached() || !memory.IsMapped(address,2)) {
            router.RequestHostStop("DSP address conversion requires a healthy mapped DATA bank");
            return kResultSuccess;
        }
        q.fill(0);q[0]=IpcMakeHeader(0xC,2,0);q[2]=address;return kResultSuccess;
    }
    if(q[0]==IpcMakeHeader(0x10,3,0)) return ReadPipeIfPossible(router,kernel,memory,thread,q);
    if(q[0]==IpcMakeHeader(0x7,1,0)) {
        if(!probe_->SetSemaphore(static_cast<std::uint16_t>(q[1]))) {
            router.RequestHostStop("DSP semaphore update failed; live target faulted or inactive");
            return kResultSuccess;
        }
        q.fill(0);q[0]=IpcMakeHeader(0x7,1,0);return kResultSuccess;
    }
    if(q[0]==IpcMakeHeader(0xD,2,2)) {
        const auto address=q[4];
        if (std::uint64_t(address)+4>0x100000000ULL || !memory.IsReadable(address,4))
            return kResultInvalidPointer;
        std::array<std::uint8_t,4> input{};
        for (unsigned i=0;i<4;++i) if(!memory.Read8(address+i,&input[i])) return kResultInvalidPointer;
        // Pinned DSP service normalizes audio-message bytes 2/3 in its local
        // translated buffer. Do not alter the caller's source bytes.
        input[2]=input[3]=0;
        const auto result=live_->WritePipe(2,input);
        if(result!=DspPipeResult::Complete) {
            router.RequestHostStop(result==DspPipeResult::WouldBlock ?
                "DSP audio startup pipe/mailbox would block; request retained" :
                result==DspPipeResult::Fault ? "DSP audio startup write failed; device may have partial effects" :
                "DSP audio startup descriptor is invalid; request retained");
            return kResultSuccess;
        }
        q.fill(0);q[0]=IpcMakeHeader(0xD,1,0);return kResultSuccess;
    }
    if(q[0]==IpcMakeHeader(0x17,1,0)) {
        // Pinned Pop<u16>: ignore upper half. This presets what a later event
        // signal sends; it does NOT directly signal either APBP direction.
        try {
            auto target=semaphore_;
            if (!target) { target=std::make_shared<SemaphoreTarget>();target->probe=probe_; }
            target->preset=static_cast<std::uint16_t>(q[1]);semaphore_=std::move(target);
            q.fill(0);q[0]=IpcMakeHeader(0x17,1,0);
        } catch (const std::bad_alloc&) {
            router.RequestHostStop("DSP semaphore preset allocation failed; request retained");
        }
        return kResultSuccess;
    }
    if(q[0]==IpcMakeHeader(0x16,0,0)) {
        // Lazy object creation avoids inventing a usable event for a failed load.
        // Repeated exports retain identity; first-export failure publishes nothing.
        try {
            auto target=semaphore_;
            auto event=semaphore_event_;
            if (!event) {
                if (!target) { target=std::make_shared<SemaphoreTarget>();target->probe=probe_; }
                event=std::make_shared<EventObject>(ResetType::OneShot,target);
            }
            ::lego::ctr::Handle handle=0;
            const auto result=kernel.handles().Create(&handle,event);
            if (result!=kResultSuccess) return result;
            semaphore_=std::move(target);semaphore_event_=std::move(event);
            q.fill(0);q[0]=IpcMakeHeader(0x16,1,2);q[2]=IpcCopyHandleDesc();q[3]=handle;
        } catch (const std::bad_alloc&) {
            router.RequestHostStop("DSP semaphore event allocation failed; request retained");
        }
        return kResultSuccess;
    }
    if(q[0]==IpcMakeHeader(0x15,2,2)) {
        const auto event=q[4]?std::dynamic_pointer_cast<EventObject>(kernel.handles().Get(q[4])):nullptr;
        if(q[4] && !event)return kResultInvalidHandle; // bounded host validation, not null-unregister
        if(event && std::count_if(interrupts_.begin(),interrupts_.end(),[](const auto& e){return bool(e);})>=6) {
            router.RequestHostStop("DSP interrupt registration capacity reached; request retained");
            return kResultSuccess;
        }
        const auto index=q[1]<2?q[1]:2+q[2];interrupts_[index]=event;
        q.fill(0);q[0]=IpcMakeHeader(0x15,1,0);q[1]=0;return kResultSuccess;
    }
    if(live_) {
        router.RequestHostStop("DSP reload while live is unsupported; current device retained");
        return kResultSuccess;
    }
    const auto bytes=q[1], address=q[5];
    if (std::uint64_t(address)+bytes>0x100000000ULL || !memory.IsReadable(address,bytes)) {
        router.RequestHostStop("DSP component input is unreadable; LoadComponent remains pending");
        return kResultSuccess;
    }
    try {
        std::vector<std::uint8_t> copy(bytes);
        for (std::uint32_t i=0;i<bytes;++i) {
            if (!memory.Read8(address+i,&copy[i])) {
                router.RequestHostStop("DSP component input changed during inspection");
                return kResultSuccess;
            }
        }
        std::unique_ptr<Dsp1Image> candidate;
        const auto result=StageDsp1Image(copy,candidate);
        if (!result) {
            router.RequestHostStop(Dsp1ErrorName(result.error));
            return kResultSuccess;
        }
        DspSpecialReceipt receipt;
        if (candidate->special.required && config_.profile()!=DspSpecialProfile::Unconfigured) {
            const auto special=StageDspSpecial(config_,*candidate,receipt);
            if (special!=DspSpecialError::None) {
                router.RequestHostStop(DspSpecialErrorName(special));
                return kResultSuccess;
            }
        }
        // Reinspection replaces only the diagnostic snapshot after full validation.
        // It never resets hardware, writes guest memory or changes events/timing.
        inspected_=std::move(candidate);
        special_receipt_=receipt;
        if (probe_options_.enabled) {
            const char* error=nullptr;
            auto probe=DspExecutionProbe::Create(*inspected_,probe_options_.reset,error,probe_options_.capture_audio,probe_options_.reference_audio_silence,probe_options_.reference_transmit,probe_options_.boot_mode,probe_options_.audio_sink);
            if (!probe) {
                router.RequestHostStop(error);return kResultSuccess;
            }
            probe->Advance(probe_options_.steps);
            probe_=std::move(probe); // Retain boot state; only the explicit live path below publishes it.
            const auto& s=probe_->summary();
            if(probe_options_.live && s.state==DspProbeState::ProtocolComplete) {
                const char* error=nullptr;
                auto device=DspLiveDevice::Prepare(probe_,kernel.now_ns(),error);
                if(!device){router.RequestHostStop(error);return kResultSuccess;}
                if(!device->Attach(memory)){
                    router.RequestHostStop("DSP data mapping unavailable; load remains pending");return kResultSuccess;
                }
                // No fallible allocation follows the map. Existing router response
                // preflight protects the reply before boot or memory publication.
                live_=std::move(device);
                const auto descriptor=q[4],pointer=q[5];q.fill(0);
                q[0]=IpcMakeHeader(0x11,2,2);q[1]=0;q[2]=1;q[3]=descriptor;q[4]=pointer;
                return kResultSuccess;
            }
            router.RequestHostStop(s.state==DspProbeState::Fault ? s.error.data() :
                s.state==DspProbeState::ProtocolComplete ?
                "DSP probe handshake complete; live execution and pipes still unimplemented" :
                "DSP execution probe reached its bounded step limit; load remains pending");
            return kResultSuccess;
        }
        router.RequestHostStop(special_receipt_.source!=DspSpecialSource::None ?
            "DSP1 ordinary and special bytes staged only: firmware executor and boot handshake unimplemented" :
            inspected_->special.required ?
            "DSP1 image verified/staged only: special-segment data and firmware boot handshake unresolved" :
            "DSP1 image verified/staged only: firmware boot handshake unimplemented");
    } catch (const std::bad_alloc&) {
        router.RequestHostStop("host allocation failed during DSP inspection");
    }
    return kResultSuccess; // Failed/incomplete load paths stop before committing a guest reply.
}
} // namespace lego::ctr
