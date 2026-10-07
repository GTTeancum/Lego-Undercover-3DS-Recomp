#include "services/dsp_discovery_service.h"

#include <array>
#include <exception>

namespace lego::ctr {
Result DspDiscoveryService::ReadPipeIfPossible(IpcRouter& router, Kernel& kernel, GuestMemory& memory,
                                              ThreadObject& thread, IpcCommandBuffer& q) {
    // Pinned Azahar DSP_DSP::ReadPipeIfPossible returns all requested bytes or
    // zero, never a partial read. Its u16 size ignores the upper request half.
    const auto size = q[3] & 0xFFFFU;
    const auto response64 = std::uint64_t(thread.tls_address) + kIpcCommandBufferOffset;
    const auto table64 = std::uint64_t(thread.tls_address) + 0x180U;
    if (response64 + sizeof(q) > 0x100000000ULL || table64 + 8 > 0x100000000ULL)
        return kResultInvalidPointer;
    const auto response = static_cast<std::uint32_t>(response64);
    const auto table = static_cast<std::uint32_t>(table64);
    std::uint32_t descriptor{}, destination{};
    if (!memory.IsReadable(response, sizeof(q)) || !memory.IsWritable(response, sizeof(q)) ||
        !memory.Read32(table, &descriptor) || !memory.Read32(table + 4, &destination) ||
        (descriptor & 0x3FFFU) != 2U || (descriptor >> 14) < size ||
        (size && (std::uint64_t(destination) + size > 0x100000000ULL ||
                  !memory.IsWritable(destination, size))))
        return kResultInvalidPointer;

    // Direct-buffer host policy: protect reply and ALL static receive descriptors,
    // including aliases of their backing. Device/shared outputs are not supported.
    if (size && (memory.SpansAlias(destination, size, response, sizeof(q)) ||
                 memory.SpansAlias(destination, size, table, 0x80U))) {
        router.RequestHostStop("DSP pipe output aliases IPC or receive descriptors; request retained");
        return kResultSuccess;
    }

    DspPipeDescriptor pipe{};
    const auto inspection = live_->InspectPipe(4, pipe);
    if (inspection != DspPipeResult::Complete) {
        router.RequestHostStop("DSP audio pipe descriptor unavailable; request retained");
        return kResultSuccess;
    }
    const auto count = pipe.used >= size ? size : 0U;
    if (count) {
        try {
            // Reserve destination metadata before consuming bytes. This private
            // commit path allocates nothing after preflight, updates reservation
            // epochs, and cannot alias the live DSP bank or a service shared page.
            if (!memory.PrepareDeviceWrite(destination, count)) {
                router.RequestHostStop("DSP pipe output requires private writable backing");
                return kResultSuccess;
            }
        } catch (const std::exception&) {
            router.RequestHostStop("DSP pipe output preparation failed; pipe and request retained");
            return kResultSuccess;
        }
        std::array<std::uint8_t,128> output{};
        const auto result = live_->ReadPipe(2, std::span(output).first(count), true);
        if (result != DspPipeResult::Complete) {
            router.RequestHostStop(result == DspPipeResult::WouldBlock ?
                "DSP pipe read notification mailbox busy; request retained" :
                result == DspPipeResult::Fault ?
                "DSP pipe read fault; possible partial device effects retained" :
                "DSP pipe read invalid metadata or unknown payload; request retained");
            return kResultSuccess;
        }
        if (!memory.CommitDeviceWrite(destination, std::span(output).first(count))) {
            // Invariant failure is not a successful read or transaction rollback.
            router.RequestHostStop("DSP pipe destination commit invariant failed after consumption");
            return kResultSuccess;
        }
    }
    // Synchronous wait quanta may emit real notifications. Use the same retained
    // event delivery as scheduled execution; no signal is invented for the read.
    if(count && !DeliverPendingInterrupts(kernel)) {
        router.RequestHostStop("DSP pipe read event delivery failed after possible commit");
        return kResultSuccess;
    }
    q.fill(0);
    q[0] = IpcMakeHeader(0x10, 2, 2);
    q[2] = count;
    q[3] = (count << 14) | 2U;
    q[4] = destination;
    return kResultSuccess;
}
} // namespace lego::ctr
