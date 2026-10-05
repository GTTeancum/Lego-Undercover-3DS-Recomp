#include "services/fs_file_service.h"
#include "runtime/ctr_svc_bridge.h"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <vector>
#if defined(__unix__) || defined(__APPLE__)
#include <csignal>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifdef LEGO_TEST_WRAP_FSYNC
#include <cerrno>
static bool fail_sync = false;
static unsigned sync_calls = 0;
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
    ++sync_calls;
    if (fail_sync) { errno = EIO; return -1; }
    return __real_fsync(fd);
}
#endif

namespace {
using namespace lego::ctr;
namespace fs = std::filesystem;
int failures = 0;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL " << __LINE__ << ": " #x "\n"; ++failures; } } while (0)
struct TempDir {
    fs::path root;
    TempDir() {
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for (unsigned i=0;i<64;++i) {
            auto path=fs::temp_directory_path()/("lego-write-"+std::to_string(stamp)+"-"+std::to_string(i));
            std::error_code ec;
            if (fs::create_directory(path,ec)) { root=path; return; }
            if (ec && ec!=std::errc::file_exists) break;
        }
        throw std::runtime_error("cannot create owned fixture directory");
    }
    ~TempDir() { std::error_code ec; fs::remove_all(root,ec); }
};
std::string Bytes(const fs::path& path) {
    std::ifstream f(path,std::ios::binary);
    if (!f) throw std::runtime_error("cannot read fixture");
    return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
}
struct Fixture {
    TempDir temp;
    SharedArchiveMounts mounts;
    Kernel kernel;
    GuestMemory memory;
    IpcRouter router;
    SvcBridge bridge{kernel,&router};
    Handle file{};
    std::uint64_t archive{};
    a32::GuestState cpu{};
    static constexpr std::uint32_t input=0x08000001;
    fs::path path() const { return temp.root/"00048000/F000000B/user/test.bin"; }
    std::uint32_t cb() const { return kernel.current_thread()->tls_address+kIpcCommandBufferOffset; }
    explicit Fixture(std::uint64_t size=20) {
        fs::create_directories(path().parent_path());
        { std::ofstream f(path(),std::ios::binary); }
        fs::resize_file(path(),size);
        CHECK(mounts.ConfigureRoot(temp.root));
        CHECK(mounts.Open(0xF000000B,&archive)==0);
        file=Open();
        CHECK(memory.EnsureTlsMappings(kernel));
        CHECK(memory.Map(input,kMaxFsFileTransfer,MemoryPermission::Read));
        std::vector<std::uint8_t> data(kMaxFsFileTransfer,0xA5);
        CHECK(memory.LoadBytes(input,data));
    }
    Handle Open() {
        std::unique_ptr<SharedArchiveFile> owner;
        CHECK(mounts.OpenFile(archive,u"/test.bin",3,&owner)==0);
        Handle h=0;
        CHECK(kernel.handles().Create(&h,std::make_shared<ClientSessionObject>("fs:File",std::make_shared<FsFileService>(std::move(owner))))==0);
        return h;
    }
    std::shared_ptr<FsFileService> Service(Handle h=0) {
        const auto s=std::dynamic_pointer_cast<ClientSessionObject>(kernel.handles().Get(h?h:file));
        return std::dynamic_pointer_cast<FsFileService>(s->service);
    }
    static IpcCommandBuffer Request(std::uint64_t offset=0,std::uint32_t count=20,std::uint32_t flags=0x10001) {
        return {0x08030102,static_cast<std::uint32_t>(offset),static_cast<std::uint32_t>(offset>>32),
                count,flags,(count<<4)|0xA,input};
    }
    void Put(const IpcCommandBuffer& q,Handle h=0) {
        for (unsigned i=0;i<q.size();++i) CHECK(memory.Write32(cb()+4*i,q[i]));
        for (unsigned i=0;i<16;++i) cpu.r[i]=0x11000000+i;
        cpu.r[0]=h?h:file;cpu.r[15]=0x0025947C;cpu.cpsr=0xA0000010;cpu.fpscr=0x23000010;
    }
    IpcCommandBuffer Read() {
        IpcCommandBuffer q{};
        for (unsigned i=0;i<q.size();++i) CHECK(memory.Read32(cb()+4*i,&q[i]));
        return q;
    }
    a32::ExecutionResult Call(GuestMemory* m=nullptr) {
        return bridge.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcSendSyncRequest},cpu,m?m:&memory);
    }
    void Reply(const IpcCommandBuffer& q,const IpcCommandBuffer& expected,Handle h=0) {
        const auto handles=kernel.handles().OpenHandleCount(), threads=kernel.threads().size();
        const auto now=kernel.now_ns();Put(q,h);const auto before=cpu;
        CHECK(Call().kind==a32::ExitKind::Fallthrough && cpu.r[0]==0 && Read()==expected);
        for (unsigned i=1;i<15;++i) CHECK(cpu.r[i]==before.r[i]);
        CHECK(cpu.r[15]==before.r[15]+4 && cpu.cpsr==before.cpsr && cpu.fpscr==before.fpscr);
        CHECK(handles==kernel.handles().OpenHandleCount() && threads==kernel.threads().size() && now==kernel.now_ns());
        CHECK(kernel.current_thread()->status==ThreadStatus::Running);
    }
    void Write(const IpcCommandBuffer& q,Result result,std::uint32_t count,Handle h=0) {
        Reply(q,{0x08030082,result,count,q[5],q[6]},h);
    }
    void Stop(const IpcCommandBuffer& q,Handle h=0) {
        const auto handles=kernel.handles().OpenHandleCount(),now=kernel.now_ns();
        Put(q,h);const auto before=cpu;
        CHECK(Call().kind==a32::ExitKind::Svc && router.unsupported_request());
        CHECK(Read()==q && cpu.r==before.r && cpu.cpsr==before.cpsr && cpu.fpscr==before.fpscr);
        CHECK(kernel.handles().OpenHandleCount()==handles && kernel.now_ns()==now);
    }
};
void BoundsAndWire() {
    Fixture f;
    f.Write(f.Request(),0,20);
    CHECK(Bytes(f.path())==std::string(20,char(0xA5)));
    std::uint8_t b=0;CHECK(f.memory.Read8(Fixture::input,&b) && b==0xA5);
    f.Write(f.Request(17,10),0,3);
    CHECK(fs::file_size(f.path())==20);
    f.Write(f.Request(20,10),0,0);
    f.Write(f.Request(21,10),kResultFsWriteBeyondEnd,0);
    f.Write(f.Request(std::numeric_limits<std::uint64_t>::max(),10),kResultFsWriteBeyondEnd,0);
    auto zero=f.Request(0,0);zero[6]=0xFFFFFFFF;
    f.Write(zero,0,0);
    f.Write(f.Request(0,kMaxFsFileTransfer),0,20);
    Fixture empty(0);empty.Write(empty.Request(),0,0);CHECK(fs::file_size(empty.path())==0);
    constexpr std::uint64_t big=(1ULL<<32)+4;
    Fixture large(big);large.Write(large.Request(big-2,20),0,2);
    CHECK(fs::file_size(large.path())==big);
    std::ifstream stream(large.path(),std::ios::binary);stream.seekg(big-3);
    char last[3]{};CHECK(bool(stream.read(last,3)) && last[0]==0 && static_cast<unsigned char>(last[1])==0xA5 && static_cast<unsigned char>(last[2])==0xA5);
    large.Reply({0x08040000},{0x080400C0,0,4,1});
}
void InputGuardsAndAliasing() {
    Fixture f;
    auto q=f.Request();
    for (unsigned index : {0U,3U,5U}) {auto bad=q;bad[index]^=1;f.Stop(bad);}
    for (auto descriptor : {0x14CU,0x14EU,0x50002U,0x14BU}) {auto bad=q;bad[5]=descriptor;f.Stop(bad);}
    for (unsigned count : {kMaxFsFileTransfer+1,0xFFFFFFFFU}) {auto bad=q;bad[3]=count;bad[5]=(count<<4)|0xA;f.Stop(bad);}
    CHECK(fs::file_size(f.path())==20 && Bytes(f.path())==std::string(20,0));
    for (auto address : {0x09000000U,0xFFFFFFF8U,Fixture::input+kMaxFsFileTransfer-1}) {
        auto bad=q;bad[6]=address;f.Put(bad);
        CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer && f.Read()==bad);
    }
    constexpr std::uint32_t write_only=0x09000000;
    CHECK(f.memory.Map(write_only,20,MemoryPermission::Write));
    auto bad=q;bad[6]=write_only;f.Put(bad);
    CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer && f.Read()==bad);
    // Even a clipped-to-one-byte write must validate the full declared input.
    bad=f.Request(19,20);bad[6]=Fixture::input+kMaxFsFileTransfer-1;f.Put(bad);
    CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer);
    for (const auto address : {f.cb(),f.cb()+4,f.cb()+252}) {
        bad=q;bad[6]=address;f.Stop(bad);CHECK(!f.router.last_host_error().empty());
    }
    CHECK(Bytes(f.path())==std::string(20,0));
    // Source adjacent to, but outside, the reply is not an alias.
    const auto adjacent=f.cb()+sizeof(IpcCommandBuffer);
    for (unsigned i=0;i<20;++i) CHECK(f.memory.Write8(adjacent+i,0xBB));
    q[6]=adjacent;f.Write(q,0,20);CHECK(Bytes(f.path())==std::string(20,char(0xBB)));
}
void ResponsePreflight() {
    Fixture f;
    for (bool partial : {false,true}) {
        for (const auto q : {f.Request(),IpcCommandBuffer{0x08080000}}) {
            f.Put(q);GuestMemory blocked;
            const auto n=partial?28U:unsigned(sizeof(IpcCommandBuffer));
            CHECK(blocked.Map(f.cb(),n,partial?MemoryPermission::Read|MemoryPermission::Write:MemoryPermission::Read));
            CHECK(blocked.LoadBytes(f.cb(),{reinterpret_cast<const std::uint8_t*>(q.data()),n}));
            CHECK(f.Call(&blocked).kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer);
            CHECK(Bytes(f.path())==std::string(20,0) && f.Service()->file().is_open());
        }
    }
}
void CloseAndOwnership() {
    Fixture f;
    Handle duplicate=0;CHECK(f.kernel.handles().Duplicate(&duplicate,f.file)==0);
    const auto independent=f.Open();
    f.Write(f.Request(),0,20,duplicate);
    f.Reply({0x08080000},{0x08080040,0},duplicate);
    CHECK(!f.Service()->file().is_open() && !f.Service(duplicate)->file().is_open());
    CHECK(f.Service(independent)->file().is_open());
    f.Stop(f.Request());f.Stop(f.Request(),duplicate);
    f.Reply({0x08080000},{0x08080040,0}); // HLE repeated-close policy, not hardware proof.
    f.Reply({0x08040000},{0x080400C0,0,20,0}); // Captured session size remains available.
    f.Write(f.Request(),0,20,independent);
    CHECK(f.kernel.CloseHandle(f.file)==0 && f.kernel.CloseHandle(duplicate)==0);
    f.file=independent;
    for (const auto q : {IpcCommandBuffer{0x08080040},IpcCommandBuffer{0x08080001},
                        IpcCommandBuffer{0x080200C2},IpcCommandBuffer{0x08090000}}) f.Stop(q);
    CHECK(f.kernel.CloseHandle(independent)==0);
}
void SourceReadDoesNotInvalidateReservation() {
    Fixture f;
    constexpr std::uint32_t source=0x0A000000;
    CHECK(f.memory.Map(source,20,MemoryPermission::Read|MemoryPermission::Write));
    CHECK(f.memory.Write32(source,0x12345678));
    std::uint64_t value=0,token=0;std::uint32_t fault=0;
    CHECK(f.memory.LoadExclusive(source,4,&value,&token,&fault));
    auto q=f.Request();q[6]=source;f.Write(q,0,20);
    CHECK(f.memory.StoreExclusive(source,4,0xABCDEF00,token,&fault)==a32::ExclusiveStoreResult::Success);
    CHECK(Bytes(f.path()).substr(0,4)==std::string("\x78\x56\x34\x12",4));
}
void ObjectAndExtentSafety() {
    Fixture f;
    const auto old=f.path().parent_path()/"old.bin";
    fs::rename(f.path(),old);{std::ofstream p(f.path());p<<"replacement untouched";}
    f.Write(f.Request(),0,20);
    CHECK(Bytes(old)==std::string(20,char(0xA5)) && Bytes(f.path())=="replacement untouched");
    fs::resize_file(old,10);f.Stop(f.Request());CHECK(fs::file_size(old)==10);
    fs::resize_file(old,25);f.Stop(f.Request());CHECK(fs::file_size(old)==25);
    fs::resize_file(old,20);const auto link=f.path().parent_path()/"alias.bin";
    fs::create_hard_link(old,link);f.Stop(f.Request());CHECK(!f.router.last_host_error().empty());
    fs::remove(link);f.Write(f.Request(),0,20);
}
#if defined(__unix__) || defined(__APPLE__)
void PartialHostFailure() {
    for (unsigned limit : {0U,8U}) {
        Fixture f;f.Put(f.Request());const auto request=f.Read();const auto before=f.cpu;
        const auto pid=::fork();if(pid<0)throw std::runtime_error("fork failed");
        if(pid==0) {
            std::signal(SIGXFSZ,SIG_IGN);const rlimit bound{limit,limit};
            if (::setrlimit(RLIMIT_FSIZE,&bound)!=0) _exit(90);
            const auto stop=f.Call();
            const auto marker="after "+std::to_string(limit)+" bytes";
            _exit(stop.kind==a32::ExitKind::Svc && f.cpu.r==before.r && f.Read()==request &&
                  f.router.last_host_error().find(marker)!=std::string::npos?0:91);
        }
        int status=0;CHECK(::waitpid(pid,&status,0)==pid && WIFEXITED(status) && WEXITSTATUS(status)==0);
        CHECK(Bytes(f.path())==std::string(limit,char(0xA5))+std::string(20-limit,0));
        CHECK(fs::file_size(f.path())==20);
    }
}
#endif
#ifdef LEGO_TEST_WRAP_FSYNC
void FlushFlagsAndFailure() {
    Fixture f;
    for (unsigned flags : {0U,1U,0x100U,0x10000U,0x10001U,0xFFFFFFFFU}) {
        const auto calls=sync_calls;f.Write(f.Request(0,20,flags),0,20);
        CHECK(sync_calls==calls+((flags&0xFF)!=0));
    }
    const auto calls=sync_calls;f.Write(f.Request(20,20,1),0,0);CHECK(sync_calls==calls);
    fail_sync=true;f.Stop(f.Request());fail_sync=false;
    CHECK(f.router.last_host_error().find("after 20 bytes")!=std::string::npos);
    CHECK(f.router.last_host_error().find("fsync")!=std::string::npos);
    CHECK(Bytes(f.path())==std::string(20,char(0xA5)));
    f.Write(f.Request(),0,20);CHECK(f.router.last_host_error().empty());
}
#endif
} // namespace
int main() {
#if defined(__unix__) || defined(__APPLE__)
    BoundsAndWire();InputGuardsAndAliasing();ResponsePreflight();CloseAndOwnership();
    SourceReadDoesNotInvalidateReservation();ObjectAndExtentSafety();PartialHostFailure();
#ifdef LEGO_TEST_WRAP_FSYNC
    FlushFlagsAndFailure();
#endif
#else
    SharedArchiveMounts mounts;CHECK(!mounts.CanOpenFiles());
#endif
    if(failures)return EXIT_FAILURE;
    std::cout<<"PASS: fixed-size Write, actual counts, flush/partial failures, Close ownership and IPC guards\n";
    return EXIT_SUCCESS;
}
