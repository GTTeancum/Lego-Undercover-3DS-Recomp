#pragma once
#include <cstdint>
#include <span>
namespace lego::ctr {
// Retained device-owned bytes. All operations are bounded and allocation-free.
// Read permission does not imply initialized content; CanRead checks provenance.
class DeviceMemory {
public:
    virtual ~DeviceMemory() = default;
    virtual std::uint32_t size() const noexcept = 0;
    virtual bool CanRead(std::uint32_t, std::uint32_t) const noexcept = 0;
    virtual bool CanWrite(std::uint32_t, std::uint32_t) const noexcept = 0;
    virtual bool Read(std::uint32_t, std::span<std::uint8_t>) const noexcept = 0;
    virtual bool Write(std::uint32_t, std::span<const std::uint8_t>) noexcept = 0;
    virtual std::uint64_t Epoch(std::uint32_t) const noexcept = 0;
};
} // namespace lego::ctr
