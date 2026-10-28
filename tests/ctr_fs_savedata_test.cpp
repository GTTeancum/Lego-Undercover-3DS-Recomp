#include "services/fs_user_service.h"
#include "runtime/ctr_svc_bridge.h"
#include <array>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {
using namespace lego::ctr;
namespace fs = std::filesystem;
constexpr std::uint64_t kProgram = 0x00040000000AD500ULL;
constexpr auto RW = MemoryPermission::Read | MemoryPermission::Write;
int failures = 0;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL " << __LINE__ << ": " #x "\n"; ++failures; } } while (0)

struct TempDir {
    fs::path root;
    TempDir() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (unsigned i=0; i<64; ++i) {
            auto candidate=fs::temp_directory_path()/("lego-save-open-"+std::to_string(stamp)+"-"+std::to_string(i));
            std::error_code error;
            if (fs::create_directory(candidate,error)) { root=std::move(candidate); return; }
            if (error && error!=std::errc::file_exists) break;
        }
        throw std::runtime_error("could not allocate owned SaveData test directory");
    }
    ~TempDir() { std::error_code error; fs::remove_all(root,error); }
};
void Write(const fs::path& path, const std::string& bytes) {
    std::ofstream out(path,std::ios::binary); out.write(bytes.data(),bytes.size());
    if (!out) throw std::runtime_error("test file write failed");
}
std::string Bytes(const fs::path& path) {
    std::ifstream in(path,std::ios::binary);
    if (!in) throw std::runtime_error("test file read failed");
    return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
}
fs::path Title(const fs::path& root) { return root/"00040000"/"000ad500"/"data"/"00000001"; }

struct Fixture {
    TempDir temp;
    Kernel kernel{7,1};
    GuestMemory memory;
    IpcRouter router;
    SvcBridge bridge{kernel,&router};
    std::shared_ptr<FsUserService> endpoint=std::make_shared<FsUserService>(kProgram);
    std::shared_ptr<FsUserService> session;
    Handle handle{};
    a32::GuestState cpu{};
    static constexpr std::uint32_t input=0x0E022D3C;
    fs::path Root() const { return temp.root/"saves"; }
    fs::path SharedRoot() const { return temp.root/"shared"; }
    fs::path SharedUser() const { return SharedRoot()/"00048000"/"F000000B"/"user"; }
    std::uint32_t cb() const { return kernel.current_thread()->tls_address+kIpcCommandBufferOffset; }
    explicit Fixture(bool configured=true, bool initialized=true) {
        fs::create_directory(Root()); fs::create_directory(SharedRoot());
        CHECK(memory.EnsureTlsMappings(kernel));
        CHECK(memory.Map(input,1,MemoryPermission::Read));
        CHECK(memory.LoadBytes(input,std::array<std::uint8_t,1>{0x3C}));
        if (configured) CHECK(endpoint->ConfigureSaveDataRoot(Root()));
        CHECK(router.RegisterService("fs:USER",endpoint)==0);
        NewSession(initialized);
    }
    void NewSession(bool initialized=true) {
        CHECK(router.ConnectToService(kernel,"fs:USER",&handle)==0);
        const auto object=std::dynamic_pointer_cast<ClientSessionObject>(kernel.handles().Get(handle));
        if (!object) throw std::runtime_error("missing FS session");
        session=std::dynamic_pointer_cast<FsUserService>(object->service);
        if (!session) throw std::runtime_error("missing FS handler");
        if (initialized) {
            Put({0x08610042,0x040203C8,0x20,0xDEADBEEF});
            CHECK(Call().kind==a32::ExitKind::Fallthrough && cpu.r[0]==0);
            CHECK(session->program_id()==kProgram && session->process_id()==7);
        }
    }
    void Put(const IpcCommandBuffer& request) {
        for (unsigned i=0;i<request.size();++i) CHECK(memory.Write32(cb()+4*i,request[i]));
        for (unsigned i=0;i<16;++i) cpu.r[i]=0xBCAD0000+i;
        cpu.r[0]=handle; cpu.r[15]=0x0025947C; cpu.cpsr=0x60000010; cpu.fpscr=0x23000010;
    }
    IpcCommandBuffer Read() {
        IpcCommandBuffer command{};
        for (unsigned i=0;i<command.size();++i) CHECK(memory.Read32(cb()+4*i,&command[i]));
        return command;
    }
    a32::ExecutionResult Call(GuestMemory* alternate=nullptr) {
        return bridge.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcSendSyncRequest},
                             cpu,alternate ? alternate : &memory);
    }
    static IpcCommandBuffer Request(std::uint32_t address=input) {
        return {0x080C00C2,4,1,1,0x4002,address};
    }
    void Unsupported(const IpcCommandBuffer& request) {
        const auto saves=endpoint->save_archives().size(),shared=endpoint->archives().size();
        const auto handles=kernel.handles().OpenHandleCount();
        Put(request); const auto before=cpu;
        CHECK(Call().kind==a32::ExitKind::Svc && router.unsupported_request());
        CHECK(cpu.r==before.r && cpu.cpsr==before.cpsr && cpu.fpscr==before.fpscr && Read()==request);
        CHECK(endpoint->save_archives().size()==saves && endpoint->archives().size()==shared);
        CHECK(kernel.handles().OpenHandleCount()==handles && kernel.now_ns()==0);
    }
    std::uint64_t Reply(const IpcCommandBuffer& request,Result expected_result) {
        const auto handles=kernel.handles().OpenHandleCount(),threads=kernel.threads().size();
        const auto saves=endpoint->save_archives().size(),shared=endpoint->archives().size();
        Put(request); const auto before=cpu;
        CHECK(Call().kind==a32::ExitKind::Fallthrough && cpu.r[0]==0 && cpu.r[15]==0x00259480);
        for (unsigned i=1;i<15;++i) CHECK(cpu.r[i]==before.r[i]);
        CHECK(cpu.cpsr==before.cpsr && cpu.fpscr==before.fpscr);
        const auto reply=Read();
        const auto archive=std::uint64_t(reply[2]) | (std::uint64_t(reply[3])<<32);
        IpcCommandBuffer expected{0x080C00C0,expected_result};
        if (expected_result==0) { expected[2]=reply[2]; expected[3]=reply[3]; CHECK(archive!=0); }
        CHECK(reply==expected && !router.unsupported_request());
        CHECK(kernel.handles().OpenHandleCount()==handles && kernel.threads().size()==threads && kernel.now_ns()==0);
        CHECK(endpoint->save_archives().size()==saves+(expected_result==0) && endpoint->archives().size()==shared);
        return archive;
    }
};

void BackendConfigurationAndBinding() {
    TempDir temp,other;
    SaveDataArchiveMounts backend;
    std::uint64_t handle=99;
    bool stopped=false;
    try { backend.Open(kProgram,&handle); } catch (const std::logic_error&) { stopped=true; }
    CHECK(stopped && handle==0 && !backend.configured());
    CHECK(!backend.ConfigureRoot({}) && !backend.ConfigureRoot(temp.root/"missing"));
    CHECK(!fs::exists(temp.root/"missing"));
    Write(temp.root/"file","unaltered");
    CHECK(!backend.ConfigureRoot(temp.root/"file") && Bytes(temp.root/"file")=="unaltered");
    CHECK(backend.ConfigureRoot(temp.root) && backend.ConfigureRoot(temp.root));
    CHECK(!backend.ConfigureRoot(other.root) && backend.size()==0);
    CHECK(backend.Open(kProgram,nullptr)==kResultInvalidPointer);
    handle=99; stopped=false;
    try { backend.Open(0,&handle); } catch (const std::logic_error&) { stopped=true; }
    CHECK(stopped && handle==0 && backend.size()==0);
    auto path=temp.root;
    for (const auto* part:{"00040000","000ad500","data","00000001"}) {
        handle=99;
        CHECK(backend.Open(kProgram,&handle)==kResultFsNotFormatted && handle==0 && backend.size()==0);
        path/=part; CHECK(!fs::exists(path)); fs::create_directory(path);
    }
    Write(path/"existing.bin",std::string("saved\0bytes",11));
    // Open does not require or parse the sibling metadata file.
    CHECK(!fs::exists(path.parent_path()/"00000001.metadata"));
    CHECK(backend.Open(kProgram,&handle)==0 && IsSaveDataArchiveHandle(handle));
    const auto first=handle;
    CHECK((handle & ~kSaveDataArchiveHandleTag)==1 && backend.size()==1);
    const auto* mount=backend.Find(handle);
    CHECK(mount && mount->archive_id==4 && mount->program_id==kProgram && mount->path==path);
    CHECK(!backend.Find(0) && !backend.Find(1) && !backend.Find(kSaveDataArchiveHandleTag));
    Write(path.parent_path()/"00000001.metadata","uninterpreted metadata");
    CHECK(backend.Open(kProgram,&handle)==0 && handle!=first && backend.size()==2);
    CHECK(Bytes(path/"existing.bin")==std::string("saved\0bytes",11));
    CHECK(Bytes(path.parent_path()/"00000001.metadata")=="uninterpreted metadata");
    constexpr std::uint64_t other_program=0xABCDEF120034ABCDULL;
    const auto other_path=temp.root/"abcdef12"/"0034abcd"/"data"/"00000001";
    CHECK(backend.Open(other_program,&handle)==kResultFsNotFormatted && handle==0 && backend.size()==2);
    CHECK(!fs::exists(other_path));
    fs::create_directories(other_path);
    CHECK(backend.Open(other_program,&handle)==0 && backend.size()==3);
    CHECK(backend.Find(handle)->program_id==other_program && backend.Find(handle)->path==other_path);
}

void MissingReplyAndIgnoredEmptyByte() {
    Fixture unconfigured(false); unconfigured.Unsupported(Fixture::Request());
    CHECK(!unconfigured.endpoint->save_archives().configured() && fs::is_empty(unconfigured.Root()));
    CHECK(unconfigured.endpoint->ConfigureSaveDataRoot(unconfigured.Root()));
    CHECK(unconfigured.Reply(Fixture::Request(),0xC8A04554)==0 && fs::is_empty(unconfigured.Root()));
    Fixture f;
    for (const std::uint8_t payload:{std::uint8_t{0x3C},std::uint8_t{0},std::uint8_t{0xFF}}) {
        CHECK(f.memory.LoadBytes(Fixture::input,std::array<std::uint8_t,1>{payload}));
        auto request=Fixture::Request(); request[6]=0x12345678; request[7]=0x9ABCDEF0;
        CHECK(f.Reply(request,kResultFsNotFormatted)==0);
        std::uint8_t after=0; CHECK(f.memory.Read8(Fixture::input,&after) && after==payload);
        CHECK(fs::is_empty(f.Root()));
    }
    // One readable byte ending at the address-space boundary is still a valid span.
    CHECK(f.memory.Map(0xFFFFFFFF,1,MemoryPermission::Read));
    CHECK(f.memory.LoadBytes(0xFFFFFFFF,std::array<std::uint8_t,1>{0x3C}));
    CHECK(f.Reply(Fixture::Request(0xFFFFFFFF),kResultFsNotFormatted)==0);
}

void ExistingSaveAndSessionIsolation() {
    Fixture f;
    fs::create_directories(Title(f.Root())); Write(Title(f.Root())/"keep.bin","existing title data");
    const auto first=f.Reply(Fixture::Request(),0),second=f.Reply(Fixture::Request(),0);
    CHECK(first!=second && IsSaveDataArchiveHandle(first) && IsSaveDataArchiveHandle(second));
    CHECK(f.endpoint->save_archives().Find(first)->program_id==kProgram);
    CHECK(f.endpoint->save_archives().Find(first)->path==Title(f.Root()));
    CHECK(!f.endpoint->archives().Find(first));
    const auto old_session=f.session;
    f.NewSession(false); CHECK(f.session!=old_session && !f.session->initialized());
    CHECK(f.session->save_archives().Find(first)==old_session->save_archives().Find(first));
    f.Unsupported(Fixture::Request());
    f.NewSession(); CHECK(f.Reply(Fixture::Request(),0)!=first);
    CHECK(Bytes(Title(f.Root())/"keep.bin")=="existing title data");
    CHECK(!fs::exists(Title(f.Root()).parent_path()/"00000001.metadata"));
}

void RequestAndReplyPreflight() {
    Fixture f(true,false); fs::create_directories(Title(f.Root()));
    f.Unsupported(Fixture::Request()); f.NewSession();
    for (unsigned index:{0U,1U,2U,3U,4U}) {
        auto request=Fixture::Request(); request[index]^=1; f.Unsupported(request);
    }
    for (const auto descriptor:{2U,0x4802U,0x8002U,0x4003U}) {
        auto request=Fixture::Request(); request[4]=descriptor; f.Unsupported(request);
    }
    f.Unsupported({0x080F0180,1,2,3,4,5,1}); // Formatting is not inferred from an open.
    f.Unsupported({0x080E0080,1,0}); // Nor is CloseArchive implemented by this slice.
    CHECK(f.memory.Map(0x09000000,1,MemoryPermission::Write));
    for (auto address:{0U,0x08000000U,0x09000000U,Fixture::input+1,0xFFFFFFFFU}) {
        const auto request=Fixture::Request(address);
        f.Put(request);
        CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer);
        CHECK(f.Read()==request && f.endpoint->save_archives().size()==0);
    }
    for (unsigned mode=0;mode<3;++mode) {
        GuestMemory protected_memory;
        const unsigned bytes=mode==1 ? 24 : sizeof(IpcCommandBuffer);
        const auto permission=mode==0 ? MemoryPermission::Read : mode==1 ? RW : MemoryPermission::Write;
        CHECK(protected_memory.Map(f.cb(),bytes,permission));
        CHECK(protected_memory.Map(Fixture::input,1,MemoryPermission::Read));
        CHECK(protected_memory.LoadBytes(Fixture::input,std::array<std::uint8_t,1>{0x3C}));
        const auto request=Fixture::Request();
        CHECK(protected_memory.LoadBytes(f.cb(),{reinterpret_cast<const std::uint8_t*>(request.data()),bytes}));
        f.Put(request);
        CHECK(f.Call(&protected_memory).kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer);
        CHECK(f.endpoint->save_archives().size()==0 && f.Read()==request);
        if (mode!=2) {
            std::uint32_t header=0;
            CHECK(protected_memory.Read32(f.cb(),&header) && header==request[0]);
        }
    }
    CHECK(f.Reply(Fixture::Request(),0)==(kSaveDataArchiveHandleTag|1));
}

void DistinctSharedExtdataAndUnsupportedSaveFiles() {
    Fixture f;
    CHECK(f.endpoint->ConfigureSharedExtdataRoot(f.SharedRoot()));
    fs::create_directories(f.SharedUser()); fs::create_directories(Title(f.Root()));
    Write(f.SharedUser()/"keep.bin","shared bytes"); Write(Title(f.Root())/"keep.bin","title bytes");
    const auto save=f.Reply(Fixture::Request(),0);
    constexpr std::uint32_t shared_path=0x08001000,file_path=0x08002000;
    CHECK(f.memory.Map(shared_path,12,MemoryPermission::Read));
    const std::array<std::uint8_t,12> binary{0xA5,0,0,0,0x0B,0,0,0xF0,0xEF,0xBE,0xAD,0xDE};
    CHECK(f.memory.LoadBytes(shared_path,binary));
    f.Put({0x080C00C2,7,2,12,0x30002,shared_path});
    CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==0);
    const auto reply=f.Read();
    CHECK(reply[0]==0x080C00C0 && reply[1]==0 && reply[2]==1 && reply[3]==0);
    const std::uint64_t shared=reply[2];
    CHECK(!IsSaveDataArchiveHandle(shared) && save!=shared);
    CHECK(f.endpoint->archives().Find(shared) && !f.endpoint->save_archives().Find(shared));
    CHECK(!f.endpoint->archives().Find(save) && f.endpoint->save_archives().Find(save));

    const std::u16string path=u"/keep.bin";
    std::vector<std::uint8_t> encoded;
    for (auto unit:path) { encoded.push_back(unit&255); encoded.push_back(unit>>8); }
    encoded.push_back(0); encoded.push_back(0);
    const auto size=static_cast<std::uint32_t>(encoded.size());
    CHECK(f.memory.Map(file_path,size,MemoryPermission::Read));
    CHECK(f.memory.LoadBytes(file_path,encoded));
    IpcCommandBuffer open{0x080201C2,0,static_cast<std::uint32_t>(save),static_cast<std::uint32_t>(save>>32),
                          4,size,1,0,(size<<14)|2,file_path};
    IpcCommandBuffer create{0x08080202,0,open[2],open[3],4,size,0,123,0,(size<<14)|2,file_path};
    f.Unsupported(open); f.Unsupported(create);
    open[2]=static_cast<std::uint32_t>(shared); open[3]=0;
    const auto handles=f.kernel.handles().OpenHandleCount();
    f.Put(open); CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==0);
    CHECK(f.Read()[0]==0x08020042 && f.Read()[1]==0 && f.Read()[2]==0x10 && f.Read()[3]!=0);
    CHECK(f.kernel.handles().OpenHandleCount()==handles+1);
    open[2]=2; // An invalid untagged archive still uses the existing shared error reply.
    f.Put(open); CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==0);
    CHECK((f.Read()==IpcCommandBuffer{0x08020042,kResultFsInvalidArchiveHandle,0x10,0}));
    CHECK(Bytes(f.SharedUser()/"keep.bin")=="shared bytes" && Bytes(Title(f.Root())/"keep.bin")=="title bytes");
}

void NoFollowAndPinnedContainer() {
    for (unsigned blocked=0;blocked<4;++blocked) {
        TempDir temp,outside; SaveDataArchiveMounts backend;
        CHECK(backend.ConfigureRoot(temp.root));
        fs::path parent=temp.root;
        const std::array<std::string,4> parts{"00040000","000ad500","data","00000001"};
        for (unsigned i=0;i<blocked;++i) { parent/=parts[i]; fs::create_directory(parent); }
        const auto entry=parent/parts[blocked];
        Write(outside.root/"sentinel","unchanged");
        fs::create_directory_symlink(outside.root,entry);
        std::uint64_t handle=123;
        CHECK(backend.Open(kProgram,&handle)==kResultFsInvalidPath && handle==0 && backend.size()==0);
        fs::remove(entry); Write(entry,"not a directory");
        CHECK(backend.Open(kProgram,&handle)==kResultFsInvalidPath && handle==0 && backend.size()==0);
        CHECK(Bytes(entry)=="not a directory" && Bytes(outside.root/"sentinel")=="unchanged");
    }
    TempDir temp,outside;
    const auto selected=temp.root/"container",moved=temp.root/"moved";
    fs::create_directories(Title(selected));
    Write(Title(selected)/"keep.bin","original"); Write(outside.root/"sentinel","outside");
    SaveDataArchiveMounts backend; CHECK(backend.ConfigureRoot(selected));
    fs::rename(selected,moved); fs::create_directory_symlink(outside.root,selected);
    std::uint64_t handle=0;
    CHECK(backend.Open(kProgram,&handle)==0 && backend.Find(handle)->path==Title(selected));
    CHECK(Bytes(Title(moved)/"keep.bin")=="original" && Bytes(outside.root/"sentinel")=="outside");
    CHECK(!fs::exists(Title(outside.root))); // A pathname reopen would not find the pinned save.
    CHECK(!backend.ConfigureRoot(selected)); // No rebase to the replacement target.
}

#if defined(__unix__) || defined(__APPLE__)
void HostFailureStopsWithoutReplyOrAllocation() {
    Fixture f; fs::create_directories(Title(f.Root())); Write(Title(f.Root())/"keep.bin","unchanged");
    // A real EACCES failure keeps spare descriptors available to sanitizer
    // diagnostics. Changing only the isolated child avoids altering the host.
    const auto permissions=fs::status(Title(f.Root())).permissions();
    fs::permissions(Title(f.Root()),fs::perms::none);
    const auto child=::fork();
    if (child<0) throw std::runtime_error("fork failed");
    if (child==0) {
        if (::geteuid()==0 && (::setgid(65534)!=0 || ::setuid(65534)!=0)) _exit(90);
        const auto request=Fixture::Request(); f.Put(request); const auto before=f.cpu;
        const auto handles=f.kernel.handles().OpenHandleCount();
        const bool good=f.Call().kind==a32::ExitKind::Svc && f.router.unsupported_request() &&
            !f.router.last_host_error().empty() && f.Read()==request && f.cpu.r==before.r &&
            f.endpoint->save_archives().size()==0 && f.kernel.handles().OpenHandleCount()==handles;
        _exit(good ? 0 : 91);
    }
    int status=0; CHECK(::waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    fs::permissions(Title(f.Root()),permissions);
    CHECK(f.endpoint->save_archives().size()==0 && Bytes(Title(f.Root())/"keep.bin")=="unchanged");
    CHECK(f.Reply(Fixture::Request(),0)==(kSaveDataArchiveHandleTag|1));
}
#endif
} // namespace

int main() {
    BackendConfigurationAndBinding(); MissingReplyAndIgnoredEmptyByte(); ExistingSaveAndSessionIsolation();
    RequestAndReplyPreflight(); DistinctSharedExtdataAndUnsupportedSaveFiles(); NoFollowAndPinnedContainer();
#if defined(__unix__) || defined(__APPLE__)
    HostFailureStopsWithoutReplyOrAllocation();
#endif
    if (failures) return EXIT_FAILURE;
    std::cout << "PASS: explicit per-title SaveData, NotFormatted reply, typed archive isolation and atomic contained opens\n";
    return EXIT_SUCCESS;
}
