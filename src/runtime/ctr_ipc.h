#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "runtime/ctr_kernel.h"
#include "runtime/ctr_memory.h"

namespace lego::ctr {

inline constexpr std::size_t kIpcCommandBufferWords = 0x100U / sizeof(std::uint32_t);
inline constexpr std::uint32_t kIpcCommandBufferOffset = 0x80U;

inline constexpr Result kResultServiceNotRegistered = 0xD0406401U;
inline constexpr Result kResultInvalidServiceNameSize = 0xD9006405U;
inline constexpr Result kResultServiceNameContainsNul = 0xD9006407U;

constexpr std::uint32_t IpcMakeHeader(std::uint16_t command_id,
                                      std::uint32_t normal_words,
                                      std::uint32_t translate_words) noexcept {
    return static_cast<std::uint32_t>(command_id) << 16U |
           (normal_words & 0x3FU) << 6U |
           (translate_words & 0x3FU);
}

constexpr std::uint16_t IpcCommandId(std::uint32_t header) noexcept {
    return static_cast<std::uint16_t>(header >> 16U);
}

constexpr std::uint32_t IpcNormalWords(std::uint32_t header) noexcept {
    return (header >> 6U) & 0x3FU;
}

constexpr std::uint32_t IpcTranslateWords(std::uint32_t header) noexcept {
    return header & 0x3FU;
}

constexpr std::uint32_t IpcMoveHandleDesc(std::uint32_t count = 1U) noexcept {
    return 0x10U | ((count - 1U) << 26U);
}

constexpr std::uint32_t IpcCopyHandleDesc(std::uint32_t count = 1U) noexcept {
    return ((count - 1U) << 26U);
}

constexpr std::uint32_t IpcCallingPidDesc() noexcept {
    return 0x20U;
}

using IpcCommandBuffer = std::array<std::uint32_t, kIpcCommandBufferWords>;

class IpcRouter;

class IpcService {
public:
    virtual ~IpcService() = default;
    virtual Result Handle(IpcRouter& router, Kernel& kernel, GuestMemory& memory,
                          ThreadObject& thread, IpcCommandBuffer& command) = 0;
};

class ClientSessionObject final : public KernelObject {
public:
    ClientSessionObject(std::string name, std::shared_ptr<IpcService> service)
        : KernelObject(Type::Session), name(std::move(name)),
          service(std::move(service)) {}

    std::string name;
    std::shared_ptr<IpcService> service;
};

class IpcRouter final {
public:
    IpcRouter();

    Result RegisterService(std::string name, std::shared_ptr<IpcService> service);
    Result ConnectToPort(Kernel& kernel, GuestMemory& memory,
                         std::uint32_t port_name_address,
                         Handle* out_handle);
    Result ConnectToService(Kernel& kernel, std::string_view name,
                            Handle* out_handle);
    Result SendSyncRequest(Kernel& kernel, GuestMemory& memory, Handle handle);

    [[nodiscard]] bool HasService(std::string_view name) const;

    // Last guest request, captured before handlers replace it with a response.
    // Diagnostics only: these fields do not alter IPC or service availability.
    [[nodiscard]] const std::string& last_session_name() const noexcept { return last_session_name_; }
    [[nodiscard]] const std::string& last_lookup_name() const noexcept { return last_lookup_name_; }
    [[nodiscard]] const IpcCommandBuffer& last_request() const noexcept { return last_request_; }

private:
    bool ReadCString(GuestMemory& memory, std::uint32_t address,
                     std::size_t max_length, std::string* out) const;
    bool ReadCommandBuffer(GuestMemory& memory, const ThreadObject& thread,
                           IpcCommandBuffer* out) const;
    bool WriteCommandBuffer(GuestMemory& memory, const ThreadObject& thread,
                            const IpcCommandBuffer& command) const;

    std::string last_session_name_;
    std::string last_lookup_name_;
    IpcCommandBuffer last_request_{};
    std::shared_ptr<IpcService> srv_;
    std::unordered_map<std::string, std::shared_ptr<IpcService>> services_;
};

}  // namespace lego::ctr
