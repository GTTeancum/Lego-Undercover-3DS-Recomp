#pragma once

#include <cstdint>

#include "recomp/a32_runtime.h"
#include "runtime/ctr_kernel.h"

namespace lego::ctr {
namespace a32 = oot3d::recomp::a32;

inline constexpr std::uint32_t kSvcCloseHandle = 0x23U;
inline constexpr std::uint32_t kSvcDuplicateHandle = 0x27U;

class SvcBridge final {
public:
    explicit SvcBridge(Kernel& kernel) noexcept : kernel_(kernel) {}

    // Consumes a TriAevum SVC exit when supported. A handled SVC returns a
    // Fallthrough result at pc+4. Unsupported SVCs are returned unchanged so
    // the outer runner can diagnose/stop rather than fabricating success.
    a32::ExecutionResult Handle(const a32::ExecutionResult& exit,
                                a32::GuestState& state) noexcept;

private:
    Kernel& kernel_;
};

}  // namespace lego::ctr
