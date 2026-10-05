#include "services/gsp_gpu_service.h"
#include <atomic>

namespace lego::ctr {
namespace {
constexpr std::uint32_t kQueueBase = 0x800;
constexpr std::uint32_t kQueueStride = 0x200;
constexpr std::uint32_t kPacketsOffset = 0x20;
constexpr std::uint32_t kPacketSize = 0x20;
constexpr std::uint32_t kPacketCount = 15;
constexpr std::uint32_t kStatusStopped = 1;
constexpr std::uint32_t kCacheFlush = 5;

std::uint32_t ReadWord(std::span<const std::uint8_t> data, std::uint32_t offset) noexcept {
    return std::uint32_t(data[offset]) | (std::uint32_t(data[offset+1]) << 8) |
           (std::uint32_t(data[offset+2]) << 16) | (std::uint32_t(data[offset+3]) << 24);
}

void WriteHeader(ServiceSharedMemoryObject& object, std::uint32_t offset,
                 std::uint32_t value) noexcept {
    // Queue bounds are checked before commit; these four bytes always fit.
    const std::array<std::uint8_t,4> bytes{
        static_cast<std::uint8_t>(value), static_cast<std::uint8_t>(value >> 8),
        static_cast<std::uint8_t>(value >> 16), static_cast<std::uint8_t>(value >> 24)};
    (void)object.Write(offset, bytes); // Also invalidates the backing reservation granule.
}
} // namespace

Result GspGpuService::TriggerCommandQueue(IpcRouter& router, Kernel& kernel,
                                        GuestMemory& memory, ThreadObject& thread,
                                        IpcCommandBuffer& command) {
    const auto stop = [&](const char* message) {
        router.RequestHostStop(message);
        return kResultSuccess; // Router preserves the guest request and CPU on a host stop.
    };
    const auto owner = shared_->owner.lock();
    if (!owner || owner->slot >= kGspRelaySlots ||
        owner->process_id != kernel.current_process()->process_id)
        return stop("GSP queue without a supported current-process owner");

    // The reference selects the GPU-rights OWNER's queue, not the requester slot.
    // A reply residing anywhere in this GSP page could corrupt queue/relay state.
    // Reject that alias, including an alternate VA, before any shared mutation.
    const auto response = std::uint64_t(thread.tls_address) + kIpcCommandBufferOffset;
    if (response > 0xFFFFFFFFULL || memory.UsesSharedBacking(
            static_cast<std::uint32_t>(response), sizeof(command), *shared_->memory))
        return stop("GSP queue IPC response aliases the GSP shared page");

    const auto base = kQueueBase + owner->slot * kQueueStride;
    const auto bytes = shared_->memory->bytes();
    const auto original = ReadWord(bytes, base);
    const auto index = original & 0xFFU;
    const auto count = (original >> 8) & 0xFFU;
    const auto status = (original >> 16) & 0xFFU;
    const auto should_stop = original >> 24;
    if (index >= kPacketCount || count > kPacketCount)
        return stop("GSP queue index/count exceeds the 15-packet ring");
    // CMD_FAILED/unknown statuses are not guessed. Pinned iteration only tests
    // equality with STOPPED, despite the more general flags comment in its header.
    if (status != 0 && status != kStatusStopped)
        return stop("GSP queue failed/unknown status is unimplemented");

    // Diagnostic safety policy: preflight the whole eligible batch before any
    // progress. An unsupported later packet leaves even a valid prefix pending.
    // Packets behind a stop marker are not eligible and are not inspected.
    std::array<bool,kPacketCount> stop_after{};
    std::uint32_t eligible = 0;
    if (count != 0 && should_stop == 0 && status != kStatusStopped) {
        for (std::uint32_t n = 0; n < count; ++n) {
            const auto packet = base + kPacketsOffset + ((index+n) % kPacketCount) * kPacketSize;
            const auto packet_header = ReadWord(bytes, packet);
            if ((packet_header & 0xFFU) != kCacheFlush)
                return stop("GSP queue packet requires unimplemented GPU execution");
            // CacheFlush has three (VA, size) pairs. Zero-length pairs need no
            // pointer. Readable, single-region spans are this host's supported
            // validation slice, not an assertion about firmware error behavior.
            for (std::uint32_t region = 0; region < 3; ++region) {
                const auto address = ReadWord(bytes, packet+4+region*8);
                const auto size = ReadWord(bytes, packet+8+region*8);
                if (size && (std::uint64_t(address)+size > 0x100000000ULL ||
                             !memory.IsReadable(address,size)))
                    return stop("GSP CacheFlush region is not a supported readable span");
            }
            stop_after[n] = ((packet_header >> 16) & 0xFFU) != 0;
            ++eligible;
            if (stop_after[n]) break;
        }
    }

    std::uint32_t header = original;
    if (count && should_stop) {
        header = (header & ~0x00FF0000U) | (kStatusStopped << 16);
        if (header != original) WriteHeader(*shared_->memory,base,header);
    } else {
        for (std::uint32_t n = 0; n < eligible; ++n) {
            const auto next = ((header & 0xFFU)+1) % kPacketCount;
            const auto remaining = ((header >> 8) & 0xFFU)-1;
            header = (header & 0xFFFF0000U) | next | (remaining << 8);
            // Match the inspected ordering: dequeue before executing the packet.
            WriteHeader(*shared_->memory,base,header);
            // There is no split CPU/GPU cache or asynchronous GPU in this host.
            // Earlier synchronous writes already target authoritative backing.
            // A conservative native fence provides ordering; it is NOT a GPU
            // transfer, device-cache simulation, interrupt, wakeup or time advance.
            // The pinned GPU::Execute CacheFlush case performs no extra action.
            std::atomic_thread_fence(std::memory_order_seq_cst);
            if (stop_after[n]) {
                header = (header & ~0x00FF0000U) | (kStatusStopped << 16);
                WriteHeader(*shared_->memory,base,header);
            }
        }
    }
    command.fill(0);
    command[0] = IpcMakeHeader(0x000C,1,0);
    command[1] = kResultSuccess;
    return kResultSuccess;
}
} // namespace lego::ctr
