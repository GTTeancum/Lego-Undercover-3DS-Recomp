#pragma once
#include "runtime/ctr_ipc.h"
#include "services/dsp1_image.h"
#include "services/dsp_special_config.h"
#include "services/dsp_execution_probe.h"

namespace lego::ctr {
// Discovery plus bounded, side-effect-free LoadComponent inspection. A staged
// image is HOST DIAGNOSTIC STATE ONLY: it is not mapped DSP SRAM or running firmware.
// Every DSP IPC still stops without a guest response. The exact image can feed a
// future backend; optional special-data staging still does not execute firmware.
class DspDiscoveryService final : public IpcService {
public:
    explicit DspDiscoveryService(DspSpecialConfig config = {}, DspProbeOptions probe = {})
        : config_(config), probe_options_(probe) {}
    bool CanHandle(const IpcCommandBuffer& q) const noexcept override {
        const auto bytes=q[1];
        return q[0]==IpcMakeHeader(0x11,3,2) && bytes>=kDsp1HeaderBytes &&
               bytes<=kDsp1MaxBytes && q[4]==((bytes<<4)|0xAU) &&
               (q[2]&0xFFFFU)==0xFFU && (q[3]&0xFFFFU)==0xFFU;
    }
    Result Handle(IpcRouter&,Kernel&,GuestMemory&,ThreadObject&,IpcCommandBuffer&) override;
    const DspExecutionProbe* execution_probe() const noexcept { return probe_.get(); }
    const Dsp1Image* inspected_image() const noexcept { return inspected_.get(); }
    const DspSpecialReceipt& special_receipt() const noexcept { return special_receipt_; }
private:
    const DspSpecialConfig config_;
    const DspProbeOptions probe_options_;
    std::unique_ptr<DspExecutionProbe> probe_;
    DspSpecialReceipt special_receipt_{};
    std::unique_ptr<Dsp1Image> inspected_;
};
} // namespace lego::ctr
