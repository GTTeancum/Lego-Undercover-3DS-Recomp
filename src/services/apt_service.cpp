#include "services/apt_service.h"

namespace lego::ctr {
bool AptService::CanHandle(const IpcCommandBuffer& command) const noexcept {
    // Application startup shapes plus the paired parameter-consumption API.
    // Unknown/malformed requests stop before side effects. ReceiveParameter is
    // component-tested; this checkpoint does not claim the game called it.
    if (command[0] == IpcMakeHeader(1, 1, 0)) return command[1] == 0;
    if (command[0] == IpcMakeHeader(2, 2, 0))
        return !initialized_ && command[1] == 0x300 && command[2] == 0;
    if (command[0] == IpcMakeHeader(3, 1, 0))
        return initialized_ && command[1] == 0;
    if (command[0] == IpcMakeHeader(0x43, 1, 0))
        return initialized_ && registered_ && command[1] == 0x300;
    if (command[0] == IpcMakeHeader(0x4B, 3, 2)) {
        // Only the two observed wrappers: SleepIfShellClosed (4) and
        // UnlockTransition (7). Other utility IDs are still explicit stops.
        const auto size = command[1] == 4 ? 1U : command[1] == 7 ? 4U : 0U;
        return initialized_ && registered_ && size != 0 &&
               command[2] == size && command[3] == 1 &&
               command[4] == ((size << 14U) | (1U << 10U) | 2U);
    }
    if (command[0] == IpcMakeHeader(0xE, 2, 0) ||
        command[0] == IpcMakeHeader(0xD, 2, 0))
        return initialized_ && command[1] == 0x300 && command[2] <= 0x1000;
    return false;
}

Result AptService::Handle(IpcRouter&, Kernel& kernel, GuestMemory& memory, ThreadObject& thread,
                          IpcCommandBuffer& command) {
    if (IpcCommandId(command[0]) == 0x4B)
        return ReplyBoundedUtility(memory, thread, command);
    if (IpcCommandId(command[0]) == 0xE || IpcCommandId(command[0]) == 0xD)
        return ReadLaunchParameter(memory, thread, command);
    if (IpcCommandId(command[0]) == 0x43) {
        // Observed NotifyToWait(app=0x300). The pinned Azahar apt.cpp handler
        // acknowledges this command with one Result word and NO side effects;
        // upstream explicitly labels it STUBBED. This is that bounded HLE
        // compatibility policy, not a claim of complete hardware semantics.
        // In particular, do not consume/requeue the launch message, signal an
        // event, block/wake a thread, or advance guest time because of its name.
        command.fill(0);
        command[0] = IpcMakeHeader(0x43, 1, 0);
        command[1] = kResultSuccess;
        return kResultSuccess;
    }
    if (IpcCommandId(command[0]) == 1) {
        const auto attributes = command[1];
        ::lego::ctr::Handle handle = 0;
        const Result result = kernel.handles().Create(&handle, lock_);
        if (result != kResultSuccess) return result;
        command.fill(0);
        command[0] = IpcMakeHeader(1, 3, 2);
        command[1] = kResultSuccess;
        command[2] = attributes;
        command[3] = 0; // Pinned AppletManager::GetLockHandle state.
        command[4] = IpcCopyHandleDesc();
        command[5] = handle;
        return kResultSuccess;
    }

    if (IpcCommandId(command[0]) == 3) {
        // Observed application Enable(attrs=0). AppletManager::Enable marks
        // this slot registered; first Initialize has already done so. There
        // are no delayed parameters or other applet roles in this slice.
        // Repeated Enable must not create a second Wakeup, re-signal events,
        // consume the pending message, or change kernel thread scheduling.
        registered_ = true;
        command.fill(0);
        command[0] = IpcMakeHeader(3, 1, 0);
        command[1] = kResultSuccess;
        return kResultSuccess;
    }

    // First application initialization: the pinned manager enables the first
    // applet and queues a real Wakeup parameter (sender None, receiver 0x300).
    // Its parameter event represents that queued message, NOT a fake GPU or
    // notification completion. Neither event is recreated on a new APT session.
    ::lego::ctr::Handle notification_handle = 0, parameter_handle = 0;
    Result result = kernel.handles().Create(&notification_handle, notification_);
    if (result != kResultSuccess) return result;
    result = kernel.handles().Create(&parameter_handle, parameter_);
    if (result != kResultSuccess) {
        kernel.CloseHandle(notification_handle);
        return result;
    }
    initialized_ = true;
    registered_ = true; // Initialize auto-enables the first application.
    pending_parameter_ = LaunchParameter{};
    parameter_->Signal();
    command.fill(0);
    command[0] = IpcMakeHeader(2, 1, 3);
    command[1] = kResultSuccess;
    command[2] = IpcCopyHandleDesc(2);
    command[3] = notification_handle;
    command[4] = parameter_handle;
    return kResultSuccess;
}

Result AptService::ReplyBoundedUtility(GuestMemory& memory, ThreadObject& thread,
                                       IpcCommandBuffer& command) {
    // libctru: utility 4 = SleepIfShellClosed (1-byte input), utility 7 =
    // UnlockTransition (4-byte input); both request one output byte. Pinned
    // Azahar explicitly STUBS these with ignored input, two success words and
    // zero output. This is that limited HLE policy, NOT real lid/sleep or
    // transition-lock emulation. No shell state, sleep completion or lock
    // success is synthesized; utility 6 and all other IDs remain unsupported.
    const auto source = command[5];
    const auto size = command[2]; // CanHandle restricts this to 1 or 4.
    if (std::uint64_t(source) + size > 0x100000000ULL)
        return kResultInvalidPointer;
    // Read the exact input extent before writing even when source and output
    // overlap. In particular utility 4 must not require a four-byte mapping.
    if (size == 1) {
        std::uint8_t input = 0;
        if (!memory.Read8(source, &input)) return kResultInvalidPointer;
    } else {
        std::uint32_t input = 0;
        if (!memory.Read32(source, &input)) return kResultInvalidPointer;
    }

    const auto table = std::uint64_t(thread.tls_address) + 0x180U;
    std::uint32_t descriptor = 0, destination = 0;
    if (table + 8U > 0x100000000ULL ||
        !memory.Read32(static_cast<std::uint32_t>(table), &descriptor) ||
        !memory.Read32(static_cast<std::uint32_t>(table + 4U), &destination) ||
        (descriptor & 0x3FFFU) != 2U || (descriptor >> 14U) < 1U ||
        !memory.IsWritable(destination, 1U))
        return kResultInvalidPointer;

    // Reject aliasing our own response/receive descriptor rather than silently
    // overwriting the returned byte during reply copy-out. This is host safety
    // policy, not an asserted CTR hardware error convention. Input/output
    // aliasing is safe because the input was already copied above.
    const auto response = std::uint64_t(thread.tls_address) + kIpcCommandBufferOffset;
    if ((destination >= response && destination < response + sizeof(command)) ||
        (destination >= table && destination < table + 8U))
        return kResultInvalidPointer;

    // All validation precedes writes; one-byte guest write also invalidates
    // exclusive reservations. No guest-sized allocation and no state changes.
    if (!memory.Write8(destination, 0)) return kResultInvalidPointer;
    command.fill(0);
    command[0] = IpcMakeHeader(0x4B, 2, 2);
    command[1] = kResultSuccess;
    command[2] = kResultSuccess; // Utility result, distinct from transport result.
    command[3] = (1U << 14U) | 2U;
    command[4] = destination;
    return kResultSuccess;
}

Result AptService::ReadLaunchParameter(GuestMemory& memory, ThreadObject& thread,
                                     IpcCommandBuffer& command) {
    const auto id = IpcCommandId(command[0]);
    if (!pending_parameter_) {
        // Transport success, service NoData. No destination is touched and no
        // replacement Wakeup is synthesized after the message was consumed.
        command.fill(0);
        command[0] = IpcMakeHeader(id, 1, 0);
        command[1] = kResultAptNoData;
        return kResultSuccess;
    }

    const auto size = command[2]; // CanHandle bounded this to 0x1000.
    // Receive static buffer 0 is outside the 64-word IPC command, at TLS+0x180.
    const auto table = std::uint64_t(thread.tls_address) + 0x180U;
    std::uint32_t descriptor = 0, destination = 0;
    if (table + 8U > 0x100000000ULL ||
        !memory.Read32(static_cast<std::uint32_t>(table), &descriptor) ||
        !memory.Read32(static_cast<std::uint32_t>(table + 4U), &destination) ||
        (descriptor & 0x3FFFU) != 2U || (descriptor >> 14U) < size)
        return kResultInvalidPointer;
    if (size != 0 && (std::uint64_t(destination) + size > 0x100000000ULL ||
                     !memory.IsWritable(destination, size)))
        return kResultInvalidPointer;

    // The pinned APT handler pads to requested size, even for the empty launch
    // message. Validate first; use guest writes so exclusive reservations are
    // invalidated. This never allocates from a guest-provided size.
    for (std::uint32_t i = 0; i < size; ++i)
        if (!memory.Write8(destination + i, 0)) return kResultInvalidPointer;

    IpcCommandBuffer response{};
    response[0] = IpcMakeHeader(id, 4, 4);
    response[1] = kResultSuccess;
    response[2] = pending_parameter_->sender_id;
    response[3] = pending_parameter_->signal;
    response[4] = 0; // The queued launch message has no payload.
    // 3dbrew distinguishes Glance copy from Receive move. Pinned Azahar uses
    // move for both; our documented wire policy follows 3dbrew here. The launch
    // object is null, so neither path allocates, copies, or moves a real handle.
    response[5] = id == 0xE ? IpcCopyHandleDesc() : IpcMoveHandleDesc();
    response[6] = 0;
    response[7] = (size << 14U) | 2U; // Padded size, matching pinned APT output.
    response[8] = destination;
    command = response;
    if (id == 0xD) pending_parameter_.reset();
    // Event acquisition and message consumption are separate operations. Do
    // not re-signal or clear either event as a shortcut for receiving a message.
    return kResultSuccess;
}
} // namespace lego::ctr
