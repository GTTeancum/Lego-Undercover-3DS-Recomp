#pragma once
#include "runtime/ctr_ipc.h"
#include "services/dsp1_image.h"

namespace lego::ctr {
// Discovery plus bounded, side-effect-free LoadComponent inspection. A staged
// image is HOST DIAGNOSTIC STATE ONLY: it is not mapped DSP SRAM or running firmware.
// Every DSP IPC still stops without a guest response. The exact image can feed a
// future backend, but unresolved special data and firmware boot cannot be skipped.
class DspDiscoveryService final : public IpcService {
public:
    bool CanHandle(const IpcCommandBuffer& q) const noexcept override {
        const auto bytes=q[1];
        return q[0]==IpcMakeHeader(0x11,3,2) && bytes>=kDsp1HeaderBytes &&
               bytes<=kDsp1MaxBytes && q[4]==((bytes<<4)|0xAU) &&
               (q[2]&0xFFFFU)==0xFFU && (q[3]&0xFFFFU)==0xFFU;
    }
    Result Handle(IpcRouter&,Kernel&,GuestMemory&,ThreadObject&,IpcCommandBuffer&) override;
    const Dsp1Image* inspected_image() const noexcept { return inspected_.get(); }
private:
    std::unique_ptr<Dsp1Image> inspected_;
};
} // namespace lego::ctr
