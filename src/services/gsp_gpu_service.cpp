#include "services/gsp_gpu_service.h"

namespace lego::ctr {
namespace {
constexpr Result kRegsAddress = 0xE0E02A01U;
constexpr Result kRegsSize = 0xE0E02BECU;
constexpr Result kRegsAlignment = 0xE0E02BF2U;
}
Result GspGpuService::WriteHwRegisters(IpcRouter& router,GuestMemory& memory,
                                      ThreadObject& thread,IpcCommandBuffer& command) {
    const auto id=IpcCommandId(command[0]);
    const auto offset=command[1], size=command[2];
    const bool masked=id==0x0002;
    // Validation order matches pinned GSP WriteHWRegs[WithMask]. No source read
    // or register mutation is needed to return these range/size/alignment errors.
    Result result=kResultSuccess;
    if ((offset&3U)!=0 || offset>=0x00420000U) result=kRegsAddress;
    else if (size>0x80U) result=kRegsSize;
    else if ((size&3U)!=0) result=kRegsAlignment;
    if (result==kResultSuccess && size!=0) {
        // Complete input validation precedes reads and any state change. Guest
        // execution is single-host-threaded; no concurrent guest mutation occurs.
        const auto response=std::uint64_t(thread.tls_address)+kIpcCommandBufferOffset;
        for(unsigned n=0;n<(masked?2U:1U);++n) {
            const auto source=command[4+n*2];
            if (std::uint64_t(source)+size>0x100000000ULL || !memory.IsReadable(source,size))
                return kResultInvalidPointer;
            if (response>0xFFFFFFFFULL || memory.SpansAlias(source,size,
                    static_cast<std::uint32_t>(response),sizeof(command))) {
                router.RequestHostStop("GSP register input aliases its IPC response");
                return kResultSuccess;
            }
        }
        // Preflight the COMPLETE batch before committing any register. GPU::WriteReg
        // is not the PICA command-list processor: direct MMIO has only five action
        // trigger cases. Enabled triggers and other MMIO banks remain host stops.
        std::array<std::uint32_t,0x80/4> values{};
        std::array<std::size_t,0x80/4> slots{};
        for (std::uint32_t index=0;index<size/4;++index) {
            const auto slot=RegisterSlot(offset+index*4);
            if (!slot) {
                router.RequestHostStop("GSP MMIO range has unimplemented register semantics");
                return kResultSuccess;
            }
            slots[index]=*slot;
            if (!memory.Read32(command[4]+index*4,&values[index])) return kResultInvalidPointer;
            if (masked) {
                const auto old=shared_->register_words[*slot];
                std::uint32_t mask=0;
                if (!memory.Read32(command[6]+index*4,&mask)) return kResultInvalidPointer;
                values[index]=(old&~mask)|(values[index]&mask);
            }
            if (TriggersAction(offset+index*4,values[index])) {
                router.RequestHostStop("GSP MMIO write would trigger unimplemented GPU work");
                return kResultSuccess;
            }
        }
        for (std::uint32_t index=0;index<size/4;++index)
            shared_->register_words[slots[index]]=values[index];
    }
    command.fill(0);command[0]=IpcMakeHeader(id,1,0);command[1]=result;
    return kResultSuccess;
}
// The pinned GSP command uses the same process/address/size shape as DSP's
// cache operation. Only this cacheless private-memory model is implemented;
// flushing does not submit GPU work, expose VRAM or complete a frame.
Result GspGpuService::FlushDataCache(IpcRouter& router, Kernel& kernel,
                                     GuestMemory& memory, ThreadObject& thread,
                                     IpcCommandBuffer& command) {
    const auto process=std::dynamic_pointer_cast<ProcessObject>(kernel.handles().Get(command[4]));
    if(!process)return kResultInvalidHandle;
    if(process!=kernel.current_process()) {
        router.RequestHostStop("GSP cache operation for another process is unsupported");
        return kResultSuccess;
    }
    const auto reply=std::uint64_t(thread.tls_address)+kIpcCommandBufferOffset;
    if(reply+sizeof(command)>0x100000000ULL ||
       !memory.IsCachelessPrivateRange(static_cast<std::uint32_t>(reply),sizeof(command)) ||
       !memory.IsWritable(static_cast<std::uint32_t>(reply),sizeof(command)))
        return kResultInvalidPointer;
    if(command[2] && !memory.IsCachelessPrivateRange(command[1],command[2])) {
        router.RequestHostStop("GSP cache range is not one readable cacheless private span");
        return kResultSuccess;
    }
    // All prior private writes already update their shared backing. No cache-line
    // rounding, byte movement, reservation invalidation, ownership change, GPU
    // notification or time advance is needed. Zero length performs no range work.
    command.fill(0);command[0]=IpcMakeHeader(0x0008,1,0);
    return kResultSuccess;
}
} // namespace lego::ctr
