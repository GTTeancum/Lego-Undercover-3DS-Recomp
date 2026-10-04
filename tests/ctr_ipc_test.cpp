#include "runtime/ctr_ipc.h"
#include "runtime/ctr_svc_bridge.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string_view>

namespace {

int failures = 0;

#define CHECK(expr)                                                                    \
    do {                                                                               \
        if (!(expr)) {                                                                 \
            std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << ": " #expr "\n"; \
            ++failures;                                                                \
        }                                                                              \
    } while (0)

using lego::ctr::GuestMemory;
using lego::ctr::Handle;
using lego::ctr::IpcCommandBuffer;
using lego::ctr::IpcRouter;
using lego::ctr::IpcService;
using lego::ctr::Kernel;
using lego::ctr::MemoryPermission;
using lego::ctr::Result;
using lego::ctr::SvcBridge;

oot3d::recomp::a32::ExecutionResult Svc(std::uint32_t id,
                                        std::uint32_t pc = 0x00100000U) {
    return {
        oot3d::recomp::a32::ExitKind::Svc,
        pc,
        oot3d::recomp::a32::FallbackReason::None,
        id,
    };
}

std::uint32_t CommandAddress(const Kernel& kernel) {
    return kernel.current_thread()->tls_address +
           lego::ctr::kIpcCommandBufferOffset;
}

void ClearCommand(GuestMemory& memory, const Kernel& kernel) {
    const std::uint32_t base = CommandAddress(kernel);
    for (std::size_t index = 0; index < lego::ctr::kIpcCommandBufferWords; ++index) {
        CHECK(memory.Write32(base + static_cast<std::uint32_t>(index * 4U), 0U));
    }
}

void WriteCommandWord(GuestMemory& memory, const Kernel& kernel,
                      std::size_t index, std::uint32_t value) {
    CHECK(memory.Write32(CommandAddress(kernel) +
                             static_cast<std::uint32_t>(index * 4U),
                         value));
}

std::uint32_t ReadCommandWord(GuestMemory& memory, const Kernel& kernel,
                              std::size_t index) {
    std::uint32_t value = 0;
    CHECK(memory.Read32(CommandAddress(kernel) +
                            static_cast<std::uint32_t>(index * 4U),
                        &value));
    return value;
}

void WriteCString(GuestMemory& memory, std::uint32_t address,
                  std::string_view text) {
    for (std::size_t index = 0; index < text.size(); ++index) {
        CHECK(memory.Write8(address + static_cast<std::uint32_t>(index),
                            static_cast<std::uint8_t>(text[index])));
    }
    CHECK(memory.Write8(address + static_cast<std::uint32_t>(text.size()), 0U));
}

void WriteServiceNameWords(IpcCommandBuffer* command, std::string_view name) {
    std::array<char, 8> bytes{};
    std::memcpy(bytes.data(), name.data(), name.size());
    std::memcpy(&(*command)[1], bytes.data(), bytes.size());
}

class EchoService final : public IpcService {
public:
    Result Handle(IpcRouter&, Kernel&, GuestMemory&,
                  lego::ctr::ThreadObject&, IpcCommandBuffer& command) override {
        const std::uint16_t command_id = lego::ctr::IpcCommandId(command[0]);
        if (command_id != 0x0010U ||
            lego::ctr::IpcNormalWords(command[0]) < 1U) {
            return lego::ctr::kResultNotFound;
        }
        const std::uint32_t input = command[1];
        command.fill(0);
        command[0] = lego::ctr::IpcMakeHeader(0x0010U, 2U, 0U);
        command[1] = lego::ctr::kResultSuccess;
        command[2] = input ^ 0xA5A5A5A5U;
        ++calls;
        return lego::ctr::kResultSuccess;
    }

    int calls{};
};

void TestConnectSrvAndRegisterClient() {
    Kernel kernel(0x42U, 1U);
    GuestMemory memory;
    CHECK(memory.EnsureTlsMappings(kernel));
    CHECK(memory.Map(0x08000000U, 0x1000U,
                     MemoryPermission::Read | MemoryPermission::Write));

    IpcRouter router;
    SvcBridge bridge(kernel, &router);
    oot3d::recomp::a32::GuestState state{};

    WriteCString(memory, 0x08000000U, "srv:");
    state.r[1] = 0x08000000U;

    auto result =
        bridge.Handle(Svc(lego::ctr::kSvcConnectToPort), state, &memory);
    CHECK(result.kind == oot3d::recomp::a32::ExitKind::Fallthrough);
    CHECK(state.r[0] == lego::ctr::kResultSuccess);
    const Handle srv_handle = state.r[1];
    CHECK(kernel.handles().IsValid(srv_handle));

    ClearCommand(memory, kernel);
    WriteCommandWord(memory, kernel, 0,
                     lego::ctr::IpcMakeHeader(0x0001U, 0U, 2U));
    WriteCommandWord(memory, kernel, 1, lego::ctr::IpcCallingPidDesc());
    WriteCommandWord(memory, kernel, 2, kernel.current_process()->process_id);

    state.r[0] = srv_handle;
    result =
        bridge.Handle(Svc(lego::ctr::kSvcSendSyncRequest), state, &memory);
    CHECK(result.kind == oot3d::recomp::a32::ExitKind::Fallthrough);
    CHECK(state.r[0] == lego::ctr::kResultSuccess);
    CHECK(ReadCommandWord(memory, kernel, 0) ==
          lego::ctr::IpcMakeHeader(0x0001U, 1U, 0U));
    CHECK(ReadCommandWord(memory, kernel, 1) ==
          lego::ctr::kResultSuccess);
}

void TestSrvGetServiceHandleAndSessionDispatch() {
    Kernel kernel(7U, 1U);
    GuestMemory memory;
    CHECK(memory.EnsureTlsMappings(kernel));
    CHECK(memory.Map(0x08000000U, 0x1000U,
                     MemoryPermission::Read | MemoryPermission::Write));

    IpcRouter router;
    auto echo = std::make_shared<EchoService>();
    CHECK(router.RegisterService("echo:", echo) == lego::ctr::kResultSuccess);

    SvcBridge bridge(kernel, &router);
    oot3d::recomp::a32::GuestState state{};

    WriteCString(memory, 0x08000000U, "srv:");
    state.r[1] = 0x08000000U;
    bridge.Handle(Svc(lego::ctr::kSvcConnectToPort), state, &memory);
    CHECK(state.r[0] == lego::ctr::kResultSuccess);
    const Handle srv_handle = state.r[1];

    ClearCommand(memory, kernel);
    IpcCommandBuffer request{};
    request[0] = lego::ctr::IpcMakeHeader(0x0005U, 4U, 0U);
    WriteServiceNameWords(&request, "echo:");
    request[3] = 5U;
    request[4] = 1U;
    for (std::size_t index = 0; index <= 4; ++index) {
        WriteCommandWord(memory, kernel, index, request[index]);
    }

    state.r[0] = srv_handle;
    bridge.Handle(Svc(lego::ctr::kSvcSendSyncRequest), state, &memory);
    CHECK(state.r[0] == lego::ctr::kResultSuccess);

    CHECK(ReadCommandWord(memory, kernel, 0) ==
          lego::ctr::IpcMakeHeader(0x0005U, 1U, 2U));
    CHECK(ReadCommandWord(memory, kernel, 1) ==
          lego::ctr::kResultSuccess);
    CHECK(ReadCommandWord(memory, kernel, 2) ==
          lego::ctr::IpcMoveHandleDesc());

    const Handle echo_handle = ReadCommandWord(memory, kernel, 3);
    CHECK(kernel.handles().IsValid(echo_handle));

    ClearCommand(memory, kernel);
    WriteCommandWord(memory, kernel, 0,
                     lego::ctr::IpcMakeHeader(0x0010U, 1U, 0U));
    WriteCommandWord(memory, kernel, 1, 0x12345678U);

    state.r[0] = echo_handle;
    bridge.Handle(Svc(lego::ctr::kSvcSendSyncRequest), state, &memory);
    CHECK(state.r[0] == lego::ctr::kResultSuccess);
    CHECK(ReadCommandWord(memory, kernel, 0) ==
          lego::ctr::IpcMakeHeader(0x0010U, 2U, 0U));
    CHECK(ReadCommandWord(memory, kernel, 1) ==
          lego::ctr::kResultSuccess);
    CHECK(ReadCommandWord(memory, kernel, 2) ==
          (0x12345678U ^ 0xA5A5A5A5U));
    CHECK(echo->calls == 1);
}

void TestSrvUnknownServiceAndPortErrors() {
    Kernel kernel;
    GuestMemory memory;
    CHECK(memory.EnsureTlsMappings(kernel));
    CHECK(memory.Map(0x08000000U, 0x1000U,
                     MemoryPermission::Read | MemoryPermission::Write));
    IpcRouter router;
    SvcBridge bridge(kernel, &router);
    oot3d::recomp::a32::GuestState state{};

    WriteCString(memory, 0x08000000U, "missing");
    state.r[1] = 0x08000000U;
    bridge.Handle(Svc(lego::ctr::kSvcConnectToPort), state, &memory);
    CHECK(state.r[0] == lego::ctr::kResultNotFound);

    const std::string long_name = "abcdefghijkl";
    WriteCString(memory, 0x08000040U, long_name);
    state.r[1] = 0x08000040U;
    bridge.Handle(Svc(lego::ctr::kSvcConnectToPort), state, &memory);
    CHECK(state.r[0] == lego::ctr::kResultPortNameTooLong);

    WriteCString(memory, 0x08000080U, "srv:");
    state.r[1] = 0x08000080U;
    bridge.Handle(Svc(lego::ctr::kSvcConnectToPort), state, &memory);
    CHECK(state.r[0] == lego::ctr::kResultSuccess);
    const Handle srv_handle = state.r[1];

    ClearCommand(memory, kernel);
    IpcCommandBuffer request{};
    request[0] = lego::ctr::IpcMakeHeader(0x0005U, 4U, 0U);
    WriteServiceNameWords(&request, "none:");
    request[3] = 5U;
    request[4] = 1U;
    for (std::size_t index = 0; index <= 4; ++index) {
        WriteCommandWord(memory, kernel, index, request[index]);
    }

    state.r[0] = srv_handle;
    bridge.Handle(Svc(lego::ctr::kSvcSendSyncRequest), state, &memory);
    CHECK(state.r[0] == lego::ctr::kResultSuccess);
    CHECK(ReadCommandWord(memory, kernel, 0) ==
          lego::ctr::IpcMakeHeader(0x0005U, 1U, 0U));
    CHECK(ReadCommandWord(memory, kernel, 1) ==
          lego::ctr::kResultServiceNotRegistered);

    state.r[0] = 0xDEADBEEFU;
    bridge.Handle(Svc(lego::ctr::kSvcSendSyncRequest), state, &memory);
    CHECK(state.r[0] == lego::ctr::kResultInvalidHandle);
}

}  // namespace

int main() {
    TestConnectSrvAndRegisterClient();
    TestSrvGetServiceHandleAndSessionDispatch();
    TestSrvUnknownServiceAndPortErrors();

    if (failures != 0) {
        std::cerr << failures << " CTR IPC checks failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "PASS: CTR srv/session IPC checks\n";
    return EXIT_SUCCESS;
}
