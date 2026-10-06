#pragma once
#include <algorithm>
#include <array>
#include <cstring>
#include <span>
#include <stdexcept>
#include "runtime/ctr_kernel.h"

namespace lego::ctr {
// Bounded service-allocated shared page: no owner-process allocation, automatic
// address selection or physical BASE-region allocator is claimed in this slice.
// Shared bytes and reservation epochs belong to the object, not a guest mapping.
class ServiceSharedMemoryObject final : public KernelObject {
public:
    static constexpr std::uint32_t kSize = 0x1000;
    // Service-owned page: immutable client permissions. Existing GSP defaults
    // stay read/write; HID exposes read-only client views of this same backing.
    explicit ServiceSharedMemoryObject(std::uint32_t client_permissions = 3)
        : KernelObject(Type::SharedMemory), client_permissions_(client_permissions) {
        if (client_permissions != 1 && client_permissions != 3)
            throw std::invalid_argument("unsupported service-page client permissions");
    }
    [[nodiscard]] std::span<const std::uint8_t> bytes() const noexcept { return bytes_; }
    [[nodiscard]] std::uint32_t size() const noexcept { return kSize; }
    [[nodiscard]] std::uint32_t owner_permissions() const noexcept { return 3; }
    [[nodiscard]] std::uint32_t other_permissions() const noexcept { return client_permissions_; }
    [[nodiscard]] std::uint64_t Epoch(std::uint32_t offset) const noexcept {
        return offset < kSize ? epochs_[offset / 8] : 0;
    }
    bool Write(std::uint32_t offset, std::span<const std::uint8_t> source) noexcept {
        if (offset > kSize || source.size() > kSize - offset) return false;
        if (source.empty()) return true;
        std::memmove(bytes_.data()+offset,source.data(),source.size());
        for (std::size_t n=offset/8; n<=(offset+source.size()-1)/8; ++n)
            epochs_[n]=next_epoch_++;
        return true;
    }
private:
    const std::uint32_t client_permissions_;
    std::array<std::uint8_t,kSize> bytes_{};
    std::array<std::uint64_t,kSize/8> epochs_{};
    std::uint64_t next_epoch_{1};
};
} // namespace lego::ctr
