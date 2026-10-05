#include "services/fs_user_service.h"
#include "services/cfg_service.h"
#include "runtime/ctr_svc_bridge.h"
#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
using namespace lego::ctr;
namespace fs = std::filesystem;
int failures = 0;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL " << __LINE__ << ": " #x "\n"; ++failures; } } while (0)
struct TempDir {
    fs::path root;
    TempDir() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int n = 0; n < 64; ++n) {
            auto candidate = fs::temp_directory_path() /
                ("lego-fs-archive-" + std::to_string(stamp) + "-" + std::to_string(n));
            std::error_code ec;
            if (fs::create_directory(candidate, ec)) { root = candidate; return; }
            if (ec && ec != std::errc::file_exists) break;
        }
        throw std::runtime_error("could not allocate owned test directory");
    }
    ~TempDir() { std::error_code ec; fs::remove_all(root, ec); }
};
struct Fixture {
    TempDir temp;
    Kernel kernel;
    GuestMemory memory;
    IpcRouter router;
    SvcBridge bridge{kernel, &router};
    std::shared_ptr<FsUserService> endpoint{std::make_shared<FsUserService>(0x00040000000AD500ULL)};
    std::shared_ptr<FsUserService> session;
    Handle handle{};
    a32::GuestState cpu{};
    static constexpr std::uint32_t input = 0x08000000;
    std::uint32_t cb() const { return kernel.current_thread()->tls_address + kIpcCommandBufferOffset; }
    fs::path UserPath() const { return temp.root / "00048000" / "F000000B" / "user"; }
    explicit Fixture(bool configure = true) {
        if (!memory.EnsureTlsMappings(kernel)) throw std::runtime_error("TLS mapping failed");
        if (configure && !endpoint->ConfigureSharedExtdataRoot(temp.root)) throw std::runtime_error("root config failed");
        CHECK(router.RegisterService("fs:USER", endpoint) == 0);
        NewSession();
    }
    void NewSession() {
        CHECK(router.ConnectToService(kernel, "fs:USER", &handle) == 0);
        const auto object = std::dynamic_pointer_cast<ClientSessionObject>(kernel.handles().Get(handle));
        if (!object) throw std::runtime_error("session allocation failed");
        session = std::dynamic_pointer_cast<FsUserService>(object->service);
        if (!session) throw std::runtime_error("FS handler missing");
    }
    void Put(const IpcCommandBuffer& request) {
        for (std::size_t i = 0; i < request.size(); ++i) CHECK(memory.Write32(cb()+i*4, request[i]));
        for (unsigned i = 0; i < 16; ++i) cpu.r[i] = 0x11220000+i;
        cpu.r[0] = handle; cpu.r[15] = 0x0025947C;
    }
    IpcCommandBuffer Read() {
        IpcCommandBuffer out{};
        for (std::size_t i = 0; i < out.size(); ++i) CHECK(memory.Read32(cb()+i*4, &out[i]));
        return out;
    }
    a32::ExecutionResult Call(GuestMemory* alternate = nullptr) {
        return bridge.Handle({a32::ExitKind::Svc, cpu.r[15], a32::FallbackReason::None,
                              kSvcSendSyncRequest}, cpu, alternate ? alternate : &memory);
    }
    void Initialize() {
        Put({0x08610042, 0x040203C8, 0x20, 0});
        CHECK(Call().kind == a32::ExitKind::Fallthrough && cpu.r[0] == 0);
    }
    void MapPath(std::uint32_t address = input, std::uint32_t size = 12,
                 std::uint32_t high = 0) {
        CHECK(memory.Map(address, size, MemoryPermission::Read));
        std::array<std::uint8_t,12> bytes{0,0,0,0, 0x0B,0,0,0xF0, 0,0,0,0};
        for (unsigned i = 0; i < 4; ++i) bytes[8+i] = high >> (i*8);
        CHECK(memory.LoadBytes(address, std::span(bytes).first(size)));
    }
    static IpcCommandBuffer Request(std::uint32_t address = input) {
        return {0x080C00C2, 7, 2, 12, 0x00030002, address};
    }
    void Unsupported(const IpcCommandBuffer& request) {
        const auto count = endpoint->archives().size();
        Put(request); const auto before = cpu;
        CHECK(Call().kind == a32::ExitKind::Svc);
        CHECK(cpu.r == before.r && Read() == request && router.unsupported_request());
        CHECK(endpoint->archives().size() == count);
    }
};

void ExplicitRootAndNoAutomaticCreation() {
    Fixture f(false); f.Initialize(); f.MapPath();
    f.Unsupported(Fixture::Request());
    CHECK(!f.endpoint->ConfigureSharedExtdataRoot({}));
    CHECK(!f.endpoint->ConfigureSharedExtdataRoot(f.temp.root / "does-not-exist"));
    CHECK(!fs::exists(f.temp.root / "does-not-exist"));
    CHECK(f.endpoint->ConfigureSharedExtdataRoot(f.temp.root));
    f.Put(Fixture::Request());
    CHECK(f.Call().kind == a32::ExitKind::Fallthrough && f.cpu.r[0] == 0);
    IpcCommandBuffer expected{0x080C00C0, kResultFsNotFormatted, 0, 0};
    CHECK(f.Read() == expected && f.endpoint->archives().size() == 0);
    CHECK(fs::is_empty(f.temp.root));
}

void RealDirectoryMountAndSharedTable() {
    Fixture f; f.Initialize(); f.MapPath(Fixture::input, 12, 0x12345678);
    fs::create_directories(f.UserPath());
    const auto kernel_handles = f.kernel.handles().OpenHandleCount();
    const auto threads = f.kernel.threads().size();
    const auto time = f.kernel.now_ns();
    std::uint64_t first = 0;
    for (unsigned i = 0; i < 2; ++i) {
        f.Put(Fixture::Request()); const auto before = f.cpu;
        CHECK(f.Call().kind == a32::ExitKind::Fallthrough && f.cpu.r[0] == 0);
        CHECK(f.cpu.r[15] == 0x00259480);
        for (unsigned reg = 1; reg < 15; ++reg) CHECK(f.cpu.r[reg] == before.r[reg]);
        const auto reply = f.Read();
        CHECK(reply[0] == 0x080C00C0 && reply[1] == 0);
        const auto mount = std::uint64_t(reply[2]) | (std::uint64_t(reply[3]) << 32);
        CHECK(mount != 0 && mount != first);
        if (!i) first = mount;
        const auto* path = f.endpoint->archives().Find(mount);
        CHECK(path && *path == fs::canonical(f.UserPath()));
        CHECK(f.session->archives().Find(mount) == path);
        for (unsigned word = 4; word < reply.size(); ++word) CHECK(reply[word] == 0);
    }
    CHECK(f.endpoint->archives().size() == 2 && !f.endpoint->archives().Find(0));
    CHECK(f.kernel.handles().OpenHandleCount() == kernel_handles); // u64 archives are not kernel handles.
    CHECK(f.kernel.threads().size() == threads && f.kernel.now_ns() == time);
    CHECK(f.kernel.current_thread()->status == ThreadStatus::Running);
    CHECK(fs::is_empty(f.UserPath())); // OpenArchive created no gamecoin file.
    const auto previous_session = f.session;
    f.NewSession();
    CHECK(f.session != previous_session && !f.session->initialized());
    CHECK(f.session->archives().Find(first) != nullptr);
    f.Unsupported(Fixture::Request());
    f.Initialize();
    CHECK(f.endpoint->ConfigureSharedExtdataRoot(f.temp.root));
    TempDir other;
    CHECK(!f.endpoint->ConfigureSharedExtdataRoot(other.root));
}

void RequestGuardsAndMemorySafety() {
    Fixture f; fs::create_directories(f.UserPath()); f.MapPath();
    f.Unsupported(Fixture::Request()); f.Initialize();
    for (const auto index : {0U,1U,2U,3U,4U}) {
        auto request = Fixture::Request(); request[index] ^= 1;
        f.Unsupported(request);
    }
    f.Unsupported({0x08080202}); // CreateFile is still an explicit stop.
    f.Unsupported({0x080201C2}); // OpenFile is still an explicit stop.
    for (const auto address : {0x09000000U,0xFFFFFFF8U}) {
        const auto request = Fixture::Request(address);
        f.Put(request);
        CHECK(f.Call().kind == a32::ExitKind::Fallthrough && f.cpu.r[0] == kResultInvalidPointer);
        CHECK(f.Read() == request && f.endpoint->archives().size() == 0);
    }
    f.MapPath(0x09000000,11);
    const auto short_request = Fixture::Request(0x09000000);
    f.Put(short_request);
    CHECK(f.Call().kind == a32::ExitKind::Fallthrough && f.cpu.r[0] == kResultInvalidPointer);
    CHECK(f.Read() == short_request && f.endpoint->archives().size() == 0);
    for (bool partial : {false,true}) {
        GuestMemory protected_memory;
        const auto length = partial ? 12U : unsigned(sizeof(IpcCommandBuffer));
        CHECK(protected_memory.Map(f.cb(),length,partial ? MemoryPermission::Read|MemoryPermission::Write : MemoryPermission::Read));
        const auto request = Fixture::Request();
        CHECK(protected_memory.LoadBytes(f.cb(),{reinterpret_cast<const std::uint8_t*>(request.data()),length}));
        f.Put(request);
        CHECK(f.Call(&protected_memory).kind == a32::ExitKind::Fallthrough && f.cpu.r[0] == kResultInvalidPointer);
        CHECK(f.endpoint->archives().size() == 0);
    }
}

void HostContainmentAndMissingMount() {
    Fixture f; f.Initialize(); f.MapPath();
    TempDir outside;
    fs::create_directories(outside.root / "F000000B" / "user");
    std::error_code ec;
    fs::create_directory_symlink(outside.root, f.temp.root / "00048000", ec);
    if (ec) throw std::runtime_error("symlink containment test unavailable on this host");
    f.Put(Fixture::Request());
    CHECK(f.Call().kind == a32::ExitKind::Fallthrough && f.cpu.r[0] == 0);
    CHECK(f.Read()[1] == kResultFsInvalidPath && f.endpoint->archives().size() == 0);
    CHECK(fs::is_empty(outside.root / "F000000B" / "user"));
    fs::remove(f.temp.root / "00048000");
    fs::create_directories(f.UserPath());
    fs::remove(f.UserPath());
    f.Put(Fixture::Request());
    CHECK(f.Call().kind == a32::ExitKind::Fallthrough);
    CHECK(f.Read()[1] == kResultFsNotFormatted && f.Read()[2] == 0 && f.Read()[3] == 0);
}

void ConfigEndpointDoesNotInventValues() {
    Fixture f;
    CHECK(f.router.RegisterService("cfg:u", std::make_shared<CfgService>()) == 0);
    CHECK(f.router.ConnectToService(f.kernel, "cfg:u", &f.handle) == 0);
    for (const auto request : {IpcCommandBuffer{0x00010082,1,0xA0002}, IpcCommandBuffer{0x00020000},
                               IpcCommandBuffer{0x000A0000}, IpcCommandBuffer{0xFFFF0000}})
        f.Unsupported(request);
}
} // namespace
int main() {
    ExplicitRootAndNoAutomaticCreation(); RealDirectoryMountAndSharedTable();
    RequestGuardsAndMemorySafety(); HostContainmentAndMissingMount();
    ConfigEndpointDoesNotInventValues();
    if (failures) return EXIT_FAILURE;
    std::cout << "PASS: explicit shared archive mounts, strict CFG endpoint and no file creation\n";
    return EXIT_SUCCESS;
}
