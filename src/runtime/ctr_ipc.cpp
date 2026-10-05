#include "runtime/ctr_ipc.h"

#include <algorithm>
#include <cstring>

namespace lego::ctr {
namespace {

class SrvService final : public IpcService {
public:
    Result Handle(IpcRouter& router, Kernel& kernel, GuestMemory&,
                  ThreadObject&, IpcCommandBuffer& command) override {
        const std::uint16_t command_id = IpcCommandId(command[0]);
        switch (command_id) {
        case 0x0001: {
            // RegisterClient: request header 0x00010002 followed by a
            // CallingPid descriptor and translated caller PID.
            if (IpcTranslateWords(command[0]) != 2U ||
                command[1] != IpcCallingPidDesc()) {
                command.fill(0);
                command[0] = IpcMakeHeader(0x0001, 1, 0);
                command[1] = kResultInvalidPointer;
                return kResultSuccess;
            }
            command.fill(0);
            command[0] = IpcMakeHeader(0x0001, 1, 0);
            command[1] = kResultSuccess;
            return kResultSuccess;
        }

        case 0x0005: {
            // GetServiceHandle:
            // words 1-2 = 8-byte name, word 3 = name length, word 4 = flags.
            if (IpcNormalWords(command[0]) < 4U) {
                command.fill(0);
                command[0] = IpcMakeHeader(0x0005, 1, 0);
                command[1] = kResultInvalidServiceNameSize;
                return kResultSuccess;
            }

            char raw_name[8]{};
            std::memcpy(raw_name, &command[1], sizeof(raw_name));
            const std::uint32_t name_length = command[3];
            if (name_length == 0U || name_length > sizeof(raw_name)) {
                command.fill(0);
                command[0] = IpcMakeHeader(0x0005, 1, 0);
                command[1] = kResultInvalidServiceNameSize;
                return kResultSuccess;
            }

            const std::string name(raw_name, raw_name + name_length);
            if (name.find('\0') != std::string::npos) {
                command.fill(0);
                command[0] = IpcMakeHeader(0x0005, 1, 0);
                command[1] = kResultServiceNameContainsNul;
                return kResultSuccess;
            }

            ::lego::ctr::Handle service_handle = 0;
            const Result result =
                router.ConnectToService(kernel, name, &service_handle);
            command.fill(0);
            if (result == kResultSuccess) {
                command[0] = IpcMakeHeader(0x0005, 1, 2);
                command[1] = kResultSuccess;
                command[2] = IpcMoveHandleDesc();
                command[3] = service_handle;
            } else {
                command[0] = IpcMakeHeader(0x0005, 1, 0);
                command[1] = result;
            }
            return kResultSuccess;
        }

        case 0x0009:  // Subscribe (stubbed-success in the historical HLE)
        case 0x000A:  // Unsubscribe
        case 0x000C:  // PublishToSubscriber
            command.fill(0);
            command[0] = IpcMakeHeader(command_id, 1, 0);
            command[1] = kResultSuccess;
            return kResultSuccess;

        default:
            // Preserve the request on an unimplemented service command and let
            // SendSyncRequest return an explicit error.
            return kResultServiceNotRegistered;
        }
    }
};

}  // namespace

IpcRouter::IpcRouter() : srv_(std::make_shared<SrvService>()) {}

Result IpcRouter::RegisterService(std::string name,
                                  std::shared_ptr<IpcService> service) {
    if (name.empty() || name.size() > 8U) {
        return kResultInvalidServiceNameSize;
    }
    if (name.find('\0') != std::string::npos) {
        return kResultServiceNameContainsNul;
    }
    if (!service) {
        return kResultInvalidPointer;
    }
    services_[std::move(name)] = std::move(service);
    return kResultSuccess;
}

bool IpcRouter::HasService(std::string_view name) const {
    return services_.find(std::string(name)) != services_.end();
}

bool IpcRouter::ReadCString(GuestMemory& memory, std::uint32_t address,
                            std::size_t max_length, std::string* out) const {
    if (out == nullptr) {
        return false;
    }
    out->clear();
    for (std::size_t index = 0; index <= max_length; ++index) {
        std::uint8_t value = 0;
        if (!memory.Read8(address + static_cast<std::uint32_t>(index), &value)) {
            return false;
        }
        if (value == 0) {
            return true;
        }
        if (index == max_length) {
            return false;
        }
        out->push_back(static_cast<char>(value));
    }
    return false;
}

bool IpcRouter::ReadCommandBuffer(GuestMemory& memory,
                                  const ThreadObject& thread,
                                  IpcCommandBuffer* out) const {
    if (out == nullptr) {
        return false;
    }
    const std::uint32_t base =
        thread.tls_address + kIpcCommandBufferOffset;
    for (std::size_t index = 0; index < out->size(); ++index) {
        if (!memory.Read32(base + static_cast<std::uint32_t>(index * 4U),
                           &(*out)[index])) {
            return false;
        }
    }
    return true;
}

bool IpcRouter::WriteCommandBuffer(GuestMemory& memory,
                                   const ThreadObject& thread,
                                   const IpcCommandBuffer& command) const {
    const std::uint32_t base =
        thread.tls_address + kIpcCommandBufferOffset;
    for (std::size_t index = 0; index < command.size(); ++index) {
        if (!memory.Write32(base + static_cast<std::uint32_t>(index * 4U),
                            command[index])) {
            return false;
        }
    }
    return true;
}

Result IpcRouter::ConnectToPort(Kernel& kernel, GuestMemory& memory,
                                std::uint32_t port_name_address,
                                Handle* out_handle) {
    if (!memory.IsReadable(port_name_address, 1U)) {
        return kResultNotFound;
    }

    std::string port_name;
    bool terminated = false;
    for (std::size_t index = 0; index < 12U; ++index) {
        std::uint8_t value = 0;
        if (!memory.Read8(port_name_address + static_cast<std::uint32_t>(index),
                          &value)) {
            return kResultNotFound;
        }
        if (value == 0U) {
            terminated = true;
            break;
        }
        port_name.push_back(static_cast<char>(value));
    }
    if (!terminated || port_name.size() > 11U) {
        return kResultPortNameTooLong;
    }
    if (port_name != "srv:") {
        return kResultNotFound;
    }
    return kernel.handles().Create(
        out_handle, std::make_shared<ClientSessionObject>("srv:", srv_));
}

Result IpcRouter::ConnectToService(Kernel& kernel, std::string_view name,
                                   Handle* out_handle) {
    last_lookup_name_ = name;
    const auto it = services_.find(std::string(name));
    if (it == services_.end()) {
        return kResultServiceNotRegistered;
    }
    auto handler = it->second->CreateSessionHandler();
    if (!handler) handler = it->second;
    return kernel.handles().Create(
        out_handle,
        std::make_shared<ClientSessionObject>(std::string(name), std::move(handler)));
}

std::optional<Result> IpcRouter::SendSyncRequest(Kernel& kernel, GuestMemory& memory,
                                  Handle handle) {
    unsupported_request_ = false;
    const auto session =
        std::dynamic_pointer_cast<ClientSessionObject>(kernel.handles().Get(handle));
    if (!session || !session->service) {
        return kResultInvalidHandle;
    }

    // Preflight the entire response area before any handler allocates handles
    // or mutates service state. Readability alone is insufficient.
    const auto command_address = std::uint64_t(kernel.current_thread()->tls_address) +
                                 kIpcCommandBufferOffset;
    if (command_address + sizeof(IpcCommandBuffer) > 0x100000000ULL ||
        !memory.IsWritable(static_cast<std::uint32_t>(command_address),
                           sizeof(IpcCommandBuffer))) {
        return kResultInvalidPointer;
    }
    IpcCommandBuffer command{};
    if (!ReadCommandBuffer(memory, *kernel.current_thread(), &command)) {
        return kResultInvalidPointer;
    }

    last_session_name_ = session->name;
    last_lookup_name_.clear();
    last_request_ = command;
    if (!session->service->CanHandle(command)) {
        unsupported_request_ = true;
        return std::nullopt;
    }
    const Result dispatch_result =
        session->service->Handle(*this, kernel, memory,
                                 *kernel.current_thread(), command);
    if (dispatch_result != kResultSuccess) {
        return dispatch_result;
    }

    if (!WriteCommandBuffer(memory, *kernel.current_thread(), command)) {
        return kResultInvalidPointer;
    }
    return kResultSuccess;
}

}  // namespace lego::ctr
