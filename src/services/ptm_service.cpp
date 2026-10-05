#include "services/ptm_service.h"

namespace lego::ctr {
bool PtmService::CanHandle(const IpcCommandBuffer& command) const noexcept {
    if (mode_ != PtmStepMode::EmptyHistory) return false;
    if (command[0] == IpcMakeHeader(0xC, 0, 0)) return true;
    if (command[0] != IpcMakeHeader(0xB, 3, 2) || command[1] > kMaxPtmHistoryHours)
        return false;
    // Observed write-only mapped buffer, one little-endian u16 per hour.
    // No static-buffer interpretation, implicit permission upgrade or allocation
    // proportional to an unchecked guest size. Other descriptor rights stop.
    return command[4] == ((command[1] * 2U) << 4U | 0xCU);
}

Result PtmService::Handle(IpcRouter& router, Kernel&, GuestMemory& memory, ThreadObject& thread,
                           IpcCommandBuffer& command) {
    if (!CanHandle(command)) return kResultNotFound;
    if (IpcCommandId(command[0]) == 0xC) {
        // Pinned Azahar returns zero and labels this STUBBED. Here zero is an
        // explicitly selected empty profile, not recovered console history.
        command.fill(0);
        command[0] = IpcMakeHeader(0xC, 2, 0);
        command[1] = kResultSuccess;
        command[2] = 0;
        return kResultSuccess;
    }

    const auto bytes = command[1] * 2U;
    const auto descriptor = command[4];
    const auto address = command[5];
    const auto end = std::uint64_t(address) + bytes;
    if (end > 0x100000000ULL || (bytes != 0 && !memory.IsWritable(address, bytes)))
        return kResultInvalidPointer;
    const auto cb = std::uint64_t(thread.tls_address) + kIpcCommandBufferOffset;
    if (bytes != 0 && address < cb + sizeof(IpcCommandBuffer) && cb < end) {
        // Explicit host boundary: the direct mapped-buffer model must not claim
        // to preserve output that would be overwritten by its own IPC reply.
        router.RequestHostStop("PTM history overlaps IPC response (unsupported alias)");
        return kResultSuccess;
    }
    // The selected profile contains no step events, at ANY requested timestamp.
    // The start-time u64 is opaque: no calendar arithmetic or fake clock advance.
    // Preflight the full span first, then use guest writes to invalidate exclusive
    // reservations. LoadBytes/ZeroBytes bypass guest-write accounting and are not
    // suitable here. No mappings can change during this single-threaded handler.
    for (std::uint32_t offset = 0; offset < bytes; ++offset) {
        if (!memory.Write8(address + offset, 0)) {
            router.RequestHostStop("PTM history write failed after preflight");
            return kResultSuccess;
        }
    }
    command.fill(0);
    command[0] = IpcMakeHeader(0xB, 1, 2);
    command[1] = kResultSuccess;
    command[2] = descriptor;
    command[3] = address;
    return kResultSuccess;
}
} // namespace lego::ctr
