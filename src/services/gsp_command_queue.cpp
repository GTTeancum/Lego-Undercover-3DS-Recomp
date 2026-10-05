#include "services/gsp_gpu_service.h"
#include <atomic>
#include <sstream>
#include <vector>

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
    std::array<std::unique_ptr<PicaListPlan>,kPacketCount> plans{};
    std::array<std::unique_ptr<DisplayTransferPlan>,kPacketCount> transfers{};
    std::array<PicaGpuRegisters,kPacketCount> transfer_registers{};
    auto staged_registers=shared_->register_words;
    const auto* staged_uploads=&shared_->pica_uploads;
    std::uint32_t total_irqs=0;
    std::uint32_t eligible = 0;
    if (count != 0 && should_stop == 0 && status != kStatusStopped) {
        for (std::uint32_t n = 0; n < count; ++n) {
            const auto packet = base + kPacketsOffset + ((index+n) % kPacketCount) * kPacketSize;
            const auto packet_header = ReadWord(bytes, packet);
            const auto kind=packet_header&0xFFU;
            if (kind==1) {
                const auto address=ReadWord(bytes,packet+4),size=ReadWord(bytes,packet+8);
                // Supported original linear-heap slice, not arbitrary GPU physical
                // memory. Both address and size must preserve the reference's /8
                // register encoding; do not round a malformed input into another list.
                if(address<0x14000000U || (address&7U) || (size&7U) ||
                   size>kPicaMaxListBytes || std::uint64_t(address)+size>0x1C000000ULL ||
                   (size&&!memory.IsReadable(address,size)))
                    return stop("GSP SubmitCmdList outside supported readable linear-heap bounds");
                if(ReadWord(bytes,packet+12)!=0 || ReadWord(bytes,packet+28)!=0)
                    return stop("GSP SubmitCmdList flags/do_flush are unsupported");
                // A command list must not alias state changed by dequeue, IRQ or
                // the IPC reply. The fully staged snapshot then stays authoritative.
                if(size&&(memory.UsesSharedBacking(address,size,*shared_->memory) ||
                         memory.SpansAlias(address,size,static_cast<std::uint32_t>(response),sizeof(command))))
                    return stop("GSP SubmitCmdList source aliases mutable queue or IPC state");
                // A previous staged transfer must not change this list between
                // preflight and execution. Dependency forwarding is not implemented.
                for (std::uint32_t prior=0;prior<n;++prior) {
                    if (transfers[prior] && memory.SpansAlias(address,size,
                            transfers[prior]->request.output,transfers[prior]->bytes))
                        return stop("GSP SubmitCmdList depends on an earlier staged transfer");
                }
                try {
                    std::vector<std::uint8_t> list(size);
                    for(std::uint32_t i=0;i<size;++i) {
                        if(!memory.Read8(address+i,&list[i]))
                            return stop("GSP SubmitCmdList snapshot read failed");
                    }
                    auto plan=std::make_unique<PicaListPlan>();
                    // GPU::Execute configures channel 0 before ProcessCmdList.
                    auto registers=staged_registers;
                    registers[0x638]=size/8;
                    registers[0x63A]=((address-0x14000000U+0x20000000U)>>3)&0x0FFFFFFFU;
                    registers[0x63C]=1;
                    if(!StagePicaStartupList(list,registers,
                            *staged_uploads,*plan)) {
                        std::ostringstream error;
                        error<<plan->result.error<<" at list byte 0x"<<std::hex
                             <<plan->result.byte_offset<<" register 0x"<<plan->result.register_id;
                        router.RequestHostStop(error.str());return kResultSuccess;
                    }
                    // GPU::SubmitCmdList clears channel 0 trigger after execution.
                    plan->registers[0x63C]=0;
                    total_irqs+=plan->result.irqs;
                    staged_registers=plan->registers;
                    staged_uploads=&plan->uploads; plans[n]=std::move(plan);
                } catch(const std::exception& error) {
                    router.RequestHostStop(error.what());return kResultSuccess;
                }
            } else if(kind==3) {
                try {
                    const DisplayTransferRequest request{ReadWord(bytes,packet+4),ReadWord(bytes,packet+8),
                        ReadWord(bytes,packet+12),ReadWord(bytes,packet+16),ReadWord(bytes,packet+20)};
                    auto plan=std::make_unique<DisplayTransferPlan>();
                    const char* error=nullptr;
                    if (!StageDisplayTransfer(request,shared_->vram.get(),*plan,error)) return stop(error);
                    if (!memory.IsWritable(request.output,plan->bytes))
                        return stop("DisplayTransfer destination lacks complete writable backing");
                    if (memory.UsesSharedBacking(request.output,plan->bytes,*shared_->memory) ||
                        memory.SpansAlias(request.output,plan->bytes,
                                          static_cast<std::uint32_t>(response),sizeof(command)))
                        return stop("DisplayTransfer destination aliases mutable queue or IPC state");
                    // GPU::Execute sets the transfer registers. The equal RGBA4
                    // path performs actual detiling before clearing trigger bit 0.
                    staged_registers[0x300]=(request.input-kGpuVramVirtualBase+kGpuVramPhysicalBase)>>3;
                    staged_registers[0x301]=(request.output-0x14000000U+0x20000000U)>>3;
                    staged_registers[0x302]=request.output_size;
                    staged_registers[0x303]=request.input_size;
                    staged_registers[0x304]=request.flags;
                    staged_registers[0x306]&=~1U;
                    transfer_registers[n]=staged_registers;
                    ++total_irqs; transfers[n]=std::move(plan);
                } catch (const std::exception& error) {
                    router.RequestHostStop(error.what()); return kResultSuccess;
                }
            } else if(kind==kCacheFlush) {
                for (std::uint32_t region = 0; region < 3; ++region) {
                    const auto address = ReadWord(bytes, packet+4+region*8);
                    const auto size = ReadWord(bytes, packet+8+region*8);
                    if (size && (std::uint64_t(address)+size > 0x100000000ULL ||
                                 !memory.IsReadable(address,size)))
                        return stop("GSP CacheFlush region is not a supported readable span");
                }
            } else {
                return stop("GSP queue packet requires unimplemented GPU execution");
            }
            stop_after[n] = ((packet_header >> 16) & 0xFFU) != 0;
            ++eligible;
            if (stop_after[n]) break;
        }
    }

    // Validate the entire IRQ batch before committing any queue/register state.
    // Full/invalid relay rings stop here; no guessed overflow/recovery behavior.
    const auto relay=owner->slot*0x40U;
    if(total_irqs) {
        if(!owner->registered || !owner->event)
            return stop("GSP IRQ requires a registered real GSP event");
        const auto irq_index=bytes[relay],irq_count=bytes[relay+1];
        if(irq_index>=0x34 || irq_count>0x34 || bytes[relay+2]!=0 ||
           total_irqs>0x34U-irq_count)
            return stop("GSP IRQ relay has invalid/full/failed state");
    }
    // Allocation for private-memory reservation metadata is done before dequeue.
    // Preparing changes neither bytes nor tokens; the later commit is allocation-free.
    try {
        for (std::uint32_t n=0;n<eligible;++n) {
            if (transfers[n] && !memory.PrepareDeviceWrite(transfers[n]->request.output,transfers[n]->bytes))
                return stop("DisplayTransfer destination is not supported private device-write backing");
        }
    } catch (const std::exception& error) {
        router.RequestHostStop(error.what());return kResultSuccess;
    }
    const auto publish_irq=[&](std::uint8_t interrupt_id) {
        const auto current=shared_->memory->bytes();
        const auto irq_index=current[relay],irq_count=current[relay+1];
        const auto next=(std::uint32_t(irq_index)+irq_count)%0x34U;
        const std::array<std::uint8_t,1> count_byte{static_cast<std::uint8_t>(irq_count+1)},id{interrupt_id};
        (void)shared_->memory->Write(relay+1,count_byte);
        (void)shared_->memory->Write(relay+12+next,id);
        // An executed PICA irq_request justifies P3D; a completed byte transfer justifies PPF.
        // Explicit synchronous host policy: no simulated GPU delay or vblank.
        kernel.SignalEventObject(*owner->event);
    };

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
            if(plans[n]) {
                shared_->register_words=plans[n]->registers;
                shared_->pica_uploads=plans[n]->uploads;
                shared_->last_pica_result=plans[n]->result;
                for(std::uint32_t i=0;i<plans[n]->result.irqs;++i)publish_irq(5);
            }
            if (transfers[n]) {
                shared_->register_words=transfer_registers[n];
                shared_->register_words[0x306]|=1U;
                if (!memory.CommitDeviceWrite(transfers[n]->request.output,transfers[n]->output))
                    return stop("DisplayTransfer internal commit invariant failed after dequeue; no PPF delivered");
                shared_->register_words[0x306]&=~1U;
                publish_irq(4); // Real bytes are committed first. No vblank or frame is implied.
            }
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
