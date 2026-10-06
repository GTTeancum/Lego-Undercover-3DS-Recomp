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
        // Reinspection replaces only the diagnostic snapshot after full validation.
        // It never resets hardware, writes guest memory or changes events/timing.
        inspected_=std::move(candidate);
        router.RequestHostStop(inspected_->special.required ?
            "DSP1 image verified/staged only: special-segment data and firmware boot handshake unresolved" :
            "DSP1 image verified/staged only: firmware boot handshake unimplemented");
    } catch (const std::bad_alloc&) {
        router.RequestHostStop("host allocation failed during DSP inspection");
    }
    return kResultSuccess; // Router stops BEFORE committing any guest reply.
}
} // namespace lego::ctr
