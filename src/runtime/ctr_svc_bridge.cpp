#include "runtime/ctr_svc_bridge.h"

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

}  // namespace

a32::ExecutionResult SvcBridge::Handle(const a32::ExecutionResult& exit,
                                       a32::GuestState& state) noexcept {
    if (exit.kind != a32::ExitKind::Svc) {
        return exit;
    }

    switch (exit.detail) {
    case kSvcDuplicateHandle: {
        // CTR SVC ABI for Result DuplicateHandle(Handle* out, Handle handle):
        // r1 supplies the input handle; r0 receives Result and r1 receives the
        // duplicated handle on success.
        const ::lego::ctr::Handle source = state.r[1];
        ::lego::ctr::Handle duplicate = 0;
        const Result result = kernel_.DuplicateHandle(&duplicate, source);
        state.r[0] = result;
        if (result == kResultSuccess) {
            state.r[1] = duplicate;
        }
        return ResumeAfterSvc(exit.pc, state);
    }

    case kSvcCloseHandle: {
        // CTR SVC ABI for Result CloseHandle(Handle handle): r0 is both input
        // and result register.
        const ::lego::ctr::Handle handle = state.r[0];
        state.r[0] = kernel_.CloseHandle(handle);
        return ResumeAfterSvc(exit.pc, state);
    }

    default:
        return exit;
    }
}

}  // namespace lego::ctr
