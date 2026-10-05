#include "services/fs_user_service.h"
#include "runtime/ctr_svc_bridge.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
using namespace lego::ctr;
constexpr std::uint64_t kProgram = 0x00040000000AD500ULL;
constexpr std::uint32_t kSdk = 0x040203C8U;
int failures = 0;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL " << __LINE__ << ": " #x "\n"; ++failures; } } while (0)

struct Fixture {
    Kernel kernel{7, 1};
    GuestMemory memory;
    IpcRouter router;
    SvcBridge bridge{kernel, &router};
    std::shared_ptr<FsUserService> endpoint{std::make_shared<FsUserService>(kProgram)};
    Handle handle{};
    a32::GuestState cpu{};

    Fixture() {
        if (!memory.EnsureTlsMappings(kernel) ||
            router.RegisterService("fs:USER", endpoint) != kResultSuccess ||
            router.ConnectToService(kernel, "fs:USER", &handle) != kResultSuccess)
            throw std::runtime_error("FS fixture failed");
    }
    std::shared_ptr<FsUserService> Session(Handle h) {
        auto session = std::dynamic_pointer_cast<ClientSessionObject>(kernel.handles().Get(h));
        if (!session) throw std::runtime_error("missing client session");
        auto fs = std::dynamic_pointer_cast<FsUserService>(session->service);
        if (!fs) throw std::runtime_error("wrong session service");
        return fs;
    }
    std::uint32_t Address() const {
        return kernel.current_thread()->tls_address + kIpcCommandBufferOffset;
    }
    void Put(const IpcCommandBuffer& request) {
        for (std::size_t i = 0; i < request.size(); ++i)
            CHECK(memory.Write32(Address() + static_cast<std::uint32_t>(i * 4), request[i]));
        for (unsigned i = 0; i < 16; ++i) cpu.r[i] = 0x12340000U + i;
        cpu.r[0] = handle;
        cpu.r[15] = 0x0025947C;
        cpu.cpsr = 0x60000010;
        cpu.fpscr = 0x23000010;
    }
    IpcCommandBuffer Read() {
        IpcCommandBuffer result{};
        for (std::size_t i = 0; i < result.size(); ++i)
            CHECK(memory.Read32(Address() + static_cast<std::uint32_t>(i * 4), &result[i]));
        return result;
    }
    a32::ExecutionResult Call() {
        return bridge.Handle({a32::ExitKind::Svc, cpu.r[15], a32::FallbackReason::None,
                              kSvcSendSyncRequest}, cpu, &memory);
    }
    void Success(const IpcCommandBuffer& request) {
        Put(request);
        const auto before = cpu;
        CHECK(Call().kind == a32::ExitKind::Fallthrough);
        CHECK(cpu.r[0] == 0 && cpu.r[15] == 0x00259480);
        for (unsigned i = 1; i < 15; ++i) CHECK(cpu.r[i] == before.r[i]);
        CHECK(cpu.cpsr == before.cpsr && cpu.fpscr == before.fpscr);
        IpcCommandBuffer expected{};
        expected[0] = IpcMakeHeader(IpcCommandId(request[0]), 1, 0);
        CHECK(Read() == expected);
        CHECK(router.last_request() == request && router.last_session_name() == "fs:USER");
        CHECK(!router.unsupported_request());
    }
    void Unsupported(const IpcCommandBuffer& request) {
        const auto fs = Session(handle);
        const auto initialized = fs->initialized();
        const auto version = fs->sdk_version();
        const auto pid = fs->process_id();
        const auto title = fs->program_id();
        const auto priority = fs->priority();
        const auto handles = kernel.handles().OpenHandleCount();
        Put(request);
        const auto before = cpu;
        CHECK(Call().kind == a32::ExitKind::Svc);
        CHECK(cpu.r == before.r && cpu.cpsr == before.cpsr && cpu.fpscr == before.fpscr);
        CHECK(Read() == request && router.unsupported_request());
        CHECK(fs->initialized() == initialized && fs->sdk_version() == version);
        CHECK(fs->process_id() == pid && fs->program_id() == title && fs->priority() == priority);
        CHECK(kernel.handles().OpenHandleCount() == handles);
    }
};

void ObservedStartupAndPidTrust() {
    Fixture f;
    const auto fs = f.Session(f.handle);
    CHECK(fs != f.endpoint && !fs->initialized() && fs->program_id() == 0);
    CHECK(fs->priority() == 0xFFFFFFFFU);
    const auto handles = f.kernel.handles().OpenHandleCount();
    const auto threads = f.kernel.threads().size();
    const auto time = f.kernel.now_ns();
    // The real game left a handle-sized placeholder after the PID descriptor.
    // Never look up this untrusted value as a process ID or a process handle.
    f.Success({0x08610042, kSdk, 0x20, 0x00048015});
    CHECK(fs->initialized() && fs->process_id() == 7 && fs->program_id() == kProgram);
    CHECK(fs->sdk_version() == kSdk);
    f.Success({0x08620040, 0});
    CHECK(fs->priority() == 0 && f.endpoint->priority() == 0);
    // Reinitialization is accepted by the pinned handler; it does not reset priority.
    for (const auto placeholder : {0U, 0xFFFFFFFFU, kCurrentProcessPseudoHandle}) {
        f.Success({0x08610042, kSdk, 0x20, placeholder});
        CHECK(fs->process_id() == 7 && fs->program_id() == kProgram);
        CHECK(fs->priority() == 0);
    }
    CHECK(!f.endpoint->initialized());
    CHECK(f.kernel.handles().OpenHandleCount() == handles);
    CHECK(f.kernel.threads().size() == threads && f.kernel.now_ns() == time);
    CHECK(f.kernel.current_thread()->status == ThreadStatus::Running);
    CHECK(f.kernel.current_thread()->priority == kThreadPriorityDefault);
    CHECK(!f.kernel.current_thread()->pending_wake);
}

void SeparateConnectionsAndDuplicatedHandles() {
    Fixture f;
    auto first = f.Session(f.handle);
    f.Success({0x08610042, kSdk, 0x20, 0});
    f.Success({0x08620040, 0x12345678});
    const auto original = f.handle;
    Handle copy = 0;
    CHECK(f.kernel.DuplicateHandle(&copy, original) == kResultSuccess);
    CHECK(f.Session(copy) == first);
    Handle second_handle = 0;
    CHECK(f.router.ConnectToService(f.kernel, "fs:USER", &second_handle) == kResultSuccess);
    const auto second = f.Session(second_handle);
    CHECK(second != first && !second->initialized() && second->program_id() == 0);
    CHECK(second->priority() == 0x12345678);
    f.handle = second_handle;
    f.Unsupported({0x08620040, 1}); // Scope restriction: initialization required.
    f.Success({0x08610042, 0x01000000, 0x20, 0xDEADBEEF});
    CHECK(first->sdk_version() == kSdk && second->sdk_version() == 0x01000000);
    f.Success({0x08620040, 0xFFFFFFFF}); // Pinned service stores an unrestricted u32.
    CHECK(first->priority() == 0xFFFFFFFF && second->priority() == 0xFFFFFFFF);
    CHECK(f.kernel.CloseHandle(original) == 0);
    f.handle = copy;
    f.Success({0x08620040, 2});
    CHECK(second->priority() == 2);
    CHECK(f.kernel.CloseHandle(copy) == 0 && f.kernel.CloseHandle(second_handle) == 0);
    first.reset();
    CHECK(f.router.ConnectToService(f.kernel, "fs:USER", &f.handle) == 0);
    CHECK(!f.Session(f.handle)->initialized() && f.Session(f.handle)->priority() == 2);
}

void GuestSrvLookupUsesSessionFactory() {
    Fixture f;
    CHECK(f.memory.Map(0x08000000, 0x1000, MemoryPermission::Read | MemoryPermission::Write));
    constexpr std::array<std::uint8_t, 5> port{'s','r','v',':',0};
    CHECK(f.memory.LoadBytes(0x08000000, port));
    Handle srv = 0;
    CHECK(f.router.ConnectToPort(f.kernel, f.memory, 0x08000000, &srv) == 0);
    auto original = f.Session(f.handle);
    const IpcCommandBuffer lookup{0x00050100, 0x553A7366, 0x00524553, 7, 1};
    f.handle = srv;
    f.Put(lookup);
    CHECK(f.Call().kind == a32::ExitKind::Fallthrough && f.cpu.r[0] == 0);
    const auto response = f.Read();
    CHECK(response[0] == 0x00050042 && response[1] == 0 && response[2] == 0x10);
    f.handle = response[3];
    const auto fs = f.Session(f.handle);
    CHECK(fs != original && !fs->initialized());
    f.Success({0x08610042, kSdk, 0x20, 0xFFFFFFFF});
    CHECK(fs->initialized() && !original->initialized());
}

void GuardsAndNoInventedFilesystemAccess() {
    Fixture f;
    f.Unsupported({0x08620040, 0});
    for (const bool initialized : {false, true}) {
        if (initialized) f.Success({0x08610042, kSdk, 0x20, 0});
        for (const auto request : {
                IpcCommandBuffer{0x08610040, kSdk, 0x20, 0},
                IpcCommandBuffer{0x08610043, kSdk, 0x20, 0},
                IpcCommandBuffer{0x08611042, kSdk, 0x20, 0},
                IpcCommandBuffer{0x08610042, kSdk, 0x10, 0},
                IpcCommandBuffer{0x08610042, kSdk, 0x21, 0},
                IpcCommandBuffer{0x08620000, 0},
                IpcCommandBuffer{0x08620042, 0, 0x20, 0},
                IpcCommandBuffer{0x08621040, 0},
                IpcCommandBuffer{0x08630000}, // GetPriority not observed/implemented.
                IpcCommandBuffer{0x08010002, 0x20, 0}, // Older Initialize is not this slice.
                IpcCommandBuffer{0x080C00C2, 7, 1, 0, 2, 0}, // No OpenArchive success.
                IpcCommandBuffer{0x080201C2}, // No OpenFile success.
                IpcCommandBuffer{0xFFFF0000}}) f.Unsupported(request);
    }
    auto invalid = std::make_shared<FsUserService>(0);
    CHECK(f.router.RegisterService("unknown", invalid) == 0);
    CHECK(f.router.ConnectToService(f.kernel, "unknown", &f.handle) == 0);
    CHECK(!f.Session(f.handle)->CanHandle({0x08610042, kSdk, 0x20, 0}));
}

void ResponsePreflightAndHandleExhaustion() {
    Fixture f;
    const auto fs = f.Session(f.handle);
    for (const auto request : {IpcCommandBuffer{0x08610042, kSdk, 0x20, 0},
                               IpcCommandBuffer{0x08620040, 9}}) {
        for (const bool partial : {false, true}) {
            GuestMemory protected_memory;
            const auto length = partial ? 8U : static_cast<unsigned>(sizeof(request));
            CHECK(protected_memory.Map(f.Address(), length, partial ?
                  MemoryPermission::Read | MemoryPermission::Write : MemoryPermission::Read));
            CHECK(protected_memory.LoadBytes(f.Address(), {
                  reinterpret_cast<const std::uint8_t*>(request.data()), length}));
            const auto old_priority = fs->priority();
            const auto old_init = fs->initialized();
            const auto old_version = fs->sdk_version();
            CHECK(f.router.SendSyncRequest(f.kernel, protected_memory, f.handle) == kResultInvalidPointer);
            CHECK(fs->priority() == old_priority && fs->initialized() == old_init);
            CHECK(fs->sdk_version() == old_version);
            std::uint32_t word = 0;
            CHECK(protected_memory.Read32(f.Address(), &word) && word == request[0]);
        }
        f.Success({0x08610042, kSdk, 0x20, 0});
    }
    while (f.kernel.handles().OpenHandleCount() < HandleTable::kMaxCount) {
        Handle h = 0;
        if (f.kernel.DuplicateHandle(&h, kCurrentProcessPseudoHandle) != 0)
            throw std::runtime_error("unexpected handle exhaustion");
    }
    Handle unchanged = 0xF00DBABE;
    CHECK(f.router.ConnectToService(f.kernel, "fs:USER", &unchanged) == kResultOutOfHandles);
    CHECK(unchanged == 0xF00DBABE && f.kernel.handles().OpenHandleCount() == HandleTable::kMaxCount);
    CHECK(fs->initialized() && fs->sdk_version() == kSdk);
    CHECK(!f.endpoint->initialized());
}
} // namespace

int main() {
    ObservedStartupAndPidTrust();
    SeparateConnectionsAndDuplicatedHandles();
    GuestSrvLookupUsesSessionFactory();
    GuardsAndNoInventedFilesystemAccess();
    ResponsePreflightAndHandleExhaustion();
    if (failures) return EXIT_FAILURE;
    std::cout << "PASS: FS initialization, caller identity, priority and session boundaries\n";
    return EXIT_SUCCESS;
}
