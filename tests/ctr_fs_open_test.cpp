#include "services/fs_user_service.h"
#include "services/fs_file_service.h"
#include "runtime/ctr_svc_bridge.h"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/stat.h>
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
        for (int i=0; i<64; ++i) {
            auto candidate = fs::temp_directory_path() / ("lego-open-"+std::to_string(stamp)+"-"+std::to_string(i));
            std::error_code ec;
            if (fs::create_directory(candidate,ec)) { root=candidate; return; }
            if (ec && ec!=std::errc::file_exists) break;
        }
        throw std::runtime_error("could not allocate owned test directory");
    }
    ~TempDir() { std::error_code ec; fs::remove_all(root,ec); }
};
std::string Bytes(const fs::path& path) {
    std::ifstream input(path,std::ios::binary);
    if (!input) throw std::runtime_error("test input read failed");
    return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
}
void WriteFixture(const fs::path& path, const std::string& bytes) {
    std::ofstream output(path,std::ios::binary);
    if (!output.write(bytes.data(),bytes.size())) throw std::runtime_error("test fixture write failed");
}
struct Fixture {
    TempDir temp;
    Kernel kernel;
    GuestMemory memory;
    IpcRouter router;
    SvcBridge bridge{kernel,&router};
    std::shared_ptr<FsUserService> endpoint=std::make_shared<FsUserService>(0x00040000000AD500ULL);
    Handle fs_session{};
    std::uint64_t archive{};
    a32::GuestState cpu{};
    static constexpr std::uint32_t input=0x08000000;
    std::uint32_t cb() const { return kernel.current_thread()->tls_address+kIpcCommandBufferOffset; }
    fs::path User() const { return temp.root/"00048000/F000000B/user"; }
    Fixture() {
        CHECK(memory.EnsureTlsMappings(kernel));
        CHECK(memory.Map(input,0x1000,MemoryPermission::Read));
        CHECK(endpoint->ConfigureSharedExtdataRoot(temp.root));
        fs::create_directories(User());
        CHECK(router.RegisterService("fs:USER",endpoint)==0);
        NewSession();
        const std::array<std::uint8_t,12> path{0,0,0,0,0x0B,0,0,0xF0,0,0,0,0};
        CHECK(memory.LoadBytes(input,path));
        Put({0x080C00C2,7,2,12,0x30002,input});
        CHECK(Call().kind==a32::ExitKind::Fallthrough && cpu.r[0]==0);
        const auto reply=Read();
        CHECK(reply[1]==0);
        archive=std::uint64_t(reply[2]) | (std::uint64_t(reply[3])<<32);
        // Synthetic nonzero bytes catch accidental truncation or replacement.
        WriteFixture(User()/"gamecoin.dat","preserve these bytes");
    }
    void NewSession(bool initialize=true) {
        CHECK(router.ConnectToService(kernel,"fs:USER",&fs_session)==0);
        if (initialize) {
            Put({0x08610042,0x040203C8,0x20,0});
            CHECK(Call().kind==a32::ExitKind::Fallthrough && cpu.r[0]==0);
        }
    }
    void Put(const IpcCommandBuffer& command,Handle target=0) {
        for (unsigned i=0;i<command.size();++i) CHECK(memory.Write32(cb()+4*i,command[i]));
        for (unsigned i=0;i<16;++i) cpu.r[i]=0x44110000+i;
        cpu.r[0]=target ? target : fs_session;
        cpu.r[15]=0x0025947C; cpu.cpsr=0xA0000010; cpu.fpscr=0x23000010;
    }
    IpcCommandBuffer Read() {
        IpcCommandBuffer result{};
        for (unsigned i=0;i<result.size();++i) CHECK(memory.Read32(cb()+4*i,&result[i]));
        return result;
    }
    a32::ExecutionResult Call(GuestMemory* other=nullptr) {
        return bridge.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcSendSyncRequest},
                             cpu,other ? other : &memory);
    }
    IpcCommandBuffer Request(std::u16string_view name=u"/gamecoin.dat",std::uint32_t mode=3) {
        std::vector<std::uint8_t> bytes;
        for (char16_t c : name) { bytes.push_back(c&255); bytes.push_back(c>>8); }
        bytes.push_back(0); bytes.push_back(0);
        CHECK(memory.LoadBytes(input,bytes));
        const auto n=static_cast<std::uint32_t>(bytes.size());
        return {0x080201C2,0,static_cast<std::uint32_t>(archive),static_cast<std::uint32_t>(archive>>32),
                4,n,mode,0,(n<<14)|2,input};
    }
    Handle Open(const IpcCommandBuffer& request,Result expected=0) {
        const auto handles=kernel.handles().OpenHandleCount(), mounts=endpoint->archives().size();
        const auto time=kernel.now_ns(), threads=kernel.threads().size();
        Put(request); const auto before=cpu;
        CHECK(Call().kind==a32::ExitKind::Fallthrough && cpu.r[0]==0);
        CHECK(cpu.r[15]==before.r[15]+4 && cpu.cpsr==before.cpsr && cpu.fpscr==before.fpscr);
        for (unsigned i=1;i<15;++i) CHECK(cpu.r[i]==before.r[i]);
        const auto reply=Read();
        CHECK(reply==IpcCommandBuffer({0x08020042,expected,IpcMoveHandleDesc(),reply[3]}));
        CHECK((reply[3]!=0)==(expected==0));
        CHECK(kernel.handles().OpenHandleCount()==handles+(expected==0));
        CHECK(endpoint->archives().size()==mounts && kernel.now_ns()==time);
        CHECK(kernel.threads().size()==threads && kernel.current_thread()->status==ThreadStatus::Running);
        CHECK(!router.HasService("fs:File")); // Object connection, not global discovery.
        return reply[3];
    }
    void Unsupported(const IpcCommandBuffer& request,Handle target=0) {
        const auto handles=kernel.handles().OpenHandleCount();
        Put(request,target); const auto before=cpu;
        CHECK(Call().kind==a32::ExitKind::Svc && router.unsupported_request());
        CHECK(cpu.r==before.r && cpu.cpsr==before.cpsr && cpu.fpscr==before.fpscr && Read()==request);
        CHECK(handles==kernel.handles().OpenHandleCount());
    }
    std::shared_ptr<FsFileService> Service(Handle handle) {
        const auto object=std::dynamic_pointer_cast<ClientSessionObject>(kernel.handles().Get(handle));
        if (!object || object->name!="fs:File") throw std::runtime_error("real file session missing");
        auto service=std::dynamic_pointer_cast<FsFileService>(object->service);
        if (!service) throw std::runtime_error("file endpoint wrong type");
        return service;
    }
    void Size(Handle handle,std::uint64_t expected) {
        const auto handles=kernel.handles().OpenHandleCount(), time=kernel.now_ns();
        Put({0x08040000},handle); const auto before=cpu;
        CHECK(Call().kind==a32::ExitKind::Fallthrough && cpu.r[0]==0);
        CHECK(Read()==IpcCommandBuffer({0x080400C0,0,static_cast<std::uint32_t>(expected),static_cast<std::uint32_t>(expected>>32)}));
        for (unsigned i=1;i<15;++i) CHECK(cpu.r[i]==before.r[i]);
        CHECK(cpu.r[15]==before.r[15]+4 && cpu.cpsr==before.cpsr && cpu.fpscr==before.fpscr);
        CHECK(kernel.handles().OpenHandleCount()==handles && kernel.now_ns()==time);
    }
};
#if defined(__linux__)
std::vector<int> FdsFor(const fs::path& path) {
    struct stat target{};
    if (::stat(path.c_str(),&target)!=0) throw std::runtime_error("stat test fixture failed");
    std::vector<int> result;
    for (const auto& item : fs::directory_iterator("/proc/self/fd")) {
        const auto name=item.path().filename().string();
        const int fd=std::stoi(name);
        struct stat opened{};
        if (::fstat(fd,&opened)==0 && opened.st_dev==target.st_dev && opened.st_ino==target.st_ino)
            result.push_back(fd);
    }
    return result;
}
#endif
void RealHandlesAndLifetime() {
    Fixture f;
    const auto bytes=Bytes(f.User()/"gamecoin.dat");
    const auto first=f.Open(f.Request());
    auto service=f.Service(first);
    CHECK(service->file().size()==bytes.size() && service->file().effective_mode()==3);
    f.Size(first,bytes.size());
    Handle duplicate=0;
    CHECK(f.kernel.handles().Duplicate(&duplicate,first)==0);
    CHECK(f.kernel.handles().Get(duplicate)==f.kernel.handles().Get(first));
    std::weak_ptr<FsFileService> lifetime=service;
    service.reset();
#if defined(__linux__)
    const auto descriptors=FdsFor(f.User()/"gamecoin.dat");
    CHECK(descriptors.size()==1);
    CHECK((::fcntl(descriptors.at(0),F_GETFL)&O_ACCMODE)==O_RDWR);
    CHECK((::fcntl(descriptors.at(0),F_GETFD)&FD_CLOEXEC)!=0);
#endif
    CHECK(f.kernel.CloseHandle(first)==0 && !lifetime.expired());
    CHECK(f.kernel.CloseHandle(first)==kResultInvalidHandle);
    f.Size(duplicate,bytes.size());
    CHECK(f.kernel.CloseHandle(f.fs_session)==0); // Closing FS does not close the file.
    f.Size(duplicate,bytes.size());
    CHECK(f.kernel.CloseHandle(duplicate)==0 && lifetime.expired());
#if defined(__linux__)
    CHECK(FdsFor(f.User()/"gamecoin.dat").empty());
#endif
    f.NewSession();
    const auto a=f.Open(f.Request()), b=f.Open(f.Request());
    CHECK(a!=b && f.Service(a)!=f.Service(b));
    CHECK(f.kernel.CloseHandle(a)==0); f.Size(b,bytes.size());
    CHECK(f.kernel.CloseHandle(b)==0 && Bytes(f.User()/"gamecoin.dat")==bytes);
}
void ModesAndErrors() {
    Fixture f;
    const auto original=Bytes(f.User()/"gamecoin.dat");
    for (unsigned mode : {1U,2U,3U}) {
        auto q=f.Request(u"/gamecoin.dat",mode); q[1]=17; q[7]=0xFFFFFFFF;
        const auto h=f.Open(q);
        CHECK(f.Service(h)->file().effective_mode()==3);
        f.Size(h,20); CHECK(f.kernel.CloseHandle(h)==0);
    }
    for (unsigned mode : {0U,4U,5U,6U,7U}) f.Open(f.Request(u"/gamecoin.dat",mode),kResultFsUnsupportedOpenFlags);
    f.Open(f.Request(u"/missing"),kResultFsFileNotFound);
    CHECK(!fs::exists(f.User()/"missing"));
    f.Open(f.Request(u"/missing/child"),kResultFsPathNotFound);
    f.Open(f.Request(u"/gamecoin.dat/child"),kResultFsUnexpectedFileOrDirectory);
    fs::create_directory(f.User()/"directory");
    f.Open(f.Request(u"/directory"),kResultFsUnexpectedFileOrDirectory);
    auto q=f.Request(); q[3]=1; f.Open(q,kResultFsInvalidArchiveHandle);
    q=f.Request(); q[2]=0; f.Open(q,kResultFsInvalidArchiveHandle);
    CHECK(Bytes(f.User()/"gamecoin.dat")==original);
    WriteFixture(f.User()/"directory"/fs::path(u"\u00e9-\U0001F680.bin"),"abc");
    const auto h=f.Open(f.Request(u"/directory/\u00e9-\U0001F680.bin"));
    f.Size(h,3); CHECK(f.kernel.CloseHandle(h)==0);
}
void StrictRequestsAndMemory() {
    Fixture f;
    const auto base=f.Request();
    const auto bytes=Bytes(f.User()/"gamecoin.dat");
    for (unsigned index : {0U,4U,5U,8U}) { auto q=base; q[index]^=1; f.Unsupported(q); }
    for (unsigned n : {0U,2U,0x1002U,0xFFFFFFFFU}) {
        auto q=base; q[5]=n; q[8]=(n<<14)|2; f.Unsupported(q);
    }
    for (unsigned mode : {8U,0x10000U,0xFFFFFFFFU}) {
        auto q=base; q[6]=mode; f.Unsupported(q);
    }
    f.NewSession(false); f.Unsupported(base); f.NewSession();
    for (auto address : {0x09000000U,0xFFFFFFF8U,Fixture::input+0x1000-2}) {
        auto q=f.Request(); q[9]=address;
        const auto handles=f.kernel.handles().OpenHandleCount(); f.Put(q);
        CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer);
        CHECK(f.Read()==q && f.kernel.handles().OpenHandleCount()==handles);
    }
    for (bool partial : {false,true}) {
        auto q=f.Request(); f.Put(q);
        const auto count=f.kernel.handles().OpenHandleCount();
        GuestMemory protected_memory;
        const auto n=partial ? 40U : unsigned(sizeof(IpcCommandBuffer));
        CHECK(protected_memory.Map(f.cb(),n,partial ? MemoryPermission::Read|MemoryPermission::Write : MemoryPermission::Read));
        CHECK(protected_memory.LoadBytes(f.cb(),{reinterpret_cast<const std::uint8_t*>(q.data()),n}));
        CHECK(f.Call(&protected_memory).kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer);
        CHECK(f.kernel.handles().OpenHandleCount()==count);
    }
    for (auto path : {u"relative",u"/../escape",u"/dir/../gamecoin.dat",u"//gamecoin.dat",u"/a/",u"/./gamecoin.dat",u"/a\\b",u"/a:b",u"/a?b",u"/\x001F"})
        f.Open(f.Request(path),kResultFsInvalidPath);
    for (auto path : {std::u16string{u'/',char16_t(0xD800)},std::u16string{u'/',char16_t(0xDC00)},
                     std::u16string{u'/',u'a',0,u'b'},std::u16string(u"/")+std::u16string(256,u'a')})
        f.Open(f.Request(path),kResultFsInvalidPath);
    auto q=f.Request(); const std::array<std::uint8_t,2> nonnul{1,0};
    CHECK(f.memory.LoadBytes(Fixture::input+q[5]-2,nonnul));
    f.Open(q,kResultFsInvalidPath);
    CHECK(Bytes(f.User()/"gamecoin.dat")==bytes);
#if defined(__linux__)
    CHECK(FdsFor(f.User()/"gamecoin.dat").empty());
#endif
}
void FileRequestsAndSizeSnapshot() {
    Fixture f;
    const auto h=f.Open(f.Request());
    for (auto q : {IpcCommandBuffer{0x08040040},IpcCommandBuffer{0x08040001},
                   IpcCommandBuffer{0x08030102,0,0,20,0x10001,0x14A,0x0FFFF600},
                   IpcCommandBuffer{0x080200C2},IpcCommandBuffer{0x08080000},IpcCommandBuffer{0x08090000}})
        f.Unsupported(q,h);
    f.Size(h,20);
    // Session size is captured at open as in the reference, not recomputed from
    // an unrelated replacement pathname each time GetSize is called.
    fs::rename(f.User()/"gamecoin.dat",f.User()/"old.dat");
    WriteFixture(f.User()/"gamecoin.dat","new");
    f.Size(h,20);
    const auto replacement=f.Open(f.Request()); f.Size(replacement,3);
    CHECK(f.kernel.CloseHandle(h)==0 && f.kernel.CloseHandle(replacement)==0);
    WriteFixture(f.User()/"zero","");
    const auto zero=f.Open(f.Request(u"/zero")); f.Size(zero,0); CHECK(f.kernel.CloseHandle(zero)==0);
    WriteFixture(f.User()/"large","");
    constexpr std::uint64_t big=(1ULL<<32)+17;
    fs::resize_file(f.User()/"large",big);
    const auto large=f.Open(f.Request(u"/large")); f.Size(large,big); CHECK(f.kernel.CloseHandle(large)==0);
    CHECK(fs::file_size(f.User()/"large")==big);
}
void ContainmentAndExhaustion() {
    Fixture f; TempDir outside;
    WriteFixture(outside.root/"sentinel","outside unchanged");
    fs::create_symlink(outside.root/"sentinel",f.User()/"leaf");
    fs::create_directory_symlink(outside.root,f.User()/"parent");
    fs::create_hard_link(outside.root/"sentinel",f.User()/"hardlink");
    f.Open(f.Request(u"/leaf"),kResultFsInvalidPath);
    f.Open(f.Request(u"/parent/sentinel"),kResultFsInvalidPath);
    f.Open(f.Request(u"/hardlink"),kResultFsInvalidPath);
#if defined(__unix__) || defined(__APPLE__)
    CHECK(::mkfifo((f.User()/"fifo").c_str(),0600)==0);
    f.Open(f.Request(u"/fifo"),kResultFsUnexpectedFileOrDirectory);
#endif
    const auto saved=f.User().parent_path()/"saved";
    fs::rename(f.User(),saved); fs::create_directory_symlink(outside.root,f.User());
    f.Open(f.Request(u"/sentinel"),kResultFsInvalidPath);
    fs::remove(f.User()); fs::rename(saved,f.User());
    const auto moved=f.temp.root.string()+"-moved";
    fs::rename(f.temp.root,moved); fs::create_directory_symlink(outside.root,f.temp.root);
    const auto pinned=f.Open(f.Request()); f.Size(pinned,20); CHECK(f.kernel.CloseHandle(pinned)==0);
    fs::remove(f.temp.root); fs::rename(moved,f.temp.root);
    std::vector<Handle> handles;
    Handle duplicate=0;
    while (f.kernel.handles().Duplicate(&duplicate,f.fs_session)==0) handles.push_back(duplicate);
    f.Open(f.Request(),kResultOutOfHandles);
#if defined(__linux__)
    CHECK(FdsFor(f.User()/"gamecoin.dat").empty());
#endif
    CHECK(Bytes(outside.root/"sentinel")=="outside unchanged");
    for (auto handle : handles) CHECK(f.kernel.CloseHandle(handle)==0);
    const auto after=f.Open(f.Request()); f.Size(after,20); CHECK(f.kernel.CloseHandle(after)==0);
}
#if defined(__unix__) || defined(__APPLE__)
void HostFailureStopsCleanly() {
    Fixture f;
    const auto request=f.Request(); f.Put(request);
    // Use a real permission failure, not RLIMIT_NOFILE=0: the latter also
    // prevents sanitizers from inspecting mappings and misdiagnoses C++ vptrs.
    fs::permissions(f.User()/"gamecoin.dat",fs::perms::none);
    const auto pid=::fork();
    if (pid<0) throw std::runtime_error("fork failed");
    if (pid==0) {
        // Root bypasses mode bits, so drop only the isolated child's identity.
        if (::geteuid()==0 && (::setgid(65534)!=0 || ::setuid(65534)!=0)) _exit(90);
        const auto before=f.cpu; const auto handles=f.kernel.handles().OpenHandleCount();
        const auto result=f.Call();
        _exit(result.kind==a32::ExitKind::Svc && before.r==f.cpu.r && f.Read()==request &&
              f.kernel.handles().OpenHandleCount()==handles && !f.router.last_host_error().empty() ? 0 : 91);
    }
    int status=0;
    CHECK(::waitpid(pid,&status,0)==pid && WIFEXITED(status) && WEXITSTATUS(status)==0);
    fs::permissions(f.User()/"gamecoin.dat",fs::perms::owner_read|fs::perms::owner_write);
    CHECK(Bytes(f.User()/"gamecoin.dat")=="preserve these bytes");
}
#endif
} // namespace
int main() {
#if defined(__unix__) || defined(__APPLE__)
    RealHandlesAndLifetime(); ModesAndErrors(); StrictRequestsAndMemory();
    FileRequestsAndSizeSnapshot(); ContainmentAndExhaustion(); HostFailureStopsCleanly();
#else
    SharedArchiveMounts mounts; CHECK(!mounts.CanOpenFiles());
#endif
    if (failures) return EXIT_FAILURE;
    std::cout << "PASS: contained OpenFile, real session ownership, GetSize, failure and lifetime guards\n";
    return EXIT_SUCCESS;
}
