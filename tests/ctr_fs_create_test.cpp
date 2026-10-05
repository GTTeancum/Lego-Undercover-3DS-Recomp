#include "services/fs_user_service.h"
#include "runtime/ctr_svc_bridge.h"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#if defined(__unix__) || defined(__APPLE__)
#include <csignal>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {
using namespace lego::ctr;
namespace fs = std::filesystem;
int failures = 0;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL " << __LINE__ << ": " #x "\n"; ++failures; } } while (0)
struct TempDir {
    fs::path root;
    TempDir() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int i = 0; i < 64; ++i) {
            auto p = fs::temp_directory_path() / ("lego-create-" + std::to_string(stamp) + "-" + std::to_string(i));
            std::error_code ec;
            if (fs::create_directory(p, ec)) { root = p; return; }
            if (ec && ec != std::errc::file_exists) break;
        }
        throw std::runtime_error("could not create owned test directory");
    }
    ~TempDir() { std::error_code ec; fs::remove_all(root, ec); }
};
std::string Bytes(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("cannot read test output");
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}
struct Fixture {
    TempDir temp;
    Kernel kernel;
    GuestMemory memory;
    IpcRouter router;
    SvcBridge bridge{kernel, &router};
    std::shared_ptr<FsUserService> endpoint{std::make_shared<FsUserService>(0x00040000000AD500ULL)};
    Handle session{};
    std::uint64_t archive{};
    a32::GuestState cpu{};
    static constexpr std::uint32_t input = 0x08000000;
    std::uint32_t cb() const { return kernel.current_thread()->tls_address + kIpcCommandBufferOffset; }
    fs::path User() const { return temp.root / "00048000" / "F000000B" / "user"; }
    Fixture() {
        CHECK(memory.EnsureTlsMappings(kernel));
        CHECK(memory.Map(input, 0x1000, MemoryPermission::Read));
        CHECK(endpoint->ConfigureSharedExtdataRoot(temp.root));
        fs::create_directories(User());
        CHECK(router.RegisterService("fs:USER", endpoint) == 0);
        NewSession();
        const std::array<std::uint8_t,12> p{0,0,0,0,0x0B,0,0,0xF0,0,0,0,0};
        CHECK(memory.LoadBytes(input, p));
        Put({0x080C00C2,7,2,12,0x30002,input});
        CHECK(Call().kind == a32::ExitKind::Fallthrough && cpu.r[0] == 0);
        const auto reply = Read();
        CHECK(reply[1] == 0);
        archive = std::uint64_t(reply[2]) | (std::uint64_t(reply[3]) << 32);
    }
    void NewSession(bool initialize = true) {
        CHECK(router.ConnectToService(kernel,"fs:USER", &session) == 0);
        if (initialize) {
            Put({0x08610042,0x040203C8,0x20,0});
            CHECK(Call().kind == a32::ExitKind::Fallthrough && cpu.r[0] == 0);
        }
    }
    void Put(const IpcCommandBuffer& q) {
        for (unsigned i = 0; i < q.size(); ++i) CHECK(memory.Write32(cb()+4*i,q[i]));
        for (unsigned i = 0; i < 16; ++i) cpu.r[i] = 0x33440000+i;
        cpu.r[0] = session; cpu.r[15] = 0x0025947C;
    }
    IpcCommandBuffer Read() {
        IpcCommandBuffer q{};
        for (unsigned i = 0; i < q.size(); ++i) CHECK(memory.Read32(cb()+4*i,&q[i]));
        return q;
    }
    a32::ExecutionResult Call(GuestMemory* other = nullptr) {
        return bridge.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcSendSyncRequest},
                             cpu,other ? other : &memory);
    }
    IpcCommandBuffer Request(std::u16string_view path=u"/gamecoin.dat",std::uint64_t size=20) {
        std::vector<std::uint8_t> data;
        for (char16_t c : path) { data.push_back(c&255); data.push_back(c>>8); }
        data.push_back(0); data.push_back(0);
        const auto n=static_cast<std::uint32_t>(data.size());
        CHECK(memory.LoadBytes(input,data));
        return {0x08080202,0,static_cast<std::uint32_t>(archive),static_cast<std::uint32_t>(archive>>32),
                4,n,0,static_cast<std::uint32_t>(size),static_cast<std::uint32_t>(size>>32),(n<<14)|2,input};
    }
    void Reply(const IpcCommandBuffer& q,Result expected) {
        const auto handles=kernel.handles().OpenHandleCount();
        const auto time=kernel.now_ns(); const auto threads=kernel.threads().size();
        Put(q); const auto before=cpu;
        CHECK(Call().kind == a32::ExitKind::Fallthrough && cpu.r[0] == 0);
        CHECK(cpu.r[15] == 0x00259480);
        for (unsigned i=1;i<15;++i) CHECK(cpu.r[i] == before.r[i]);
        const IpcCommandBuffer reply{0x08080040,expected}; CHECK(Read() == reply);
        CHECK(handles == kernel.handles().OpenHandleCount() && time == kernel.now_ns());
        CHECK(threads == kernel.threads().size() && kernel.current_thread()->status == ThreadStatus::Running);
    }
    void Unsupported(const IpcCommandBuffer& q) {
        Put(q); const auto before=cpu;
        CHECK(Call().kind == a32::ExitKind::Svc && router.unsupported_request());
        CHECK(cpu.r == before.r && Read() == q);
    }
};
void CreateAndPreserve() {
    Fixture f;
    f.Reply(f.Request(),0);
    CHECK(Bytes(f.User()/"gamecoin.dat") == std::string(20,'\0'));
    { std::ofstream file(f.User()/"gamecoin.dat",std::ios::binary); file << "existing console data"; }
    const auto original=Bytes(f.User()/"gamecoin.dat");
    f.Reply(f.Request(),kResultFsFileAlreadyExists);
    f.Reply(f.Request(u"/gamecoin.dat",0),kResultFsFileAlreadyExists);
    CHECK(Bytes(f.User()/"gamecoin.dat") == original);
    f.NewSession();
    f.Reply(f.Request(u"/second.bin",7),0);
    CHECK(Bytes(f.User()/"second.bin") == std::string(7,'\0'));
    auto q=f.Request(u"/attributes.bin",1); q[1]=123; q[6]=0xFFFFFFFF;
    f.Reply(q,0); // Pinned backend ignores transaction and attributes.
    f.Reply(f.Request(u"/zero.bin",0),kResultFsUnsupportedOpenFlags);
    CHECK(!fs::exists(f.User()/"zero.bin"));
    fs::create_directory(f.User()/"nested");
    f.Reply(f.Request(u"/nested/\u00e9-\U0001F680.bin",32),0);
    CHECK(fs::file_size(f.User()/"nested"/fs::path(u"\u00e9-\U0001F680.bin")) == 32);
    f.Reply(f.Request(u"/missing/file.bin"),kResultFsPathNotFound);
    f.Reply(f.Request(u"/nested"),kResultFsFileAlreadyExists);
    f.Reply(f.Request(u"/second.bin/file.bin"),kResultFsUnexpectedFileOrDirectory);
    q=f.Request(u"/badhandle.bin"); q[3]=1;
    f.Reply(q,kResultFsInvalidArchiveHandle);
    CHECK(!fs::exists(f.User()/"badhandle.bin"));
}
void GuardedRequests() {
    Fixture f;
    auto base=f.Request();
    for (auto index : {0U,4U,5U,9U}) { auto q=base; q[index]^=1; f.Unsupported(q); }
    for (auto size : {0U,2U,0x1002U,0xFFFFFFFFU}) {
        auto q=base; q[5]=size; q[9]=(size<<14)|2; f.Unsupported(q);
    }
    for (std::uint64_t size : std::array<std::uint64_t,2>{kMaxSharedCreateSize+1,0x100000000ULL}) {
        auto q=base; q[7]=size; q[8]=size>>32; f.Unsupported(q);
    }
    f.NewSession(false); f.Unsupported(base); f.NewSession();
}
void InputAndOutputSafety() {
    Fixture f;
    for (auto address : {0x09000000U,0xFFFFFFF8U,Fixture::input+0x1000-2}) {
        auto q=f.Request(); q[10]=address; f.Put(q);
        CHECK(f.Call().kind == a32::ExitKind::Fallthrough && f.cpu.r[0] == kResultInvalidPointer);
        CHECK(f.Read() == q && fs::is_empty(f.User()));
    }
    for (bool partial : {false,true}) {
        auto q=f.Request(); f.Put(q);
        GuestMemory protected_memory;
        const auto size=partial ? 44U : unsigned(sizeof(IpcCommandBuffer));
        CHECK(protected_memory.Map(f.cb(),size,partial ? MemoryPermission::Read|MemoryPermission::Write : MemoryPermission::Read));
        CHECK(protected_memory.LoadBytes(f.cb(),{reinterpret_cast<const std::uint8_t*>(q.data()),size}));
        CHECK(f.Call(&protected_memory).kind == a32::ExitKind::Fallthrough && f.cpu.r[0] == kResultInvalidPointer);
        CHECK(fs::is_empty(f.User()));
    }
    for (const auto path : {u"relative",u"/../escape",u"/a/../b",u"//bad",u"/a/",u"/./a",
                            u"/a\\b",u"/a:b",u"/a?b",u"/\x001F"})
        f.Reply(f.Request(path),kResultFsInvalidPath);
    for (auto path : {std::u16string{u'/',char16_t(0xD800)},std::u16string{u'/',char16_t(0xDC00)},
                      std::u16string{u'/',u'a',0,u'b'},std::u16string(u"/")+std::u16string(256,u'a')})
        f.Reply(f.Request(path),kResultFsInvalidPath);
    auto q=f.Request();
    const std::array<std::uint8_t,2> nonnul{1,0};
    CHECK(f.memory.LoadBytes(Fixture::input+q[5]-2,nonnul));
    f.Reply(q,kResultFsInvalidPath);
    CHECK(fs::is_empty(f.User()));
}
void Containment() {
    Fixture f; TempDir outside;
    { std::ofstream file(outside.root/"sentinel"); file << "untouched"; }
    fs::create_symlink(outside.root/"sentinel",f.User()/"leaf");
    f.Reply(f.Request(u"/leaf"),kResultFsInvalidPath);
    fs::create_directory_symlink(outside.root,f.User()/"parent");
    f.Reply(f.Request(u"/parent/escaped.bin"),kResultFsInvalidPath);
    CHECK(Bytes(outside.root/"sentinel") == "untouched" && !fs::exists(outside.root/"escaped.bin"));
    const auto saved=f.User().parent_path()/"saved-user";
    fs::rename(f.User(),saved); fs::create_directory_symlink(outside.root,f.User());
    f.Reply(f.Request(),kResultFsInvalidPath);
    CHECK(!fs::exists(outside.root/"gamecoin.dat"));
    fs::remove(f.User()); fs::rename(saved,f.User());
    // The selected root is pinned: rename+replacement cannot redirect a write.
    const auto moved=f.temp.root.string()+"-moved";
    fs::rename(f.temp.root,moved); fs::create_directory_symlink(outside.root,f.temp.root);
    f.Reply(f.Request(u"/pinned.bin"),0);
    CHECK(fs::file_size(fs::path(moved)/"00048000/F000000B/user/pinned.bin") == 20);
    CHECK(!fs::exists(outside.root/"00048000"));
    fs::remove(f.temp.root); fs::rename(moved,f.temp.root);
}
#if defined(__unix__) || defined(__APPLE__)
void ConcurrentCreationPreservesWinner() {
    Fixture f;
    int gate[2]{};
    if (::pipe(gate) != 0) throw std::runtime_error("pipe failed");
    pid_t children[2]{};
    for (unsigned i=0; i<2; ++i) {
        children[i]=::fork();
        if (children[i]<0) throw std::runtime_error("fork failed");
        if (children[i]==0) {
            ::close(gate[1]);
            char go=0;
            if (::read(gate[0],&go,1)!=1) _exit(90);
            ::close(gate[0]);
            f.Put(f.Request(u"/concurrent.bin",20+i));
            if (f.Call().kind!=a32::ExitKind::Fallthrough || f.cpu.r[0]!=0) _exit(91);
            const auto result=f.Read()[1];
            _exit(result==0 ? 0 : result==kResultFsFileAlreadyExists ? 1 : 92);
        }
    }
    ::close(gate[0]);
    CHECK(::write(gate[1],"xx",2)==2);
    ::close(gate[1]);
    int exits[2]{-1,-1};
    for (unsigned i=0; i<2; ++i) {
        int status=0;
        CHECK(::waitpid(children[i],&status,0)==children[i] && WIFEXITED(status));
        if (WIFEXITED(status)) exits[i]=WEXITSTATUS(status);
    }
    CHECK((exits[0]==0 && exits[1]==1) || (exits[0]==1 && exits[1]==0));
    const auto expected=exits[0]==0 ? 20U : 21U;
    CHECK(Bytes(f.User()/"concurrent.bin")==std::string(expected,'\0'));
}
void HostFailureIsAStop() {
    Fixture f;
    const auto request=f.Request(u"/resize-failure.bin"); f.Put(request);
    const auto pid=::fork();
    if (pid<0) throw std::runtime_error("fork failed");
    if (pid==0) {
        std::signal(SIGXFSZ,SIG_IGN);
        const rlimit limit{0,0};
        if (::setrlimit(RLIMIT_FSIZE,&limit)!=0) _exit(90);
        const auto before=f.cpu;
        const auto stop=f.Call();
        _exit(stop.kind==a32::ExitKind::Svc && f.cpu.r==before.r && f.Read()==request &&
              f.router.unsupported_request() && !f.router.last_host_error().empty() ? 0 : 91);
    }
    int status=0; CHECK(::waitpid(pid,&status,0)==pid && WIFEXITED(status) && WEXITSTATUS(status)==0);
    // A real allocation failure can leave its exclusively-created zero-length
    // file. Report it; never pretend rollback or delete a concurrently replaced path.
    CHECK(fs::file_size(f.User()/"resize-failure.bin")==0);
    f.Reply(f.Request(u"/resize-failure.bin"),kResultFsFileAlreadyExists);
}
#endif
} // namespace
int main() {
#if defined(__unix__) || defined(__APPLE__)
    CreateAndPreserve(); GuardedRequests(); InputAndOutputSafety(); Containment(); ConcurrentCreationPreservesWinner(); HostFailureIsAStop();
#else
    SharedArchiveMounts mounts; CHECK(!mounts.CanCreateFiles());
#endif
    if (failures) return EXIT_FAILURE;
    std::cout << "PASS: guest-driven fixed-size creation, request guards, containment and host-error stops\n";
    return EXIT_SUCCESS;
}
