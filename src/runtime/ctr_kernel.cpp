#include "runtime/ctr_kernel.h"

#include <utility>

namespace lego::ctr {

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
      current_thread_(std::make_shared<ThreadObject>(thread_id)) {
    handles_.SetPseudoObjects(current_thread_, current_process_);
}

Result Kernel::DuplicateHandle(Handle* out_handle, Handle handle) noexcept {
    return handles_.Duplicate(out_handle, handle);
}

Result Kernel::CloseHandle(Handle handle) noexcept {
    return handles_.Close(handle);
}

}  // namespace lego::ctr
