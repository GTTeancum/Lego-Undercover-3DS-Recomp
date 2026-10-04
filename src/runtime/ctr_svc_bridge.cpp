#include "runtime/ctr_svc_bridge.h"

#include <vector>

namespace lego::ctr {
namespace {

a32::ExecutionResult ResumeAfterSvc(std::uint32_t pc,
                                    a32::GuestState& state) noexcept {
    const std::uint32_t next_pc = pc + 4U;
    state.r[15] = next_pc;
    return {
        a32::ExitKind::Fallthrough,
        next_pc,
        a32::FallbackReason::None,
        0U,
    };
}

a32::ExecutionResult WaitAfterSvc(std::uint32_t pc,
                                  a32::GuestState& state) noexcept {
    const std::uint32_t next_pc = pc + 4U;
    state.r[15] = next_pc;
    return {
        a32::ExitKind::Wait,
        next_pc,
        a32::FallbackReason::None,
        0U,
    };
}

std::int64_t Signed64(std::uint32_t low, std::uint32_t high) noexcept {
    const std::uint64_t bits =
        static_cast<std::uint64_t>(low) |
        (static_cast<std::uint64_t>(high) << 32U);
    return static_cast<std::int64_t>(bits);
}

}  // namespace

bool SvcBridge::ApplyPendingWake(a32::GuestState& state) noexcept {
    Result result = kResultSuccess;
    std::int32_t index = -1;
    bool index_valid = false;
    if (!kernel_.ConsumeCurrentThreadWake(&result, &index, &index_valid)) {
        return false;
    }
    state.r[0] = result;
    if (index_valid) {
        state.r[1] = static_cast<std::uint32_t>(index);
    }
    return true;
}

a32::ExecutionResult SvcBridge::Handle(const a32::ExecutionResult& exit,
                                       a32::GuestState& state,
                                       a32::MemoryBus* memory) noexcept {
    if (exit.kind != a32::ExitKind::Svc) {
        return exit;
    }

    switch (exit.detail) {
    case kSvcCreateThread: {
        ::lego::ctr::Handle handle = 0;
        const Result result = kernel_.CreateThread(
            &handle, state.r[1], state.r[2], state.r[3], state.r[0],
            static_cast<std::int32_t>(state.r[4]));
        state.r[0] = result;
        if (result == kResultSuccess) {
            state.r[1] = handle;
        }
        return ResumeAfterSvc(exit.pc, state);
    }

    case kSvcExitThread:
        kernel_.ExitCurrentThread();
        return WaitAfterSvc(exit.pc, state);

    case kSvcSleepThread: {
        const std::int64_t nanoseconds = Signed64(state.r[0], state.r[1]);
        const bool blocked = kernel_.SleepCurrentThread(nanoseconds);
        return blocked ? WaitAfterSvc(exit.pc, state)
                       : ResumeAfterSvc(exit.pc, state);
    }

    case kSvcCreateMutex: {
        ::lego::ctr::Handle handle = 0;
        const Result result = kernel_.CreateMutex(&handle, state.r[1] != 0U);
        state.r[0] = result;
        if (result == kResultSuccess) {
            state.r[1] = handle;
        }
        return ResumeAfterSvc(exit.pc, state);
    }

    case kSvcReleaseMutex:
        state.r[0] = kernel_.ReleaseMutex(state.r[0]);
        return ResumeAfterSvc(exit.pc, state);

    case kSvcCreateSemaphore: {
        ::lego::ctr::Handle handle = 0;
        const Result result = kernel_.CreateSemaphore(
            &handle, static_cast<std::int32_t>(state.r[1]),
            static_cast<std::int32_t>(state.r[2]));
        state.r[0] = result;
        if (result == kResultSuccess) {
            state.r[1] = handle;
        }
        return ResumeAfterSvc(exit.pc, state);
    }

    case kSvcReleaseSemaphore: {
        std::int32_t previous = 0;
        const Result result = kernel_.ReleaseSemaphore(
            &previous, state.r[1], static_cast<std::int32_t>(state.r[2]));
        state.r[0] = result;
        if (result == kResultSuccess) {
            state.r[1] = static_cast<std::uint32_t>(previous);
        }
        return ResumeAfterSvc(exit.pc, state);
    }

    case kSvcCreateEvent: {
        ::lego::ctr::Handle handle = 0;
        const Result result = kernel_.CreateEvent(&handle, state.r[1]);
        state.r[0] = result;
        if (result == kResultSuccess) {
            state.r[1] = handle;
        }
        return ResumeAfterSvc(exit.pc, state);
    }

    case kSvcSignalEvent:
        state.r[0] = kernel_.SignalEvent(state.r[0]);
        return ResumeAfterSvc(exit.pc, state);

    case kSvcClearEvent:
        state.r[0] = kernel_.ClearEvent(state.r[0]);
        return ResumeAfterSvc(exit.pc, state);

    case kSvcDuplicateHandle: {
        const ::lego::ctr::Handle source = state.r[1];
        ::lego::ctr::Handle duplicate = 0;
        const Result result = kernel_.DuplicateHandle(&duplicate, source);
        state.r[0] = result;
        if (result == kResultSuccess) {
            state.r[1] = duplicate;
        }
        return ResumeAfterSvc(exit.pc, state);
    }

    case kSvcCloseHandle:
        state.r[0] = kernel_.CloseHandle(state.r[0]);
        return ResumeAfterSvc(exit.pc, state);

    case kSvcWaitSynchronization1: {
        const std::int64_t timeout = Signed64(state.r[2], state.r[3]);
        const WaitOutcome outcome =
            kernel_.WaitSynchronization1(state.r[0], timeout);
        state.r[0] = outcome.result;
        return outcome.blocked ? WaitAfterSvc(exit.pc, state)
                               : ResumeAfterSvc(exit.pc, state);
    }

    case kSvcWaitSynchronizationN: {
        if (memory == nullptr) {
            return exit;
        }

        const std::int32_t count = static_cast<std::int32_t>(state.r[2]);
        if (count < 0) {
            state.r[0] = kResultOutOfRange;
            return ResumeAfterSvc(exit.pc, state);
        }

        std::vector<::lego::ctr::Handle> handles(static_cast<std::size_t>(count));
        for (std::int32_t index = 0; index < count; ++index) {
            std::uint32_t handle = 0;
            if (!memory->Read32(state.r[1] + static_cast<std::uint32_t>(index) * 4U,
                                &handle)) {
                state.r[0] = kResultInvalidPointer;
                return ResumeAfterSvc(exit.pc, state);
            }
            handles[static_cast<std::size_t>(index)] = handle;
        }

        const std::int64_t timeout = Signed64(state.r[0], state.r[4]);
        const WaitOutcome outcome = kernel_.WaitSynchronizationN(
            handles, state.r[3] != 0U, timeout);
        state.r[0] = outcome.result;
        if (outcome.index_valid) {
            state.r[1] = static_cast<std::uint32_t>(outcome.index);
        }
        if (outcome.blocked) {
            state.r[1] = 0xFFFFFFFFU;
            return WaitAfterSvc(exit.pc, state);
        }
        return ResumeAfterSvc(exit.pc, state);
    }

    default:
        return exit;
    }
}

}  // namespace lego::ctr
