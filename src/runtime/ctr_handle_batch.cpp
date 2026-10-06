#include "runtime/ctr_kernel.h"
#include <algorithm>

namespace lego::ctr {
Result HandleTable::CreateCopies(std::span<const std::shared_ptr<KernelObject>> objects,
                                 std::span<Handle> out_handles) noexcept {
    if (objects.empty() || objects.size() != out_handles.size() || objects.size() > kMaxCount)
        return kResultInvalidPointer;
    for (const auto& object : objects) if (!object) return kResultInvalidHandle;
    if (objects.size() > kMaxCount - OpenHandleCount()) return kResultOutOfHandles;
    // Fixed-size staging has no allocation and cannot expose a partial batch.
    // The host kernel is serialized; there is no concurrent handle-table mutation.
    HandleTable staged = *this;
    std::array<Handle,kMaxCount> handles{};
    for (std::size_t i = 0; i < objects.size(); ++i) {
        const auto result = staged.Create(&handles[i], objects[i]);
        if (result != kResultSuccess) return result;
    }
    *this = std::move(staged);
    std::copy_n(handles.begin(), objects.size(), out_handles.begin());
    return kResultSuccess;
}
} // namespace lego::ctr
