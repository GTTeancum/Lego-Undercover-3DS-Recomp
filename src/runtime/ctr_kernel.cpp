#include "runtime/ctr_kernel.h"
#include "runtime/ctr_memory.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace lego::ctr {

ResourceLimitObject::ResourceLimitObject() noexcept
    : KernelObject(Type::ResourceLimit) {
    limits_[static_cast<std::size_t>(ResourceLimitType::Priority)] = 0x18;
    limits_[static_cast<std::size_t>(ResourceLimitType::Commit)] = 0x04000000;
    limits_[static_cast<std::size_t>(ResourceLimitType::Thread)] = 0x20;
    limits_[static_cast<std::size_t>(ResourceLimitType::Event)] = 0x20;
    limits_[static_cast<std::size_t>(ResourceLimitType::Mutex)] = 0x20;
    limits_[static_cast<std::size_t>(ResourceLimitType::Semaphore)] = 0x8;
    limits_[static_cast<std::size_t>(ResourceLimitType::Timer)] = 0x8;
    limits_[static_cast<std::size_t>(ResourceLimitType::SharedMemory)] = 0x10;
    limits_[static_cast<std::size_t>(ResourceLimitType::AddressArbiter)] = 0x2;
    limits_[static_cast<std::size_t>(ResourceLimitType::CpuTime)] = 80;

    // Recovered Stage-2 accounting: code/data/BSS/main-stack consume
    // 0x4B5000 before the 0x124B000 normal and 0x2900000 linear heaps.
    current_[static_cast<std::size_t>(ResourceLimitType::Commit)] = 0x004B5000;
    current_[static_cast<std::size_t>(ResourceLimitType::Thread)] = 1;
}

std::int32_t ResourceLimitObject::Current(ResourceLimitType type) const noexcept {
    return current_[static_cast<std::size_t>(type)];
}

std::int32_t ResourceLimitObject::Limit(ResourceLimitType type) const noexcept {
    return limits_[static_cast<std::size_t>(type)];
}

void ResourceLimitObject::Reserve(ResourceLimitType type,
                                  std::int32_t amount) noexcept {
    current_[static_cast<std::size_t>(type)] += amount;
}

bool MutexObject::ShouldWait(const ThreadObject& thread) const noexcept {
    const auto holder = holding_thread_.lock();
    return lock_count_ > 0 && holder.get() != &thread;
}

void MutexObject::Acquire(ThreadObject& thread) noexcept {
    if (lock_count_ == 0) {
        holding_thread_ = thread.shared_from_this();
        lock_count_ = 1;
        return;
    }
    const auto holder = holding_thread_.lock();
    if (holder.get() == &thread) {
        ++lock_count_;
    }
}

Result MutexObject::Release(ThreadObject& thread) noexcept {
    const auto holder = holding_thread_.lock();
    if (holder.get() != &thread) {
        return kResultWrongLockingThread;
    }
    if (lock_count_ <= 0) {
        return kResultInvalidResultValue;
    }
    --lock_count_;
    if (lock_count_ == 0) {
        holding_thread_.reset();
    }
    return kResultSuccess;
}

Result SemaphoreObject::Release(std::int32_t* out_count,
                                std::int32_t release_count) noexcept {
    const std::int64_t new_count =
        static_cast<std::int64_t>(available_count_) + release_count;
    if (new_count > max_count_) {
        return kResultOutOfRangeKernel;
    }
    if (out_count != nullptr) {
        *out_count = available_count_;
    }
    available_count_ = static_cast<std::int32_t>(new_count);
    return kResultSuccess;
}

HandleTable::HandleTable() {
    Clear();
}

void HandleTable::SetPseudoObjects(std::shared_ptr<KernelObject> current_thread,
                                   std::shared_ptr<KernelObject> current_process) noexcept {
    current_thread_ = std::move(current_thread);
    current_process_ = std::move(current_process);
}

Result HandleTable::Create(Handle* out_handle, std::shared_ptr<KernelObject> object) noexcept {
    if (out_handle == nullptr || object == nullptr) {
        return kResultInvalidHandle;
    }

    const std::uint16_t slot = next_free_slot_;
    if (slot >= generations_.size()) {
        return kResultOutOfHandles;
    }
    next_free_slot_ = generations_[slot];

    const std::uint16_t generation = next_generation_++;
    if (next_generation_ >= (1U << 15U)) {
        next_generation_ = 1;
    }

    generations_[slot] = generation;
    objects_[slot] = std::move(object);
    *out_handle = static_cast<Handle>(generation) |
                  (static_cast<Handle>(slot) << 15U);
    return kResultSuccess;
}

Result HandleTable::Duplicate(Handle* out_handle, Handle handle) noexcept {
    std::shared_ptr<KernelObject> object = Get(handle);
    if (object == nullptr) {
        return kResultInvalidHandle;
    }
    return Create(out_handle, std::move(object));
}

Result HandleTable::Close(Handle handle) noexcept {
    if (!IsValid(handle)) {
        return kResultInvalidHandle;
    }

    const std::uint16_t slot = Slot(handle);
    objects_[slot].reset();
    generations_[slot] = next_free_slot_;
    next_free_slot_ = slot;
    return kResultSuccess;
}

bool HandleTable::IsValid(Handle handle) const noexcept {
    const std::uint16_t slot = Slot(handle);
    const std::uint16_t generation = Generation(handle);
    return slot < kMaxCount && objects_[slot] != nullptr &&
           generations_[slot] == generation;
}

std::shared_ptr<KernelObject> HandleTable::Get(Handle handle) const noexcept {
    if (handle == kCurrentThreadPseudoHandle) {
        return current_thread_;
    }
    if (handle == kCurrentProcessPseudoHandle) {
        return current_process_;
    }
    if (!IsValid(handle)) {
        return nullptr;
    }
    return objects_[Slot(handle)];
}

std::shared_ptr<WaitObject> HandleTable::GetWaitObject(Handle handle) const noexcept {
    return std::dynamic_pointer_cast<WaitObject>(Get(handle));
}

std::size_t HandleTable::OpenHandleCount() const noexcept {
    std::size_t count = 0;
    for (const auto& object : objects_) {
        count += object != nullptr ? 1U : 0U;
    }
    return count;
}

void HandleTable::Clear() noexcept {
    for (std::uint16_t i = 0; i < kMaxCount; ++i) {
        generations_[i] = static_cast<std::uint16_t>(i + 1U);
        objects_[i].reset();
    }
    next_generation_ = 1;
    next_free_slot_ = 0;
}

Kernel::Kernel(std::uint32_t process_id, std::uint32_t thread_id)
    : current_process_(std::make_shared<ProcessObject>(process_id)),
      application_resource_limit_(std::make_shared<ResourceLimitObject>()),
      current_thread_(std::make_shared<ThreadObject>(thread_id)) {
    current_thread_->status = ThreadStatus::Running;
    current_thread_->priority = kThreadPriorityDefault;
    current_thread_->tls_address = AllocateTlsAddress();
    current_thread_->guest_state.cpsr = kUserModeCpsr;
    current_thread_->guest_state.fpscr = kMainThreadInitialFpscr;
    current_thread_->guest_state.thread_pointer = current_thread_->tls_address;
    threads_.push_back(current_thread_);
    handles_.SetPseudoObjects(current_thread_, current_process_);
}

std::uint32_t Kernel::AllocateTlsAddress() noexcept {
    return kTlsAreaBase + next_tls_slot_++ * kTlsEntrySize;
}

void Kernel::InitializeGuestContext(ThreadObject& thread,
                                    std::uint32_t entry_point,
                                    std::uint32_t argument,
                                    std::uint32_t stack_top,
                                    std::uint32_t fpscr) noexcept {
    thread.guest_state = {};
    thread.guest_state.r[0] = argument;
    thread.guest_state.r[13] = stack_top;
    thread.guest_state.r[15] = entry_point & ~1U;
    thread.guest_state.cpsr =
        kUserModeCpsr | ((entry_point & 1U) << 5U);
    thread.guest_state.fpscr = fpscr;
    thread.tls_address = AllocateTlsAddress();
    thread.guest_state.thread_pointer = thread.tls_address;
}

void Kernel::SetCurrentGuestState(const a32::GuestState& state) noexcept {
    current_thread_->guest_state = state;
    current_thread_->guest_state.thread_pointer = current_thread_->tls_address;
}

const a32::GuestState& Kernel::CurrentGuestState() const noexcept {
    return current_thread_->guest_state;
}

void Kernel::ApplyPendingWakeToContext(ThreadObject& thread) noexcept {
    if (!thread.pending_wake) {
        return;
    }
    thread.guest_state.r[0] = thread.wait_result;
    if (thread.wait_index_valid) {
        thread.guest_state.r[1] =
            static_cast<std::uint32_t>(thread.wait_index);
    }
    thread.pending_wake = false;
}

bool Kernel::Reschedule(a32::GuestState& live_state) noexcept {
    const auto previous = current_thread_;
    previous->guest_state = live_state;
    previous->guest_state.thread_pointer = previous->tls_address;

    if (previous->status == ThreadStatus::Running) {
        previous->status = ThreadStatus::Ready;
    }

    const auto next = HighestPriorityReadyThread();
    if (!next) {
        return false;
    }

    current_thread_ = next;
    current_thread_->status = ThreadStatus::Running;
    handles_.SetPseudoObjects(current_thread_, current_process_);
    ApplyPendingWakeToContext(*current_thread_);
    live_state = current_thread_->guest_state;
    live_state.thread_pointer = current_thread_->tls_address;
    return current_thread_ != previous;
}

Result Kernel::ControlMemory(
    GuestMemory* memory, std::uint32_t* out_address,
    std::uint32_t addr0, std::uint32_t addr1, std::uint32_t size,
    std::uint32_t operation, std::uint32_t permissions) noexcept {
    (void)addr1;
    if (memory == nullptr || out_address == nullptr) {
        return kResultInvalidPointer;
    }
    if ((addr0 & 0xFFFU) != 0U) {
        return kResultMisalignedAddress;
    }
    if ((size & 0xFFFU) != 0U) {
        return kResultMisalignedSize;
    }
    if ((permissions & ~3U) != 0U || permissions == 0U) {
        return kResultInvalidCombination;
    }

    constexpr std::uint32_t kMemopCommit = 3U;
    constexpr std::uint32_t kMemopLinear = 0x10000U;
    const std::uint32_t operation_code = operation & 0xFFU;
    const bool linear = (operation & kMemopLinear) != 0U;
    if (operation_code != kMemopCommit) {
        // Only the two commit paths observed in LEGO startup are restored here.
        return kResultInvalidCombination;
    }

    std::uint32_t base = addr0;
    if (base == 0U) {
        base = linear ? 0x14000000U : 0x08000000U;
    }

    MemoryPermission mapped_permissions = MemoryPermission::None;
    if ((permissions & 1U) != 0U) {
        mapped_permissions = mapped_permissions | MemoryPermission::Read;
    }
    if ((permissions & 2U) != 0U) {
        mapped_permissions = mapped_permissions | MemoryPermission::Write;
    }
    if (!memory->Map(base, size, mapped_permissions)) {
        return kResultInvalidCombination;
    }

    application_resource_limit_->Reserve(
        ResourceLimitType::Commit, static_cast<std::int32_t>(size));
    *out_address = base;
    return kResultSuccess;
}

Result Kernel::CreateAddressArbiter(Handle* out_handle) noexcept {
    const Result result =
        handles_.Create(out_handle, std::make_shared<AddressArbiterObject>());
    if (result == kResultSuccess) {
        application_resource_limit_->Reserve(ResourceLimitType::AddressArbiter, 1);
    }
    return result;
}

WaitOutcome Kernel::ArbitrateAddress(
    GuestMemory* memory, Handle handle, std::uint32_t address,
    std::uint32_t type, std::int32_t value,
    std::int64_t timeout_ns) noexcept {
    if (memory == nullptr) {
        return {kResultInvalidPointer, false, -1, false};
    }
    const auto arbiter =
        std::dynamic_pointer_cast<AddressArbiterObject>(handles_.Get(handle));
    if (!arbiter) {
        return {kResultInvalidHandle, false, -1, false};
    }
    if ((address & 3U) != 0U) {
        return {kResultMisalignedAddress, false, -1, false};
    }
    if (type > 4U) {
        return {kResultInvalidEnumValue, false, -1, false};
    }

    if (type == 0U) {
        std::vector<std::size_t> matches;
        for (std::size_t index = 0; index < arbiter_waits_.size(); ++index) {
            const ArbiterWait& wait = arbiter_waits_[index];
            if (wait.arbiter == arbiter && wait.address == address) {
                matches.push_back(index);
            }
        }
        std::sort(matches.begin(), matches.end(),
                  [&](std::size_t left, std::size_t right) {
                      const auto& a = arbiter_waits_[left].thread;
                      const auto& b = arbiter_waits_[right].thread;
                      return a->priority != b->priority
                                 ? a->priority < b->priority
                                 : a->thread_id < b->thread_id;
                  });

        const std::size_t wake_count =
            value < 0 ? matches.size()
                      : std::min<std::size_t>(
                            matches.size(), static_cast<std::size_t>(value));
        std::vector<ThreadObject*> woken;
        for (std::size_t index = 0; index < wake_count; ++index) {
            ThreadObject& thread = *arbiter_waits_[matches[index]].thread;
            thread.wake_deadline_ns_.reset();
            thread.wait_result = kResultSuccess;
            thread.pending_wake = true;
            thread.status = ThreadStatus::Ready;
            woken.push_back(&thread);
        }
        arbiter_waits_.erase(
            std::remove_if(
                arbiter_waits_.begin(), arbiter_waits_.end(),
                [&](const ArbiterWait& wait) {
                    return std::find(woken.begin(), woken.end(),
                                     wait.thread.get()) != woken.end();
                }),
            arbiter_waits_.end());
        return {kResultSuccess, false, -1, false};
    }

    std::uint32_t raw = 0;
    if (!memory->Read32(address, &raw)) {
        return {kResultInvalidPointer, false, -1, false};
    }
    const std::int32_t memory_value = static_cast<std::int32_t>(raw);
    const bool should_wait = memory_value < value;
    const bool decrement = type == 2U || type == 4U;
    const bool timed = type == 3U || type == 4U;
    const Result result = timed ? kResultTimeout : kResultSuccess;

    if (!should_wait) {
        return {result, false, -1, false};
    }
    if (decrement &&
        !memory->Write32(address,
                         static_cast<std::uint32_t>(memory_value - 1))) {
        return {kResultInvalidPointer, false, -1, false};
    }

    current_thread_->status = ThreadStatus::WaitArb;
    current_thread_->wait_result = result;
    current_thread_->pending_wake = false;
    if (timed) {
        SetDeadline(*current_thread_, timeout_ns);
    } else {
        current_thread_->wake_deadline_ns_.reset();
    }
    arbiter_waits_.push_back({arbiter, current_thread_, address});
    return {result, true, -1, false};
}

void Kernel::RemoveArbiterWait(ThreadObject& thread) noexcept {
    arbiter_waits_.erase(
        std::remove_if(arbiter_waits_.begin(), arbiter_waits_.end(),
                       [&](const ArbiterWait& wait) {
                           return wait.thread.get() == &thread;
                       }),
        arbiter_waits_.end());
}

Result Kernel::GetProcessId(std::uint32_t* out_process_id,
                            Handle process_handle) noexcept {
    if (out_process_id == nullptr) {
        return kResultInvalidPointer;
    }
    const auto process =
        std::dynamic_pointer_cast<ProcessObject>(handles_.Get(process_handle));
    if (!process) {
        return kResultInvalidHandle;
    }
    *out_process_id = process->process_id;
    return kResultSuccess;
}

Result Kernel::GetResourceLimit(Handle* out_handle,
                                Handle process_handle) noexcept {
    const auto process =
        std::dynamic_pointer_cast<ProcessObject>(handles_.Get(process_handle));
    if (!process) {
        return kResultInvalidHandle;
    }
    return handles_.Create(out_handle, application_resource_limit_);
}

Result Kernel::GetResourceLimitValues(
    GuestMemory* memory, bool current_values, std::uint32_t values_address,
    Handle resource_limit_handle, std::uint32_t names_address,
    std::uint32_t name_count) noexcept {
    if (memory == nullptr) {
        return kResultInvalidPointer;
    }
    const auto resource =
        std::dynamic_pointer_cast<ResourceLimitObject>(
            handles_.Get(resource_limit_handle));
    if (!resource) {
        return kResultInvalidHandle;
    }

    for (std::uint32_t index = 0; index < name_count; ++index) {
        std::uint32_t raw_name = 0;
        if (!memory->Read32(names_address + index * 4U, &raw_name)) {
            return kResultInvalidPointer;
        }
        if (raw_name >= static_cast<std::uint32_t>(ResourceLimitType::Max)) {
            return kResultInvalidEnumValue;
        }
        const auto type = static_cast<ResourceLimitType>(raw_name);
        const std::int64_t value =
            current_values ? resource->Current(type) : resource->Limit(type);
        std::uint32_t fault = 0;
        if (!memory->Write64(values_address + index * 8U,
                             static_cast<std::uint64_t>(value), &fault)) {
            return kResultInvalidPointer;
        }
    }
    return kResultSuccess;
}

Result Kernel::DuplicateHandle(Handle* out_handle, Handle handle) noexcept {
    return handles_.Duplicate(out_handle, handle);
}

Result Kernel::CloseHandle(Handle handle) noexcept {
    return handles_.Close(handle);
}

Result Kernel::CreateEvent(Handle* out_handle, std::uint32_t reset_type) noexcept {
    return handles_.Create(out_handle,
                           std::make_shared<EventObject>(static_cast<ResetType>(reset_type)));
}

Result Kernel::SignalEvent(Handle handle) noexcept {
    const auto event = std::dynamic_pointer_cast<EventObject>(handles_.Get(handle));
    if (!event) {
        return kResultInvalidHandle;
    }
    event->Signal();
    TryWakeWaitingThreads();
    if (event->reset_type() == ResetType::Pulse) {
        event->Clear();
    }
    return kResultSuccess;
}

Result Kernel::ClearEvent(Handle handle) noexcept {
    const auto event = std::dynamic_pointer_cast<EventObject>(handles_.Get(handle));
    if (!event) {
        return kResultInvalidHandle;
    }
    event->Clear();
    return kResultSuccess;
}

Result Kernel::CreateMutex(Handle* out_handle, bool initial_locked) noexcept {
    auto mutex = std::make_shared<MutexObject>();
    if (initial_locked) {
        mutex->Acquire(*current_thread_);
    }
    return handles_.Create(out_handle, std::move(mutex));
}

Result Kernel::ReleaseMutex(Handle handle) noexcept {
    const auto mutex = std::dynamic_pointer_cast<MutexObject>(handles_.Get(handle));
    if (!mutex) {
        return kResultInvalidHandle;
    }
    const Result result = mutex->Release(*current_thread_);
    if (result == kResultSuccess) {
        TryWakeWaitingThreads();
    }
    return result;
}

Result Kernel::CreateSemaphore(Handle* out_handle, std::int32_t initial_count,
                               std::int32_t max_count) noexcept {
    if (initial_count > max_count) {
        return kResultInvalidCombinationKernel;
    }
    return handles_.Create(out_handle,
                           std::make_shared<SemaphoreObject>(initial_count, max_count));
}

Result Kernel::ReleaseSemaphore(std::int32_t* out_count, Handle handle,
                                std::int32_t release_count) noexcept {
    const auto semaphore =
        std::dynamic_pointer_cast<SemaphoreObject>(handles_.Get(handle));
    if (!semaphore) {
        return kResultInvalidHandle;
    }
    const Result result = semaphore->Release(out_count, release_count);
    if (result == kResultSuccess) {
        TryWakeWaitingThreads();
    }
    return result;
}

void Kernel::SetDeadline(ThreadObject& thread, std::int64_t timeout_ns) noexcept {
    if (timeout_ns < 0) {
        thread.wake_deadline_ns_.reset();
        return;
    }
    const std::uint64_t delta = static_cast<std::uint64_t>(timeout_ns);
    if (delta > std::numeric_limits<std::uint64_t>::max() - now_ns_) {
        thread.wake_deadline_ns_ = std::numeric_limits<std::uint64_t>::max();
    } else {
        thread.wake_deadline_ns_ = now_ns_ + delta;
    }
}

void Kernel::ClearWait(ThreadObject& thread) noexcept {
    thread.wait_objects_.clear();
    thread.wake_deadline_ns_.reset();
    thread.wait_reports_index_ = false;
}

void Kernel::WakeThread(ThreadObject& thread, Result result, std::int32_t index,
                        bool index_valid) noexcept {
    ClearWait(thread);
    thread.wait_result = result;
    thread.wait_index = index;
    thread.wait_index_valid = index_valid;
    thread.pending_wake = true;
    if (thread.status != ThreadStatus::Dead) {
        thread.status = ThreadStatus::Ready;
    }
}

WaitOutcome Kernel::WaitSynchronization1(Handle handle,
                                         std::int64_t timeout_ns) noexcept {
    const auto object = handles_.GetWaitObject(handle);
    if (!object) {
        return {kResultInvalidHandle, false, -1, false};
    }

    if (!object->ShouldWait(*current_thread_)) {
        object->Acquire(*current_thread_);
        return {kResultSuccess, false, -1, false};
    }
    if (timeout_ns == 0) {
        return {kResultTimeout, false, -1, false};
    }

    current_thread_->status = ThreadStatus::WaitSynchAny;
    current_thread_->wait_objects_ = {object};
    current_thread_->wait_result = kResultTimeout;
    current_thread_->wait_index = -1;
    current_thread_->wait_index_valid = false;
    current_thread_->wait_reports_index_ = false;
    current_thread_->pending_wake = false;
    SetDeadline(*current_thread_, timeout_ns);
    return {kResultTimeout, true, -1, false};
}

WaitOutcome Kernel::WaitSynchronizationN(std::span<const Handle> handles,
                                         bool wait_all,
                                         std::int64_t timeout_ns) noexcept {
    std::vector<std::shared_ptr<WaitObject>> objects;
    objects.reserve(handles.size());
    for (const Handle handle : handles) {
        auto object = handles_.GetWaitObject(handle);
        if (!object) {
            return {kResultInvalidHandle, false, -1, false};
        }
        objects.push_back(std::move(object));
    }

    if (wait_all) {
        const bool all_ready =
            std::all_of(objects.begin(), objects.end(),
                        [&](const auto& object) {
                            return !object->ShouldWait(*current_thread_);
                        });
        if (all_ready) {
            for (auto& object : objects) {
                object->Acquire(*current_thread_);
            }
            return {kResultSuccess, false, -1, false};
        }
    } else {
        for (std::size_t index = 0; index < objects.size(); ++index) {
            if (!objects[index]->ShouldWait(*current_thread_)) {
                objects[index]->Acquire(*current_thread_);
                return {kResultSuccess, false, static_cast<std::int32_t>(index), true};
            }
        }
    }

    if (timeout_ns == 0) {
        return {kResultTimeout, false, -1, false};
    }

    current_thread_->status =
        wait_all ? ThreadStatus::WaitSynchAll : ThreadStatus::WaitSynchAny;
    current_thread_->wait_objects_ = std::move(objects);
    current_thread_->wait_result = kResultTimeout;
    current_thread_->wait_index = -1;
    current_thread_->wait_index_valid = false;
    current_thread_->wait_reports_index_ = !wait_all;
    current_thread_->pending_wake = false;
    SetDeadline(*current_thread_, timeout_ns);
    return {kResultTimeout, true, -1, !wait_all};
}

Result Kernel::CreateThread(Handle* out_handle, std::uint32_t entry_point,
                            std::uint32_t argument, std::uint32_t stack_top,
                            std::uint32_t priority,
                            std::int32_t processor_id) noexcept {
    if (priority > kThreadPriorityLowest) {
        return kResultOutOfRange;
    }
    if (processor_id == kThreadProcessorDefault ||
        processor_id == kThreadProcessorAll) {
        processor_id = 0;
    } else if (processor_id < 0 || processor_id > 3) {
        return kResultOutOfRange;
    }

    auto thread = std::make_shared<ThreadObject>(next_thread_id_++);
    thread->status = ThreadStatus::Ready;
    thread->entry_point = entry_point;
    thread->argument = argument;
    thread->stack_top = stack_top;
    thread->priority = priority;
    thread->processor_id = processor_id;
    InitializeGuestContext(*thread, entry_point, argument, stack_top,
                           kThreadInitialFpscr);

    Handle handle = 0;
    const Result result = handles_.Create(&handle, thread);
    if (result != kResultSuccess) {
        return result;
    }
    threads_.push_back(std::move(thread));
    *out_handle = handle;
    return kResultSuccess;
}

void Kernel::ExitCurrentThread() noexcept {
    current_thread_->status = ThreadStatus::Dead;
    RemoveArbiterWait(*current_thread_);
    ClearWait(*current_thread_);
    current_thread_->pending_wake = false;
    TryWakeWaitingThreads();
}

std::shared_ptr<ThreadObject> Kernel::HighestPriorityReadyThread() const noexcept {
    std::shared_ptr<ThreadObject> best;
    for (const auto& thread : threads_) {
        if (thread->status != ThreadStatus::Ready) {
            continue;
        }
        if (!best || thread->priority < best->priority ||
            (thread->priority == best->priority &&
             thread->thread_id < best->thread_id)) {
            best = thread;
        }
    }
    return best;
}

bool Kernel::SleepCurrentThread(std::int64_t nanoseconds) noexcept {
    if (nanoseconds == 0 && HighestPriorityReadyThread() == nullptr) {
        return false;
    }
    current_thread_->status = ThreadStatus::WaitSleep;
    current_thread_->pending_wake = false;
    SetDeadline(*current_thread_, nanoseconds);
    return true;
}

void Kernel::TryWakeWaitingThreads() noexcept {
    std::vector<std::shared_ptr<ThreadObject>> candidates;
    for (const auto& thread : threads_) {
        if (thread->status == ThreadStatus::WaitSynchAny ||
            thread->status == ThreadStatus::WaitSynchAll) {
            candidates.push_back(thread);
        }
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const auto& left, const auto& right) {
                  if (left->priority != right->priority) {
                      return left->priority < right->priority;
                  }
                  return left->thread_id < right->thread_id;
              });

    for (const auto& thread : candidates) {
        if (thread->status == ThreadStatus::WaitSynchAll) {
            const bool all_ready =
                std::all_of(thread->wait_objects_.begin(),
                            thread->wait_objects_.end(),
                            [&](const auto& object) {
                                return !object->ShouldWait(*thread);
                            });
            if (!all_ready) {
                continue;
            }
            for (auto& object : thread->wait_objects_) {
                object->Acquire(*thread);
            }
            WakeThread(*thread, kResultSuccess, -1, false);
            continue;
        }

        for (std::size_t index = 0; index < thread->wait_objects_.size(); ++index) {
            auto& object = thread->wait_objects_[index];
            if (!object->ShouldWait(*thread)) {
                object->Acquire(*thread);
                const bool report_index = thread->wait_reports_index_;
                WakeThread(*thread, kResultSuccess,
                           static_cast<std::int32_t>(index), report_index);
                break;
            }
        }
    }
}

void Kernel::AdvanceTime(std::uint64_t nanoseconds) noexcept {
    if (nanoseconds > std::numeric_limits<std::uint64_t>::max() - now_ns_) {
        now_ns_ = std::numeric_limits<std::uint64_t>::max();
    } else {
        now_ns_ += nanoseconds;
    }

    for (const auto& thread : threads_) {
        if (!thread->wake_deadline_ns_ ||
            now_ns_ < *thread->wake_deadline_ns_) {
            continue;
        }

        if (thread->status == ThreadStatus::WaitSleep) {
            thread->wake_deadline_ns_.reset();
            thread->status = ThreadStatus::Ready;
            continue;
        }

        if (thread->status == ThreadStatus::WaitArb) {
            RemoveArbiterWait(*thread);
            WakeThread(*thread, kResultTimeout, -1, false);
            continue;
        }

        if (thread->status == ThreadStatus::WaitSynchAny ||
            thread->status == ThreadStatus::WaitSynchAll) {
            const bool report_index = thread->wait_reports_index_;
            WakeThread(*thread, kResultTimeout, -1, report_index);
        }
    }
}

bool Kernel::ConsumeCurrentThreadWake(Result* result, std::int32_t* index,
                                      bool* index_valid) noexcept {
    if (!current_thread_->pending_wake) {
        return false;
    }
    if (result != nullptr) {
        *result = current_thread_->wait_result;
    }
    if (index != nullptr) {
        *index = current_thread_->wait_index;
    }
    if (index_valid != nullptr) {
        *index_valid = current_thread_->wait_index_valid;
    }
    current_thread_->pending_wake = false;
    return true;
}

}  // namespace lego::ctr
