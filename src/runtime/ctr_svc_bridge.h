#pragma once

#include <cstdint>

#include "recomp/a32_runtime.h"
#include "runtime/ctr_kernel.h"

namespace lego::ctr {
namespace a32 = oot3d::recomp::a32;

inline constexpr std::uint32_t kSvcCreateThread = 0x08U;
inline constexpr std::uint32_t kSvcExitThread = 0x09U;
inline constexpr std::uint32_t kSvcSleepThread = 0x0AU;
inline constexpr std::uint32_t kSvcCreateMutex = 0x13U;
inline constexpr std::uint32_t kSvcReleaseMutex = 0x14U;
inline constexpr std::uint32_t kSvcCreateSemaphore = 0x15U;
inline constexpr std::uint32_t kSvcReleaseSemaphore = 0x16U;
inline constexpr std::uint32_t kSvcCreateEvent = 0x17U;
inline constexpr std::uint32_t kSvcSignalEvent = 0x18U;
inline constexpr std::uint32_t kSvcClearEvent = 0x19U;
inline constexpr std::uint32_t kSvcCloseHandle = 0x23U;
inline constexpr std::uint32_t kSvcWaitSynchronization1 = 0x24U;
inline constexpr std::uint32_t kSvcWaitSynchronizationN = 0x25U;
inline constexpr std::uint32_t kSvcDuplicateHandle = 0x27U;
inline constexpr std::uint32_t kSvcConnectToPort = 0x2DU;
inline constexpr std::uint32_t kSvcSendSyncRequest = 0x32U;

class IpcRouter;

class SvcBridge final {
public:
    explicit SvcBridge(Kernel& kernel, IpcRouter* ipc = nullptr) noexcept
        : kernel_(kernel), ipc_(ipc) {}

    void SetIpcRouter(IpcRouter* ipc) noexcept { ipc_ = ipc; }

    a32::ExecutionResult Handle(const a32::ExecutionResult& exit,
                                a32::GuestState& state,
                                a32::MemoryBus* memory = nullptr) noexcept;

    bool ApplyPendingWake(a32::GuestState& state) noexcept;

private:
    Kernel& kernel_;
    IpcRouter* ipc_{};
};

}  // namespace lego::ctr
