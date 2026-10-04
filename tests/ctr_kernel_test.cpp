#include "runtime/ctr_kernel.h"
#include "runtime/ctr_svc_bridge.h"

#include <cstdlib>
#include <iostream>
#include <memory>

using lego::ctr::GenericObject;
using lego::ctr::Handle;
using lego::ctr::KernelObject;

namespace {

int failures = 0;

#define CHECK(expr)                                                                    \
    do {                                                                               \
        if (!(expr)) {                                                                 \
            std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << ": " #expr "\n"; \
            ++failures;                                                                \
        }                                                                              \
    } while (0)

void TestPseudoHandlesAndDuplicate() {
    lego::ctr::Kernel kernel(7, 11);
    auto thread = kernel.handles().Get(lego::ctr::kCurrentThreadPseudoHandle);
    auto process = kernel.handles().Get(lego::ctr::kCurrentProcessPseudoHandle);
    CHECK(thread == kernel.current_thread());
    CHECK(process == kernel.current_process());
    CHECK(thread->type() == KernelObject::Type::Thread);
    CHECK(process->type() == KernelObject::Type::Process);

    Handle duplicate = 0;
    CHECK(kernel.DuplicateHandle(&duplicate, lego::ctr::kCurrentThreadPseudoHandle) ==
          lego::ctr::kResultSuccess);
    CHECK(duplicate == 1U);
    CHECK(kernel.handles().IsValid(duplicate));
    CHECK(kernel.handles().Get(duplicate) == thread);
    CHECK(kernel.handles().OpenHandleCount() == 1U);

    CHECK(kernel.CloseHandle(duplicate) == lego::ctr::kResultSuccess);
    CHECK(!kernel.handles().IsValid(duplicate));
    CHECK(kernel.handles().Get(duplicate) == nullptr);
    CHECK(kernel.CloseHandle(duplicate) == lego::ctr::kResultInvalidHandle);
    CHECK(kernel.CloseHandle(lego::ctr::kCurrentThreadPseudoHandle) ==
          lego::ctr::kResultInvalidHandle);
}

void TestGenerationProtectsStaleHandles() {
    lego::ctr::Kernel kernel;
    Handle first = 0;
    CHECK(kernel.handles().Create(
              &first, std::make_shared<GenericObject>(KernelObject::Type::Event)) ==
          lego::ctr::kResultSuccess);
    CHECK(kernel.CloseHandle(first) == lego::ctr::kResultSuccess);

    Handle second = 0;
    CHECK(kernel.handles().Create(
              &second, std::make_shared<GenericObject>(KernelObject::Type::Event)) ==
          lego::ctr::kResultSuccess);
    CHECK(first != second);
    CHECK(!kernel.handles().IsValid(first));
    CHECK(kernel.handles().IsValid(second));
    CHECK((first >> 15U) == (second >> 15U));
}

void TestTableLimit() {
    lego::ctr::Kernel kernel;
    Handle handle = 0;
    for (std::size_t i = 0; i < lego::ctr::HandleTable::kMaxCount; ++i) {
        CHECK(kernel.handles().Create(
                  &handle, std::make_shared<GenericObject>(KernelObject::Type::Other)) ==
              lego::ctr::kResultSuccess);
    }
    CHECK(kernel.handles().OpenHandleCount() == lego::ctr::HandleTable::kMaxCount);
    CHECK(kernel.handles().Create(
              &handle, std::make_shared<GenericObject>(KernelObject::Type::Other)) ==
          lego::ctr::kResultOutOfHandles);
}

void TestDuplicateSvcStage2Shape() {
    lego::ctr::Kernel kernel(1, 1);
    lego::ctr::SvcBridge bridge(kernel);
    oot3d::recomp::a32::GuestState state{};
    state.r[1] = lego::ctr::kCurrentThreadPseudoHandle;
    state.r[15] = 0x0013313CU;

    const oot3d::recomp::a32::ExecutionResult svc_exit{
        oot3d::recomp::a32::ExitKind::Svc,
        0x0013313CU,
        oot3d::recomp::a32::FallbackReason::None,
        lego::ctr::kSvcDuplicateHandle,
    };

    const auto resumed = bridge.Handle(svc_exit, state);
    CHECK(resumed.kind == oot3d::recomp::a32::ExitKind::Fallthrough);
    CHECK(resumed.pc == 0x00133140U);
    CHECK(state.r[15] == 0x00133140U);
    CHECK(state.r[0] == lego::ctr::kResultSuccess);
    CHECK(state.r[1] != lego::ctr::kCurrentThreadPseudoHandle);
    CHECK(kernel.handles().IsValid(state.r[1]));
    CHECK(kernel.handles().Get(state.r[1]) == kernel.current_thread());

    state.r[0] = state.r[1];
    const oot3d::recomp::a32::ExecutionResult close_exit{
        oot3d::recomp::a32::ExitKind::Svc,
        0x00140000U,
        oot3d::recomp::a32::FallbackReason::None,
        lego::ctr::kSvcCloseHandle,
    };
    const auto after_close = bridge.Handle(close_exit, state);
    CHECK(after_close.kind == oot3d::recomp::a32::ExitKind::Fallthrough);
    CHECK(state.r[0] == lego::ctr::kResultSuccess);
}

void TestInvalidAndUnsupportedSvc() {
    lego::ctr::Kernel kernel;
    lego::ctr::SvcBridge bridge(kernel);
    oot3d::recomp::a32::GuestState state{};
    state.r[1] = 0x12345678U;

    const oot3d::recomp::a32::ExecutionResult bad_duplicate{
        oot3d::recomp::a32::ExitKind::Svc,
        0x00100000U,
        oot3d::recomp::a32::FallbackReason::None,
        lego::ctr::kSvcDuplicateHandle,
    };
    bridge.Handle(bad_duplicate, state);
    CHECK(state.r[0] == lego::ctr::kResultInvalidHandle);
    CHECK(state.r[1] == 0x12345678U);

    const auto before = state;
    const oot3d::recomp::a32::ExecutionResult unsupported{
        oot3d::recomp::a32::ExitKind::Svc,
        0x00100004U,
        oot3d::recomp::a32::FallbackReason::None,
        0x7FU,
    };
    const auto returned = bridge.Handle(unsupported, state);
    CHECK(returned.kind == oot3d::recomp::a32::ExitKind::Svc);
    CHECK(returned.detail == 0x7FU);
    CHECK(state.r == before.r);
}

}  // namespace

int main() {
    TestPseudoHandlesAndDuplicate();
    TestGenerationProtectsStaleHandles();
    TestTableLimit();
    TestDuplicateSvcStage2Shape();
    TestInvalidAndUnsupportedSvc();
    if (failures != 0) {
        std::cerr << failures << " CTR kernel checks failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "PASS: CTR handle/SVC checks\n";
    return EXIT_SUCCESS;
}
