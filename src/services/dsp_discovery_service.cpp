#include "services/dsp_discovery_service.h"
#include <new>

namespace lego::ctr {
Result DspDiscoveryService::Handle(IpcRouter& router,Kernel&,GuestMemory& memory,
                                   ThreadObject&,IpcCommandBuffer& q) {
    if (!CanHandle(q)) return kResultNotFound;
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
            auto probe=DspExecutionProbe::Create(*inspected_,probe_options_.reset,error);
            if (!probe) {
                router.RequestHostStop(error);return kResultSuccess;
            }
            probe->Advance(probe_options_.steps);
            probe_=std::move(probe); // HOST-ONLY diagnostics, never a loaded device.
            const auto& s=probe_->summary();
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
    return kResultSuccess; // Router stops BEFORE committing any guest reply.
}
} // namespace lego::ctr
