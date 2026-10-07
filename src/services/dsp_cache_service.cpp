#include "services/dsp_discovery_service.h"

namespace lego::ctr {
Result DspDiscoveryService::FlushDataCache(IpcRouter& router, Kernel& kernel,
                                          GuestMemory& memory, ThreadObject& thread,
                                          IpcCommandBuffer& command) {
    // Pinned DSP service: two u32 parameters and one copied Process object.
    // The kernel resolves its current-process pseudo-handle or an actual copy.
    const auto process = std::dynamic_pointer_cast<ProcessObject>(
        kernel.handles().Get(command[4]));
    if (!process) return kResultInvalidHandle;
    if (process != kernel.current_process()) {
        router.RequestHostStop("DSP cache operation for another process is unsupported");
        return kResultSuccess;
    }
    if (!live_ || !probe_ || live_error() || !live_->attached() ||
        probe_->summary().state == DspProbeState::Fault) {
        router.RequestHostStop("DSP cache operation requires a healthy attached live device");
        return kResultSuccess;
    }
    // Normal router preflight covers reply permissions. Keep this bounded path
    // private too, so even the reply cannot invoke device/shared-memory callbacks.
    const auto reply = std::uint64_t(thread.tls_address) + kIpcCommandBufferOffset;
    if (reply + sizeof(command) > 0x100000000ULL ||
        !memory.IsCachelessPrivateRange(static_cast<std::uint32_t>(reply), sizeof(command)) ||
        !memory.IsWritable(static_cast<std::uint32_t>(reply), sizeof(command))) {
        return kResultInvalidPointer;
    }
    const auto address = command[1], size = command[2];
    if (size && !memory.IsCachelessPrivateRange(address, size)) {
        router.RequestHostStop("DSP cache range is not one readable cacheless private span");
        return kResultSuccess;
    }
    // GuestMemory writes immediately reach the same private backing seen through
    // every alias. There are no dirty CPU cache lines to copy, invalidate or drain.
    // No target read/write, epoch change, fence, DSP execution, time advance,
    // interrupt, mapping or external AHB/FCRAM transaction is implied here.
    // Zero length is an explicit no-work case after the process/device checks.
    // Nonzero cross-region, protected, shared and device spans remain unsupported.
    command.fill(0);
    command[0] = IpcMakeHeader(0x13, 1, 0);
    return kResultSuccess;
}
} // namespace lego::ctr
