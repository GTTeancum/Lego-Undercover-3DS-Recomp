#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "recomp/a32_runtime.h"
#include "runtime/ctr_cpu_schedule.h"

namespace lego::ctr {
namespace a32 = oot3d::recomp::a32;

using Handle = std::uint32_t;
using Result = std::uint32_t;

inline constexpr Result kResultSuccess = 0x00000000U;
inline constexpr Result kResultTimeout = 0x09401BFEU;
inline constexpr Result kResultInvalidPointer = 0xD8E007F6U;
inline constexpr Result kResultInvalidHandle = 0xD8E007F7U;
inline constexpr Result kResultOutOfHandles = 0xD8600413U;
inline constexpr Result kResultNotFound = 0xD88007FAU;
inline constexpr Result kResultPortNameTooLong = 0xE0E0181EU;
inline constexpr Result kResultOutOfRange = 0xE0E01BFDU;
inline constexpr Result kResultOutOfRangeKernel = 0xD8E007FDU;
inline constexpr Result kResultInvalidCombinationKernel = 0xD90007EEU;
inline constexpr Result kResultWrongLockingThread = 0xD8E0041FU;
inline constexpr Result kResultInvalidResultValue = 0xD8A007FFU;
inline constexpr Result kResultInvalidEnumValue = 0xD8E007EDU;
inline constexpr Result kResultInvalidCombination = 0xE0E01BEEU;
inline constexpr Result kResultMisalignedAddress = 0xE0E01BF1U;
inline constexpr Result kResultMisalignedSize = 0xE0E01BF2U;

inline constexpr Handle kCurrentThreadPseudoHandle = 0xFFFF8000U;
inline constexpr Handle kCurrentProcessPseudoHandle = 0xFFFF8001U;

inline constexpr std::uint32_t kThreadPriorityLowest = 63U;
inline constexpr std::uint32_t kThreadPriorityDefault = 48U;
inline constexpr std::int32_t kThreadProcessorDefault = -2;
inline constexpr std::int32_t kThreadProcessorAll = -1;
inline constexpr std::uint32_t kTlsAreaBase = 0x1FF82000U;
inline constexpr std::uint32_t kTlsEntrySize = 0x200U;
inline constexpr std::uint32_t kUserModeCpsr = 0x10U;
inline constexpr std::uint32_t kThreadInitialFpscr = 0x03C00000U;
inline constexpr std::uint32_t kMainThreadInitialFpscr = 0x03C00010U;

enum class ThreadStatus : std::uint8_t {
    Running,
    Ready,
    WaitSleep,
    WaitArb,
    WaitSynchAny,
    WaitSynchAll,
    Dormant,
    Dead,
};

enum class ResetType : std::uint32_t {
    OneShot = 0,
    Sticky = 1,
    Pulse = 2,
};

class Kernel;
class ThreadObject;
class WaitObject;
class GuestMemory;

class KernelObject {
public:
    enum class Type : std::uint8_t {
        Process,
        Thread,
        Event,
        Mutex,
        Semaphore,
        Timer,
        SharedMemory,
        Session,
        AddressArbiter,
        ResourceLimit,
        Other,
    };

    explicit KernelObject(Type type) noexcept : type_(type) {}
    virtual ~KernelObject() = default;

    [[nodiscard]] Type type() const noexcept { return type_; }

private:
    Type type_;
};

class ProcessObject final : public KernelObject {
public:
    explicit ProcessObject(std::uint32_t process_id) noexcept
        : KernelObject(Type::Process), process_id(process_id) {}

    std::uint32_t process_id{};
};

class WaitObject : public KernelObject {
public:
    explicit WaitObject(Type type) noexcept : KernelObject(type) {}
    ~WaitObject() override = default;

    [[nodiscard]] virtual bool ShouldWait(const ThreadObject& thread) const noexcept = 0;
    virtual void Acquire(ThreadObject& thread) noexcept = 0;
};

class ThreadObject final : public WaitObject,
                           public std::enable_shared_from_this<ThreadObject> {
public:
    explicit ThreadObject(std::uint32_t thread_id) noexcept
        : WaitObject(Type::Thread), thread_id(thread_id) {}

    [[nodiscard]] bool ShouldWait(const ThreadObject&) const noexcept override {
        return status != ThreadStatus::Dead;
    }
    void Acquire(ThreadObject&) noexcept override {}

    std::uint32_t thread_id{};
    bool execution_started{}; // Diagnostic provenance for bounded fresh-thread operations.
    ThreadStatus status{ThreadStatus::Dormant};
    std::uint32_t entry_point{};
    std::uint32_t argument{};
    std::uint32_t stack_top{};
    std::uint32_t priority{kThreadPriorityDefault};
    std::int32_t processor_id{};
    Result wait_result{kResultSuccess};
    std::int32_t wait_index{-1};
    bool wait_index_valid{};
    bool pending_wake{};
    std::uint32_t tls_address{};
    a32::GuestState guest_state{};

private:
    friend class Kernel;
    std::vector<std::shared_ptr<WaitObject>> wait_objects_{};
    std::optional<std::uint64_t> wake_deadline_ns_{};
    bool wait_reports_index_{};
};

// Service notification runs after ordinary waiter processing, matching the pinned
// kernel contract. It must not throw or run guest CPU instructions. A returned
// error is an explicit host stop; a signal may already have woken a waiter.
class EventSignalTarget {
public:
    virtual ~EventSignalTarget() = default;
    virtual const char* OnSignal() noexcept = 0;
};

class EventObject final : public WaitObject {
public:
    explicit EventObject(ResetType reset_type,
                         std::shared_ptr<EventSignalTarget> target = {}) noexcept
        : WaitObject(Type::Event), reset_type_(reset_type), signal_target_(std::move(target)) {}

    [[nodiscard]] bool ShouldWait(const ThreadObject&) const noexcept override {
        return !signaled_;
    }
    void Acquire(ThreadObject&) noexcept override {
        if (reset_type_ == ResetType::OneShot) {
            signaled_ = false;
        }
    }

    void Signal() noexcept { signaled_ = true; }
    void Clear() noexcept { signaled_ = false; }
    [[nodiscard]] bool signaled() const noexcept { return signaled_; }
    [[nodiscard]] ResetType reset_type() const noexcept { return reset_type_; }
    const char* NotifySignal() noexcept {
        return signal_target_ ? signal_target_->OnSignal() : nullptr;
    }

private:
    ResetType reset_type_;
    bool signaled_{};
    const std::shared_ptr<EventSignalTarget> signal_target_;
};

class MutexObject final : public WaitObject {
public:
    MutexObject() noexcept : WaitObject(Type::Mutex) {}

    [[nodiscard]] bool ShouldWait(const ThreadObject& thread) const noexcept override;
    void Acquire(ThreadObject& thread) noexcept override;
    Result Release(ThreadObject& thread) noexcept;

    [[nodiscard]] std::int32_t lock_count() const noexcept { return lock_count_; }
    [[nodiscard]] std::shared_ptr<ThreadObject> holding_thread() const noexcept {
        return holding_thread_.lock();
    }

private:
    std::int32_t lock_count_{};
    std::weak_ptr<ThreadObject> holding_thread_{};
};

class SemaphoreObject final : public WaitObject {
public:
    SemaphoreObject(std::int32_t initial_count, std::int32_t max_count) noexcept
        : WaitObject(Type::Semaphore),
          max_count_(max_count),
          available_count_(initial_count) {}

    [[nodiscard]] bool ShouldWait(const ThreadObject&) const noexcept override {
        return available_count_ <= 0;
    }
    void Acquire(ThreadObject&) noexcept override {
        if (available_count_ > 0) {
            --available_count_;
        }
    }
    Result Release(std::int32_t* out_count, std::int32_t release_count) noexcept;

    [[nodiscard]] std::int32_t max_count() const noexcept { return max_count_; }
    [[nodiscard]] std::int32_t available_count() const noexcept { return available_count_; }

private:
    std::int32_t max_count_{};
    std::int32_t available_count_{};
};

class AddressArbiterObject final : public KernelObject {
public:
    AddressArbiterObject() noexcept : KernelObject(Type::AddressArbiter) {}
};

enum class ResourceLimitType : std::uint32_t {
    Priority = 0,
    Commit = 1,
    Thread = 2,
    Event = 3,
    Mutex = 4,
    Semaphore = 5,
    Timer = 6,
    SharedMemory = 7,
    AddressArbiter = 8,
    CpuTime = 9,
    Max = 10,
};

class ResourceLimitObject final : public KernelObject {
public:
    ResourceLimitObject() noexcept;

    [[nodiscard]] std::int32_t Current(ResourceLimitType type) const noexcept;
    [[nodiscard]] std::int32_t Limit(ResourceLimitType type) const noexcept;
    void Reserve(ResourceLimitType type, std::int32_t amount) noexcept;

private:
    friend class Kernel; // Only the owning kernel updates application CPU state.
    void SetCpuTime(std::int32_t value) noexcept {
        current_[static_cast<std::size_t>(ResourceLimitType::CpuTime)] = value;
    }
    std::array<std::int32_t,
               static_cast<std::size_t>(ResourceLimitType::Max)> limits_{};
    std::array<std::int32_t,
               static_cast<std::size_t>(ResourceLimitType::Max)> current_{};
};

class GenericObject final : public KernelObject {
public:
    explicit GenericObject(Type type) noexcept : KernelObject(type) {}
};

class HandleTable final {
public:
    static constexpr std::size_t kMaxCount = 4096;

    HandleTable();

    void SetPseudoObjects(std::shared_ptr<KernelObject> current_thread,
                          std::shared_ptr<KernelObject> current_process) noexcept;

    Result Create(Handle* out_handle, std::shared_ptr<KernelObject> object) noexcept;
    // Allocate a copied IPC-object batch atomically. Failure preserves output,
    // free-list order, generation counters and every existing handle.
    Result CreateCopies(std::span<const std::shared_ptr<KernelObject>> objects,
                        std::span<Handle> out_handles) noexcept;
    Result Duplicate(Handle* out_handle, Handle handle) noexcept;
    Result Close(Handle handle) noexcept;

    [[nodiscard]] bool IsValid(Handle handle) const noexcept;
    [[nodiscard]] std::shared_ptr<KernelObject> Get(Handle handle) const noexcept;
    [[nodiscard]] std::shared_ptr<WaitObject> GetWaitObject(Handle handle) const noexcept;
    [[nodiscard]] std::size_t OpenHandleCount() const noexcept;

    void Clear() noexcept;

private:
    static constexpr std::uint16_t Slot(Handle handle) noexcept {
        return static_cast<std::uint16_t>(handle >> 15U);
    }

    static constexpr std::uint16_t Generation(Handle handle) noexcept {
        return static_cast<std::uint16_t>(handle & 0x7FFFU);
    }

    std::array<std::shared_ptr<KernelObject>, kMaxCount> objects_{};
    std::array<std::uint16_t, kMaxCount> generations_{};
    std::uint16_t next_generation_{1};
    std::uint16_t next_free_slot_{0};
    std::shared_ptr<KernelObject> current_thread_{};
    std::shared_ptr<KernelObject> current_process_{};
};

struct WaitOutcome {
    Result result{kResultSuccess};
    bool blocked{};
    std::int32_t index{-1};
    bool index_valid{};
};

class Kernel final {
public:
    Kernel(std::uint32_t process_id = 1, std::uint32_t thread_id = 1);

    [[nodiscard]] HandleTable& handles() noexcept { return handles_; }
    [[nodiscard]] const HandleTable& handles() const noexcept { return handles_; }
    [[nodiscard]] const std::shared_ptr<ProcessObject>& current_process() const noexcept {
        return current_process_;
    }
    [[nodiscard]] const std::shared_ptr<ThreadObject>& current_thread() const noexcept {
        return current_thread_;
    }
    [[nodiscard]] std::span<const std::shared_ptr<ThreadObject>> threads() const noexcept {
        return threads_;
    }
    [[nodiscard]] std::uint64_t now_ns() const noexcept { return now_ns_; }

    void SetCurrentGuestState(const a32::GuestState& state) noexcept;
    [[nodiscard]] const a32::GuestState& CurrentGuestState() const noexcept;
    bool Reschedule(a32::GuestState& live_state) noexcept;
    // One host executor selects a logical core; independent core priority order
    // and context/TLS are retained. Host core switching is not a guest preemption.
    bool ConfigureCpuExecution(CpuExecutionMode mode,
                               std::optional<std::uint32_t> launch_cpu_maximum = std::nullopt) noexcept;
    bool SelectDiagnosticCore(std::uint32_t core,a32::GuestState& live) noexcept;
    [[nodiscard]] bool DiagnosticCoreReady(std::uint32_t core) const noexcept;
    [[nodiscard]] CpuExecutionMode cpu_execution_mode() const noexcept{return cpu_mode_;}
    void SetDiagnosticTick(std::uint64_t tick) noexcept{diagnostic_tick_=tick;}
    [[nodiscard]] const Core1Quota& core1_quota() const noexcept{return core1_quota_;}
    bool ConsumeCpuQuota(std::uint64_t tick) noexcept;


    Result ControlMemory(GuestMemory* memory, std::uint32_t* out_address,
                         std::uint32_t addr0, std::uint32_t addr1,
                         std::uint32_t size, std::uint32_t operation,
                         std::uint32_t permissions) noexcept;

    Result CreateAddressArbiter(Handle* out_handle) noexcept;
    WaitOutcome ArbitrateAddress(GuestMemory* memory, Handle handle,
                                 std::uint32_t address, std::uint32_t type,
                                 std::int32_t value,
                                 std::int64_t timeout_ns) noexcept;

    Result GetProcessId(std::uint32_t* out_process_id,
                        Handle process_handle) noexcept;
    Result GetResourceLimit(Handle* out_handle,
                            Handle process_handle) noexcept;
    Result GetResourceLimitValues(GuestMemory* memory, bool current_values,
                                  std::uint32_t values_address,
                                  Handle resource_limit_handle,
                                  std::uint32_t names_address,
                                  std::uint32_t name_count) noexcept;

    // Bounded PM:APP UpdateResourceLimit(CpuTime) path into the SAME object
    // exposed by GetResourceLimit[Values]. Strict mode stops unsupported core-1
    // execution; DiagnosticDual uses its explicit timed application windows.
    // nullopt = host stop. No cycle-accurate hardware enforcement is claimed.
    std::optional<Result> UpdateAppCpuTimeLimit(std::uint32_t value) noexcept;
    [[nodiscard]] std::uint32_t app_cpu_time_current() const noexcept {
        return static_cast<std::uint32_t>(application_resource_limit_->Current(ResourceLimitType::CpuTime));
    }
    [[nodiscard]] std::int32_t app_cpu_time_maximum() const noexcept {
        return application_resource_limit_->Limit(ResourceLimitType::CpuTime);
    }
    [[nodiscard]] bool app_cpu_core0_only() const noexcept { return app_cpu_core0_only_; }
    [[nodiscard]] bool AppCpuExecutionSupported() const noexcept;
    [[nodiscard]] bool AppCpuThreadCreationSupported(std::int32_t processor) const noexcept {
        // Leave invalid IDs to the existing guest argument validation.
        if(cpu_mode_==CpuExecutionMode::DiagnosticDual)
            return processor!=2 && processor!=3;
        return !app_cpu_core0_only_ || processor < 1 || processor > 3;
    }

    Result DuplicateHandle(Handle* out_handle, Handle handle) noexcept;
    Result CloseHandle(Handle handle) noexcept;

    Result CreateEvent(Handle* out_handle, std::uint32_t reset_type) noexcept;
    // nullopt: notifier failure, not an invented guest Result.
    std::optional<Result> SignalEvent(Handle handle) noexcept;
    // Service holds a real EventObject reference even after the client handle
    // closes. Use the same wake/pulse semantics without allocating a fake handle.
    bool SignalEventObject(EventObject& event) noexcept;
    const char* event_signal_error() const noexcept { return event_signal_error_; }
    Result ClearEvent(Handle handle) noexcept;

    Result CreateMutex(Handle* out_handle, bool initial_locked) noexcept;
    Result ReleaseMutex(Handle handle) noexcept;

    Result CreateSemaphore(Handle* out_handle, std::int32_t initial_count,
                           std::int32_t max_count) noexcept;
    Result ReleaseSemaphore(std::int32_t* out_count, Handle handle,
                            std::int32_t release_count) noexcept;

    WaitOutcome WaitSynchronization1(Handle handle, std::int64_t timeout_ns) noexcept;
    WaitOutcome WaitSynchronizationN(std::span<const Handle> handles, bool wait_all,
                                     std::int64_t timeout_ns) noexcept;

    Result CreateThread(Handle* out_handle, std::uint32_t entry_point,
                        std::uint32_t argument, std::uint32_t stack_top,
                        std::uint32_t priority, std::int32_t processor_id) noexcept;
    // First priority assignment before the thread executes. Broader priority
    // inheritance/reordering on running or waiting threads is not modeled here.
    std::optional<Result> SetFreshThreadPriority(Handle handle,std::uint32_t priority) noexcept;
    void ExitCurrentThread() noexcept;
    bool SleepCurrentThread(std::int64_t nanoseconds) noexcept;

    void AdvanceTime(std::uint64_t nanoseconds) noexcept;
    [[nodiscard]] std::optional<std::uint64_t> NextWakeDeadline() const noexcept;
    [[nodiscard]] std::shared_ptr<ThreadObject> HighestPriorityReadyThread() const noexcept;
    bool ConsumeCurrentThreadWake(Result* result, std::int32_t* index,
                                  bool* index_valid) noexcept;

private:
    void SetDeadline(ThreadObject& thread, std::int64_t timeout_ns) noexcept;
    void ClearWait(ThreadObject& thread) noexcept;
    void WakeThread(ThreadObject& thread, Result result, std::int32_t index,
                    bool index_valid) noexcept;
    void TryWakeWaitingThreads() noexcept;
    void RemoveArbiterWait(ThreadObject& thread) noexcept;
    std::uint32_t AllocateTlsAddress() noexcept;
    void InitializeGuestContext(ThreadObject& thread, std::uint32_t entry_point,
                                std::uint32_t argument, std::uint32_t stack_top,
                                std::uint32_t fpscr) noexcept;
    void ApplyPendingWakeToContext(ThreadObject& thread) noexcept;

    std::shared_ptr<ProcessObject> current_process_;
    std::shared_ptr<ResourceLimitObject> application_resource_limit_;
    bool app_cpu_core0_only_{};
    CpuExecutionMode cpu_mode_{CpuExecutionMode::Strict};
    Core1Quota core1_quota_;
    std::uint64_t diagnostic_tick_{};
    std::array<std::weak_ptr<ThreadObject>,2> diagnostic_core_threads_{};
    std::shared_ptr<ThreadObject> current_thread_;
    const char* event_signal_error_{};
    HandleTable handles_;

    struct ArbiterWait {
        std::shared_ptr<AddressArbiterObject> arbiter;
        std::shared_ptr<ThreadObject> thread;
        std::uint32_t address{};
    };

    std::vector<std::shared_ptr<ThreadObject>> threads_;
    std::vector<ArbiterWait> arbiter_waits_;
    std::uint32_t next_thread_id_{2};
    std::uint32_t next_tls_slot_{};
    std::uint64_t now_ns_{};
};

}  // namespace lego::ctr
