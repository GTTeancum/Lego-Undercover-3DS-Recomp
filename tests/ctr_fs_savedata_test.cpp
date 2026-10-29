#include "services/fs_user_service.h"
#include "services/fs_file_service.h"
#include "runtime/ctr_svc_bridge.h"
#include <array>
#include <chrono>
#include <csignal>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <system_error>
#include <vector>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#include <sys/resource.h>
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
    static IpcCommandBuffer FormatRequest(std::uint32_t address=input) {
        // Authored 4 KiB/5-directory/7-file metadata, with ignored bucket counts.
        return {0x084C0242,4,1,1,8,5,7,3,4,1,0x4002,address};
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
    void FormatReply(const IpcCommandBuffer& request,Result result=kResultSuccess) {
        const auto handles=kernel.handles().OpenHandleCount(),threads=kernel.threads().size();
        const auto saves=endpoint->save_archives().size(),shared=endpoint->archives().size();
        Put(request); const auto before=cpu;
        CHECK(Call().kind==a32::ExitKind::Fallthrough && cpu.r[0]==0 && cpu.r[15]==before.r[15]+4);
        CHECK((Read()==IpcCommandBuffer{0x084C0040,result}) && !router.unsupported_request());
        for (unsigned i=1;i<15;++i) CHECK(cpu.r[i]==before.r[i]);
        CHECK(cpu.cpsr==before.cpsr && cpu.fpscr==before.fpscr);
        CHECK(kernel.handles().OpenHandleCount()==handles && kernel.threads().size()==threads && kernel.now_ns()==0);
        CHECK(endpoint->save_archives().size()==saves && endpoint->archives().size()==shared);
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

void FormatCapability() {
    Fixture f;
#if defined(__linux__)
    CHECK(f.endpoint->save_archives().CanFormat());
#else
    CHECK(!f.endpoint->save_archives().CanFormat());
    f.Unsupported(Fixture::FormatRequest());
    CHECK(fs::is_empty(f.Root()));
#endif
}

#if defined(__linux__)
constexpr std::array<std::uint8_t,16> kAuthoredFormatMetadata{
    0,0x10,0,0,5,0,0,0,7,0,0,0,1,0,0,0};
void CheckFormatMetadata(const fs::path& title) {
    const auto bytes=Bytes(title.parent_path()/"00000001.metadata");
    CHECK(bytes.size()==kAuthoredFormatMetadata.size());
    if (bytes.size()==kAuthoredFormatMetadata.size())
        CHECK(std::memcmp(bytes.data(),kAuthoredFormatMetadata.data(),bytes.size())==0);
}

void FirstFormatAndTitleBinding() {
    // Exercise each supported publication boundary: missing high, low or data.
    for (unsigned existing=0;existing<3;++existing) {
        Fixture f;
        auto parent=f.Root();
        const std::array<const char*,2> names{"00040000","000ad500"};
        for (unsigned i=0;i<existing;++i) { parent/=names[i];fs::create_directory(parent); }
        Write(parent/"keep.bin","existing neighboring bytes");
        CHECK(f.Reply(Fixture::Request(),kResultFsNotFormatted)==0);
        f.FormatReply(Fixture::FormatRequest());
        CHECK(fs::is_directory(Title(f.Root())) && fs::is_empty(Title(f.Root())));
        CheckFormatMetadata(Title(f.Root()));
        CHECK(Bytes(parent/"keep.bin")=="existing neighboring bytes");
        CHECK(f.endpoint->save_archives().size()==0 && fs::is_empty(f.SharedRoot()));
        CHECK(f.Reply(Fixture::Request(),0)==(kSaveDataArchiveHandleTag|1));
        Write(Title(f.Root())/"save.bin","already saved");
        f.Unsupported(Fixture::FormatRequest());
        CHECK(Bytes(Title(f.Root())/"save.bin")=="already saved");
        CheckFormatMetadata(Title(f.Root()));
    }

    TempDir temp; SaveDataArchiveMounts backend;
    const SaveDataFormatInfo info{4096,5,7,true};
    CHECK(!backend.CanFormat());
    bool stopped=false;
    try { backend.FormatMissing(kProgram,info); } catch (const std::logic_error&) { stopped=true; }
    CHECK(stopped && fs::is_empty(temp.root));
    CHECK(backend.ConfigureRoot(temp.root) && backend.CanFormat());
    stopped=false;
    try { backend.FormatMissing(0,info); } catch (const std::logic_error&) { stopped=true; }
    CHECK(stopped && fs::is_empty(temp.root));
    for (const auto& invalid:std::array<SaveDataFormatInfo,4>{{
            {0,5,7,true},{kSaveDataFormatMaxBytes+512U,5,7,true},
            {4096,kSaveDataFormatMaxEntries+1,7,true},{4096,5,kSaveDataFormatMaxEntries+1,true}}}) {
        stopped=false;
        try { backend.FormatMissing(kProgram,invalid); } catch (const std::logic_error&) { stopped=true; }
        CHECK(stopped && fs::is_empty(temp.root));
    }
    CHECK(backend.FormatMissing(kProgram,info)==0);
    constexpr std::uint64_t other=0xABCDEF120034ABCDULL;
    CHECK(backend.FormatMissing(other,info)==0 && backend.size()==0);
    const auto other_title=temp.root/"abcdef12"/"0034abcd"/"data"/"00000001";
    CheckFormatMetadata(Title(temp.root)); CheckFormatMetadata(other_title);
    CHECK(fs::is_empty(Title(temp.root)) && fs::is_empty(other_title));
    std::uint64_t handle=0;
    CHECK(backend.Open(other,&handle)==0 && backend.Find(handle)->program_id==other);
}

struct FailingReplyDevice final : DeviceMemory {
    std::array<std::uint8_t,0x1000> bytes{};
    mutable unsigned reads{};
    unsigned writes{};
    std::uint32_t size() const noexcept override { return bytes.size(); }
    bool CanRead(std::uint32_t offset,std::uint32_t length) const noexcept override {
        return offset<=bytes.size() && length<=bytes.size()-offset;
    }
    bool CanWrite(std::uint32_t offset,std::uint32_t length) const noexcept override { return CanRead(offset,length); }
    bool Read(std::uint32_t offset,std::span<std::uint8_t> output) const noexcept override {
        ++reads;
        if (offset>bytes.size() || output.size()>bytes.size()-offset) return false;
        std::memcpy(output.data(),bytes.data()+offset,output.size()); return true;
    }
    bool Write(std::uint32_t,std::span<const std::uint8_t>) noexcept override { ++writes;return false; }
    std::uint64_t Epoch(std::uint32_t) const noexcept override { return 0; }
};

constexpr std::uint32_t kSaveFilePath=0x08001000,kSaveFilePayload=0x08002000;
IpcCommandBuffer SaveFileRequest(Fixture& f,std::uint64_t archive,std::u16string_view path) {
    std::vector<std::uint8_t> bytes;
    for (const auto unit:path) { bytes.push_back(unit&255);bytes.push_back(unit>>8); }
    bytes.push_back(0);bytes.push_back(0);
    if (!f.memory.IsMapped(kSaveFilePath,0x1000))
        CHECK(f.memory.Map(kSaveFilePath,0x1000,MemoryPermission::Read));
    CHECK(f.memory.LoadBytes(kSaveFilePath,bytes));
    const auto length=static_cast<std::uint32_t>(bytes.size());
    return {0x080201C2,0,static_cast<std::uint32_t>(archive),static_cast<std::uint32_t>(archive>>32),
            4,length,7,0,(length<<14)|2,kSaveFilePath};
}
Handle OpenSaveFile(Fixture& f,std::uint64_t archive,std::u16string_view path,
                    Result result=kResultSuccess) {
    const auto handles=f.kernel.handles().OpenHandleCount(),mounts=f.endpoint->save_archives().size();
    f.Put(SaveFileRequest(f,archive,path));
    CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==0);
    const auto reply=f.Read();const Handle file=result==0?reply[3]:0;
    CHECK((reply==IpcCommandBuffer{0x08020042,result,0x10,file}));
    CHECK((file!=0)==(result==0) && !f.router.unsupported_request());
    CHECK(f.kernel.handles().OpenHandleCount()==handles+(result==0));
    CHECK(f.endpoint->save_archives().size()==mounts && f.kernel.now_ns()==0);
    return file;
}
std::shared_ptr<FsFileService> SaveFileService(Fixture& f,Handle file) {
    const auto session=std::dynamic_pointer_cast<ClientSessionObject>(f.kernel.handles().Get(file));
    if (!session) throw std::runtime_error("missing authored save file session");
    const auto service=std::dynamic_pointer_cast<FsFileService>(session->service);
    if (!service) throw std::runtime_error("wrong authored save file service");
    CHECK(service->file().is_savedata());
    return service;
}
IpcCommandBuffer SaveWriteRequest(std::uint64_t offset,std::uint32_t length,
                                  std::uint32_t address=kSaveFilePayload) {
    return {0x08030102,static_cast<std::uint32_t>(offset),static_cast<std::uint32_t>(offset>>32),
            length,1,(length<<4)|0xA,address};
}
void SaveFileReply(Fixture& f,Handle file,const IpcCommandBuffer& request,const IpcCommandBuffer& reply) {
    const auto handles=f.kernel.handles().OpenHandleCount();
    f.Put(request);f.cpu.r[0]=file;const auto before=f.cpu;
    CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==0 && f.Read()==reply);
    for (unsigned i=1;i<15;++i) CHECK(f.cpu.r[i]==before.r[i]);
    CHECK(f.cpu.r[15]==before.r[15]+4 && f.cpu.cpsr==before.cpsr && f.cpu.fpscr==before.fpscr);
    CHECK(f.kernel.handles().OpenHandleCount()==handles && f.kernel.now_ns()==0);
}
void SaveFileStop(Fixture& f,Handle file,const IpcCommandBuffer& request) {
    const auto handles=f.kernel.handles().OpenHandleCount();
    f.Put(request);f.cpu.r[0]=file;const auto before=f.cpu;
    CHECK(f.Call().kind==a32::ExitKind::Svc && f.router.unsupported_request());
    CHECK(f.Read()==request && f.cpu.r==before.r && f.cpu.cpsr==before.cpsr && f.cpu.fpscr==before.fpscr);
    CHECK(f.kernel.handles().OpenHandleCount()==handles && f.kernel.now_ns()==0);
}
bool HasStagedSaveFile(const fs::path& directory) {
    for (const auto& entry:fs::directory_iterator(directory))
        if (entry.path().filename().string().starts_with(".lego-file-")) return true;
    return false;
}

void SaveFileLifecycleAndGrowth() {
    Fixture f;f.FormatReply(Fixture::FormatRequest());
    const auto archive=f.Reply(Fixture::Request(),0);
    const auto title=Title(f.Root());
    Write(title/"neighbor.bin","preserve neighboring data");
    const auto first=OpenSaveFile(f,archive,u"/profile.bin");
    CHECK(Bytes(title/"profile.bin").empty() && SaveFileService(f,first)->file().is_open());
    SaveFileReply(f,first,{0x08040000},{0x080400C0,0,0,0});
    const std::array<std::uint8_t,8> payload{'L','E','G','O',0,0x81,0xFE,0x7F};
    CHECK(f.memory.Map(kSaveFilePayload,payload.size(),MemoryPermission::Read));
    CHECK(f.memory.LoadBytes(kSaveFilePayload,payload));
    SaveFileReply(f,first,SaveWriteRequest(0,8),{0x08030082,0,8,0x8A,kSaveFilePayload});
    std::string expected(reinterpret_cast<const char*>(payload.data()),payload.size());
    CHECK(Bytes(title/"profile.bin")==expected);
    const auto second=OpenSaveFile(f,archive,u"/profile.bin");
    CHECK(first!=second && SaveFileService(f,first)!=SaveFileService(f,second));
    CHECK(Bytes(title/"profile.bin")==expected); // Mode 7 does not truncate.
    SaveFileReply(f,second,SaveWriteRequest(12,4,kSaveFilePayload+4),
                  {0x08030082,0,4,0x4A,kSaveFilePayload+4});
    expected+=std::string(4,'\0');expected.append(reinterpret_cast<const char*>(payload.data()+4),4);
    CHECK(Bytes(title/"profile.bin")==expected); // Real gap and growth, not EOF clipping.
    for (const auto file:{first,second})
        SaveFileReply(f,file,{0x08040000},{0x080400C0,0,16,0});
    SaveFileReply(f,first,SaveWriteRequest(16,1),{0x08030082,0,1,0x1A,kSaveFilePayload});
    expected+='L';CHECK(Bytes(title/"profile.bin")==expected);
    SaveFileStop(f,second,SaveWriteRequest(kMaxSaveDataFileBytes,1));
    CHECK(Bytes(title/"profile.bin")==expected && !f.router.last_host_error().empty());
    Handle duplicate=0;CHECK(f.kernel.handles().Duplicate(&duplicate,first)==0);
    SaveFileReply(f,duplicate,{0x08080000},{0x08080040,0});
    CHECK(!SaveFileService(f,first)->file().is_open() && SaveFileService(f,second)->file().is_open());
    SaveFileReply(f,first,{0x08080000},{0x08080040,0});
    SaveFileReply(f,second,{0x08040000},{0x080400C0,0,17,0});
    SaveFileStop(f,first,SaveWriteRequest(0,1));
    SaveFileStop(f,second,{0x080200C2}); // Read is still an unsupported command.
    CHECK(f.kernel.CloseHandle(first)==0 && f.kernel.CloseHandle(duplicate)==0);
    CHECK(f.kernel.CloseHandle(second)==0);
    const auto reopened=OpenSaveFile(f,archive,u"/profile.bin");
    SaveFileReply(f,reopened,{0x08040000},{0x080400C0,0,17,0});
    CHECK(Bytes(title/"profile.bin")==expected && Bytes(title/"neighbor.bin")=="preserve neighboring data");
    CHECK(!HasStagedSaveFile(title) && fs::is_empty(f.SharedRoot()));CheckFormatMetadata(title);
}

void SaveFilePublicationAndContainment() {
    TempDir temp,outside;const auto title=Title(temp.root);
    fs::create_directories(title/"nested");Write(outside.root/"external.bin","outside bytes");
    SaveDataArchiveMounts backend;CHECK(backend.ConfigureRoot(temp.root));
    std::uint64_t archive=0;CHECK(backend.Open(kProgram,&archive)==0 && backend.CanOpenFiles());
    for (const auto [handle,program]:std::array<std::pair<std::uint64_t,std::uint64_t>,3>{{
            {1,kProgram},{archive+1,kProgram},{archive,kProgram+1}}}) {
        SaveDataFileOpenPlan plan;
        CHECK(backend.StageOpenFile(handle,program,u"/forbidden.bin",7,plan)==kResultFsInvalidArchiveHandle);
        CHECK(!plan.TakeFile() && plan.Rollback());
    }
    fs::create_symlink(outside.root/"external.bin",title/"leaf-link.bin");
    fs::create_directory_symlink(outside.root,title/"parent-link");
    fs::create_hard_link(outside.root/"external.bin",title/"hard-link.bin");
    Write(title/"parent-file","ordinary file");
    const std::u16string malformed{u'/',static_cast<char16_t>(0xD800),u'x'};
    const std::array<std::pair<std::u16string_view,Result>,9> cases{{
        {u"/../external.bin",kResultFsInvalidPath},{u"/leaf-link.bin",kResultFsInvalidPath},
        {u"/parent-link/new.bin",kResultFsInvalidPath},{u"/hard-link.bin",kResultFsInvalidPath},
        {u"/missing/new.bin",kResultFsPathNotFound},{u"/parent-file/new.bin",kResultFsUnexpectedFileOrDirectory},
        {u"/nested",kResultFsUnexpectedFileOrDirectory},{u"/bad//name.bin",kResultFsInvalidPath},
        {malformed,kResultFsInvalidPath}}};
    for (const auto& [path,result]:cases) {
        SaveDataFileOpenPlan plan;CHECK(backend.StageOpenFile(archive,kProgram,path,7,plan)==result);
        CHECK(!plan.TakeFile() && plan.Rollback());
    }
    {
        SaveDataFileOpenPlan plan;CHECK(backend.StageOpenFile(archive,kProgram,u"/contended.bin",7,plan)==0);
        auto file=plan.TakeFile();CHECK(file && file->is_open() && file->size()==0);
        CHECK(!fs::exists(title/"contended.bin") && HasStagedSaveFile(title));
        Write(title/"contended.bin","concurrent existing data");
        bool stopped=false;try { plan.Publish(); } catch (const std::system_error&) { stopped=true; }
        CHECK(stopped && plan.Rollback() && !HasStagedSaveFile(title));
        CHECK(Bytes(title/"contended.bin")=="concurrent existing data");
    }
    {
        SaveDataFileOpenPlan plan;CHECK(backend.StageOpenFile(archive,kProgram,u"/contended.bin",7,plan)==0);
        auto file=plan.TakeFile();CHECK(file && file->size()==24 && plan.Rollback());
        // Failure/abandonment after an existing open never unlinks that leaf.
    }
    {
        SaveDataFileOpenPlan plan;CHECK(backend.StageOpenFile(archive,kProgram,u"/abandoned.bin",7,plan)==0);
    }
    CHECK(!fs::exists(title/"abandoned.bin") && !HasStagedSaveFile(title));
    CHECK(Bytes(title/"contended.bin")=="concurrent existing data");
    const auto moved=title.parent_path()/"retained-title";
    fs::rename(title,moved);fs::create_directory_symlink(outside.root,title);
    SaveDataFileOpenPlan plan;CHECK(backend.StageOpenFile(archive,kProgram,u"/nested/caf\u00E9.bin",7,plan)==0);
    auto file=plan.TakeFile();plan.Publish();
    CHECK(file && file->size()==0 && fs::is_regular_file(moved/"nested"/"caf\xC3\xA9.bin"));
    CHECK(!fs::exists(outside.root/"nested") && Bytes(outside.root/"external.bin")=="outside bytes");
}

void SaveFilePreflightAndCapacity() {
    Fixture f;f.FormatReply(Fixture::FormatRequest());const auto archive=f.Reply(Fixture::Request(),0);
    const auto title=Title(f.Root());
    auto request=SaveFileRequest(f,archive,u"/prepared.bin");
    for (const auto [word,value]:std::array<std::pair<unsigned,std::uint32_t>,4>{{
            {0,0x080201C0},{6,3},{7,1},{8,request[8]^0x400U}}}) {
        auto bad=request;bad[word]=value;f.Unsupported(bad);
    }
    CHECK(fs::is_empty(title));
    CHECK(f.memory.LoadBytes(kSaveFilePath+request[5]-2,std::array<std::uint8_t,2>{'X',0}));
    f.Put(request);CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==0);
    CHECK((f.Read()==IpcCommandBuffer{0x08020042,kResultFsInvalidPath,0x10,0}) && fs::is_empty(title));
    request=SaveFileRequest(f,archive,u"/prepared.bin");
    auto bad=request;bad[9]=0xFFFFFFFE;f.Put(bad);
    CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer);
    CHECK(f.Read()==bad && fs::is_empty(title));
    OpenSaveFile(f,archive+1,u"/prepared.bin",kResultFsInvalidArchiveHandle);

    // A reply callback must not get a chance to fail after file publication,
    // data writes, or descriptor Close. Exercise all three on the real IPC path.
    std::array<std::uint8_t,20> payload{};payload.fill(0xA6);
    CHECK(f.memory.Map(kSaveFilePayload,payload.size(),MemoryPermission::Read));
    CHECK(f.memory.LoadBytes(kSaveFilePayload,payload));
    Handle file=0;
    for (unsigned step=0;step<3;++step) {
        const auto q=step==0?SaveFileRequest(f,archive,u"/prepared.bin"):
                     step==1?SaveWriteRequest(0,payload.size()):IpcCommandBuffer{0x08080000};
        GuestMemory alternate;
        CHECK(alternate.Map(kSaveFilePath,0x1000,MemoryPermission::Read));
        std::vector<std::uint8_t> path(request[5]);
        for (unsigned i=0;i<path.size();++i) CHECK(f.memory.Read8(kSaveFilePath+i,&path[i]));
        CHECK(alternate.LoadBytes(kSaveFilePath,path));
        CHECK(alternate.Map(kSaveFilePayload,payload.size(),MemoryPermission::Read));
        CHECK(alternate.LoadBytes(kSaveFilePayload,payload));
        auto device=std::make_shared<FailingReplyDevice>();
        const auto base=f.cb()&~0xFFFU,offset=f.cb()&0xFFFU;
        std::memcpy(device->bytes.data()+offset,q.data(),sizeof(q));
        CHECK(alternate.MapDeviceMemory(base,device,RW));
        f.Put(q);if (step) f.cpu.r[0]=file;const auto before=f.cpu;
        const auto handles=f.kernel.handles().OpenHandleCount();
        CHECK(f.Call(&alternate).kind==a32::ExitKind::Svc && f.router.unsupported_request());
        CHECK(f.cpu.r==before.r && device->writes==0 && f.kernel.handles().OpenHandleCount()==handles);
        CHECK(std::memcmp(device->bytes.data()+offset,q.data(),sizeof(q))==0);
        if (step==0) {
            CHECK(fs::is_empty(title));file=OpenSaveFile(f,archive,u"/prepared.bin");
        } else CHECK(Bytes(title/"prepared.bin").empty() && SaveFileService(f,file)->file().is_open());
    }
    auto source=std::make_shared<FailingReplyDevice>();
    CHECK(f.memory.MapDeviceMemory(0x18000000,source,MemoryPermission::Read));
    SaveFileStop(f,file,SaveWriteRequest(0,20,0x18000000));
    CHECK(source->reads==0 && Bytes(title/"prepared.bin").empty());

    // Growable writes report a real partial host write without a success reply.
    const auto child=::fork();if (child<0) throw std::runtime_error("fork failed");
    if (child==0) {
        struct rlimit limit{};
        if (::getrlimit(RLIMIT_FSIZE,&limit)!=0 || limit.rlim_max<8) _exit(90);
        limit.rlim_cur=8;
        if (::setrlimit(RLIMIT_FSIZE,&limit)!=0 || std::signal(SIGXFSZ,SIG_IGN)==SIG_ERR) _exit(90);
        const auto q=SaveWriteRequest(0,payload.size());f.Put(q);f.cpu.r[0]=file;const auto before=f.cpu;
        const bool good=f.Call().kind==a32::ExitKind::Svc && f.cpu.r==before.r && f.Read()==q &&
            f.router.last_host_error().find("after 8 bytes; host changes retained")!=std::string::npos;
        _exit(good?0:91);
    }
    int status=0;CHECK(::waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    CHECK(Bytes(title/"prepared.bin")==std::string(8,char(0xA6)));
    SaveFileReply(f,file,{0x08040000},{0x080400C0,0,8,0});

    Fixture full;full.FormatReply(Fixture::FormatRequest());const auto full_archive=full.Reply(Fixture::Request(),0);
    Handle spare=0;
    const auto missing=HandleTable::kMaxCount-full.kernel.handles().OpenHandleCount();
    for (std::size_t i=0;i<missing;++i)
        CHECK(full.kernel.handles().Duplicate(&spare,full.handle)==0);
    auto expected_table=full.kernel.handles();
    OpenSaveFile(full,full_archive,u"/capacity.bin",kResultOutOfHandles);
    CHECK(fs::is_empty(Title(full.Root())));
    CHECK(full.kernel.CloseHandle(spare)==0 && expected_table.Close(spare)==0);
    Handle expected_handle=0;CHECK(expected_table.Duplicate(&expected_handle,full.handle)==0);
    CHECK(OpenSaveFile(full,full_archive,u"/capacity.bin")==expected_handle);
    CHECK(Bytes(Title(full.Root())/"capacity.bin").empty() && !HasStagedSaveFile(Title(full.Root())));
}

constexpr std::uint32_t kCommitInput=0x08003001,kCommitOutput=0x08004001;
IpcCommandBuffer CommitRequest(std::uint64_t archive) {
    return {0x080D0144,static_cast<std::uint32_t>(archive),static_cast<std::uint32_t>(archive>>32),
            0,1,1,0x1A,kCommitInput,0x1C,kCommitOutput};
}
void CommitBuffers(GuestMemory& memory) {
    CHECK(memory.Map(kCommitInput-1,3,MemoryPermission::Read));
    CHECK(memory.Map(kCommitOutput-1,3,RW));
    CHECK(memory.LoadBytes(kCommitInput-1,std::array<std::uint8_t,3>{0x49,0x87,0x63}));
    CHECK(memory.LoadBytes(kCommitOutput-1,std::array<std::uint8_t,3>{0x5B,0xA7,0x3E}));
}
std::array<std::uint8_t,3> CommitBytes(GuestMemory& memory,std::uint32_t address) {
    std::array<std::uint8_t,3> bytes{};
    for (unsigned i=0;i<bytes.size();++i) CHECK(memory.Read8(address-1+i,&bytes[i]));
    return bytes;
}
std::map<std::string,std::string> CommitInventory(const fs::path& root) {
    std::map<std::string,std::string> result;
    for (const auto& entry:fs::recursive_directory_iterator(root)) {
        const auto path=entry.path().lexically_relative(root).generic_string();
        if (entry.is_symlink()) result[path]="link:"+fs::read_symlink(entry.path()).generic_string();
        else if (entry.is_directory()) result[path]="directory";
        else if (entry.is_regular_file()) result[path]="file:"+Bytes(entry.path());
        else result[path]="other";
    }
    return result;
}
void CommitReply(Fixture& f,const IpcCommandBuffer& request,Result result) {
    const auto handles=f.kernel.handles().OpenHandleCount(),mounts=f.endpoint->save_archives().size();
    const auto inventory=CommitInventory(f.temp.root);
    f.Put(request);const auto before=f.cpu;
    CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==0 && !f.router.unsupported_request());
    CHECK((f.Read()==IpcCommandBuffer{0x080D0040,result}));
    for (unsigned i=1;i<15;++i) CHECK(f.cpu.r[i]==before.r[i]);
    CHECK(f.cpu.r[15]==before.r[15]+4 && f.cpu.cpsr==before.cpsr && f.cpu.fpscr==before.fpscr);
    CHECK(f.kernel.handles().OpenHandleCount()==handles && f.endpoint->save_archives().size()==mounts);
    CHECK(f.kernel.now_ns()==0 && CommitInventory(f.temp.root)==inventory);
}

void SaveCommitAcknowledgementAndOwnership() {
    Fixture f;f.FormatReply(Fixture::FormatRequest());const auto archive=f.Reply(Fixture::Request(),0);
    Write(Title(f.Root())/"progress.bin","authored saved progress remains byte-for-byte intact");
    CommitBuffers(f.memory);const auto input=CommitBytes(f.memory,kCommitInput);
    CommitReply(f,CommitRequest(archive),0);
    CHECK(CommitBytes(f.memory,kCommitInput)==input);
    CHECK((CommitBytes(f.memory,kCommitOutput)==std::array<std::uint8_t,3>{0x5B,0,0x3E}));
    CheckFormatMetadata(Title(f.Root()));
    CHECK(f.memory.LoadBytes(kCommitOutput,std::array<std::uint8_t,1>{0xA7}));
    const auto untouched=CommitBytes(f.memory,kCommitOutput);
    CommitReply(f,CommitRequest(archive+99),kResultFsInvalidArchiveHandle);
    CHECK(CommitBytes(f.memory,kCommitOutput)==untouched && CommitBytes(f.memory,kCommitInput)==input);

    // Host fixture setup opens a different title through the real backend. The
    // underlying object is mutable; its public diagnostic view is const-only.
    const auto other=f.Root()/"00040000"/"000ad501"/"data"/"00000001";
    fs::create_directories(other);Write(other/"foreign.bin","another title is not selected");
    auto& backend=const_cast<SaveDataArchiveMounts&>(f.endpoint->save_archives());
    std::uint64_t foreign=0;CHECK(backend.Open(kProgram+1,&foreign)==0);
    CHECK(backend.Find(foreign) && backend.Find(foreign)->program_id==kProgram+1);
    CommitReply(f,CommitRequest(foreign),kResultFsInvalidArchiveHandle);
    CHECK(CommitBytes(f.memory,kCommitOutput)==untouched && CommitBytes(f.memory,kCommitInput)==input);
    CommitReply(f,CommitRequest(archive),0); // Repeated acknowledgement leaves storage untouched.
    CHECK((CommitBytes(f.memory,kCommitOutput)==std::array<std::uint8_t,3>{0x5B,0,0x3E}));
}

void SaveCommitRequestAndReplyPreflight() {
    Fixture unconfigured(false);unconfigured.Unsupported(CommitRequest(kSaveDataArchiveHandleTag|1));
    Fixture uninitialized(true,false);uninitialized.Unsupported(CommitRequest(kSaveDataArchiveHandleTag|1));
    Fixture f;f.FormatReply(Fixture::FormatRequest());const auto archive=f.Reply(Fixture::Request(),0);
    Write(Title(f.Root())/"preserve.bin","real existing bytes");CommitBuffers(f.memory);
    const auto input=CommitBytes(f.memory,kCommitInput),output=CommitBytes(f.memory,kCommitOutput);
    const auto inventory=CommitInventory(f.temp.root);
    const auto request=CommitRequest(archive);
    for (const auto [word,value]:std::array<std::pair<unsigned,std::uint32_t>,9>{{
            {0,0x080D0142},{2,0},{3,1},{4,0},{4,2},{5,0},{5,2},{6,0x1E},{8,0x1A}}}) {
        auto bad=request;bad[word]=value;f.Unsupported(bad);
    }
    CHECK(f.memory.Map(0x08100000,1,MemoryPermission::Read));
    CHECK(f.memory.LoadBytes(0x08100000,std::array<std::uint8_t,1>{0x56}));
    for (const auto [word,value]:std::array<std::pair<unsigned,std::uint32_t>,3>{{
            {7,0},{9,0},{9,0x08100000}}}) {
        auto bad=request;bad[word]=value;f.Put(bad);
        CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer);
        CHECK(f.Read()==bad);
    }
    // A device source/output must not receive a callback. Shared service bytes
    // are also outside this bounded private-memory command.
    auto device=std::make_shared<FailingReplyDevice>();device->bytes[0]=0x69;
    CHECK(f.memory.MapDeviceMemory(0x18000000,device,RW));
    auto shared=std::make_shared<ServiceSharedMemoryObject>();
    CHECK(shared->Write(0,std::array<std::uint8_t,1>{0x73}));
    CHECK(f.memory.MapSharedServicePage(0x18100000,shared,RW));
    const auto epoch=shared->Epoch(0);
    for (unsigned word:{7U,9U}) {
        auto bad=request;bad[word]=0x18000000;f.Unsupported(bad);
        bad[word]=0x18100000;f.Unsupported(bad);
    }
    CHECK(device->reads==0 && device->writes==0 && device->bytes[0]==0x69);
    CHECK(shared->bytes()[0]==0x73 && shared->Epoch(0)==epoch);
    for (unsigned mode=0;mode<3;++mode) {
        GuestMemory alternate;CommitBuffers(alternate);
        const auto length=mode==1?sizeof(IpcCommandBuffer)-4:sizeof(IpcCommandBuffer);
        auto reply_device=std::make_shared<FailingReplyDevice>();
        if (mode==2) {
            const auto base=f.cb()&~0xFFFU,offset=f.cb()&0xFFFU;
            std::memcpy(reply_device->bytes.data()+offset,request.data(),sizeof(request));
            CHECK(alternate.MapDeviceMemory(base,reply_device,RW));
        } else {
            CHECK(alternate.Map(f.cb(),length,mode==0?MemoryPermission::Read:RW));
            CHECK(alternate.LoadBytes(f.cb(),{reinterpret_cast<const std::uint8_t*>(request.data()),length}));
        }
        f.Put(request);const auto before=f.cpu;const auto handles=f.kernel.handles().OpenHandleCount();
        const auto result=f.Call(&alternate);
        if (mode==2) CHECK(result.kind==a32::ExitKind::Svc && f.cpu.r==before.r && f.router.unsupported_request());
        else CHECK(result.kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer);
        std::uint32_t header=0;CHECK(alternate.Read32(f.cb(),&header) && header==request[0]);
        CHECK(reply_device->writes==0 && f.kernel.handles().OpenHandleCount()==handles);
        CHECK(CommitBytes(alternate,kCommitInput)==input && CommitBytes(alternate,kCommitOutput)==output);
    }
    // Distinct virtual addresses can still target the same physical reply.
    constexpr std::uint32_t reply_page=0x08008000,alias_page=0x0E100000;
    CHECK(f.memory.Map(reply_page,0x1000,RW));
    CHECK(f.memory.MapUserAlias(alias_page,reply_page,0x1000,3)==0);
    f.kernel.current_thread()->tls_address=reply_page;
    for (unsigned word:{7U,9U}) {
        auto bad=request;bad[word]=alias_page+kIpcCommandBufferOffset;f.Unsupported(bad);
    }
    CHECK(CommitBytes(f.memory,kCommitInput)==input && CommitBytes(f.memory,kCommitOutput)==output);
    CHECK(CommitInventory(f.temp.root)==inventory);
}

IpcCommandBuffer CloseSaveRequest(std::uint64_t archive) {
    return {0x080E0080,static_cast<std::uint32_t>(archive),static_cast<std::uint32_t>(archive>>32)};
}
void CloseSaveReply(Fixture& f,std::uint64_t archive,Result result=kResultSuccess) {
    const auto handles=f.kernel.handles().OpenHandleCount(),mounts=f.endpoint->save_archives().size();
    const auto shared=f.endpoint->archives().size();const auto inventory=CommitInventory(f.temp.root);
    f.Put(CloseSaveRequest(archive));const auto before=f.cpu;
    CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==0 && !f.router.unsupported_request());
    CHECK((f.Read()==IpcCommandBuffer{0x080E0040,result}));
    for (unsigned i=1;i<15;++i) CHECK(f.cpu.r[i]==before.r[i]);
    CHECK(f.cpu.r[15]==before.r[15]+4 && f.cpu.cpsr==before.cpsr && f.cpu.fpscr==before.fpscr);
    CHECK(f.endpoint->save_archives().size()==mounts-(result==0));
    CHECK(f.endpoint->archives().size()==shared && f.kernel.handles().OpenHandleCount()==handles);
    CHECK(f.kernel.now_ns()==0 && CommitInventory(f.temp.root)==inventory);
}

void SaveArchiveCloseAndLiveFiles() {
    Fixture f;f.FormatReply(Fixture::FormatRequest());
    const auto first=f.Reply(Fixture::Request(),0),other=f.Reply(Fixture::Request(),0);
    CHECK(first!=other && f.endpoint->save_archives().size()==2);
    Write(Title(f.Root())/"neighbor.bin","preserve this file and its directory");
    const auto file=OpenSaveFile(f,first,u"/live.bin");
    const std::array<std::uint8_t,5> payload{'A','R',0,0x93,0x7D};
    CHECK(f.memory.Map(kSaveFilePayload,payload.size(),MemoryPermission::Read));
    CHECK(f.memory.LoadBytes(kSaveFilePayload,payload));
    SaveFileReply(f,file,SaveWriteRequest(0,3),{0x08030082,0,3,0x3A,kSaveFilePayload});
    CloseSaveReply(f,first);
    CHECK(!f.endpoint->save_archives().Find(first) && f.endpoint->save_archives().Find(other));
    CHECK(SaveFileService(f,file)->file().is_open());
    CloseSaveReply(f,first,kResultFsInvalidArchiveHandle);
    OpenSaveFile(f,first,u"/stale.bin",kResultFsInvalidArchiveHandle);
    CHECK(!fs::exists(Title(f.Root())/"stale.bin"));
    SaveFileReply(f,file,SaveWriteRequest(3,5),{0x08030082,0,5,0x5A,kSaveFilePayload});
    SaveFileReply(f,file,{0x08040000},{0x080400C0,0,8,0});
    std::string expected(reinterpret_cast<const char*>(payload.data()),3);
    expected.append(reinterpret_cast<const char*>(payload.data()),payload.size());
    CHECK(Bytes(Title(f.Root())/"live.bin")==expected);
    const auto reopened=f.Reply(Fixture::Request(),0);
    CHECK(reopened==other+1 && reopened!=first && f.endpoint->save_archives().Find(other));
    const auto new_file=OpenSaveFile(f,reopened,u"/live.bin");
    SaveFileReply(f,new_file,{0x08040000},{0x080400C0,0,8,0});
    CloseSaveReply(f,other);CHECK(f.endpoint->save_archives().Find(reopened));
    CloseSaveReply(f,reopened);CHECK(f.endpoint->save_archives().size()==0);
    for (const auto endpoint:{file,new_file}) {
        SaveFileReply(f,endpoint,{0x08040000},{0x080400C0,0,8,0});
        SaveFileReply(f,endpoint,{0x08080000},{0x08080040,0});
        CHECK(f.kernel.CloseHandle(endpoint)==0);
    }
    CHECK(Bytes(Title(f.Root())/"live.bin")==expected);
    CHECK(Bytes(Title(f.Root())/"neighbor.bin")=="preserve this file and its directory");
    CheckFormatMetadata(Title(f.Root()));
}

void SaveArchiveCloseOwnershipAndReplyPreflight() {
    Fixture unconfigured(false);unconfigured.Unsupported(CloseSaveRequest(kSaveDataArchiveHandleTag|1));
    Fixture uninitialized(true,false);uninitialized.Unsupported(CloseSaveRequest(kSaveDataArchiveHandleTag|1));
    Fixture f;f.FormatReply(Fixture::FormatRequest());const auto archive=f.Reply(Fixture::Request(),0);
    Write(Title(f.Root())/"keep.bin","real file bytes stay intact");
    const auto foreign_path=f.Root()/"00040000"/"000ad501"/"data"/"00000001";
    fs::create_directories(foreign_path);Write(foreign_path/"foreign.bin","foreign title remains mounted");
    auto& backend=const_cast<SaveDataArchiveMounts&>(f.endpoint->save_archives());
    std::uint64_t foreign=0;CHECK(backend.Open(kProgram+1,&foreign)==0);
    CHECK(backend.Close(archive,0)==kResultFsInvalidArchiveHandle);
    CloseSaveReply(f,foreign,kResultFsInvalidArchiveHandle);
    CloseSaveReply(f,archive+123,kResultFsInvalidArchiveHandle);
    CHECK(backend.Find(foreign) && backend.Find(archive));
    auto bad=CloseSaveRequest(archive);bad[0]=0x080E0082;f.Unsupported(bad);
    bad=CloseSaveRequest(archive);bad[2]=0;f.Unsupported(bad); // SharedExtData close stays unsupported.
    const auto request=CloseSaveRequest(archive);const auto inventory=CommitInventory(f.temp.root);
    for (unsigned mode=0;mode<4;++mode) {
        GuestMemory alternate;
        auto device=std::make_shared<FailingReplyDevice>();
        auto shared=std::make_shared<ServiceSharedMemoryObject>();
        const auto base=f.cb()&~0xFFFU,offset=f.cb()&0xFFFU;
        const auto length=mode==1?sizeof(IpcCommandBuffer)-4:sizeof(IpcCommandBuffer);
        const auto data=std::span(reinterpret_cast<const std::uint8_t*>(request.data()),sizeof(request));
        if (mode==2) {
            std::memcpy(device->bytes.data()+offset,request.data(),sizeof(request));
            CHECK(alternate.MapDeviceMemory(base,device,RW));
        } else if (mode==3) {
            CHECK(shared->Write(offset,data));CHECK(alternate.MapSharedServicePage(base,shared,RW));
        } else {
            CHECK(alternate.Map(f.cb(),length,mode==0?MemoryPermission::Read:RW));
            CHECK(alternate.LoadBytes(f.cb(),data.first(length)));
        }
        const auto epoch=shared->Epoch(offset);const auto handles=f.kernel.handles().OpenHandleCount();
        f.Put(request);const auto before=f.cpu;const auto result=f.Call(&alternate);
        if (mode>=2) CHECK(result.kind==a32::ExitKind::Svc && f.cpu.r==before.r && f.router.unsupported_request());
        else CHECK(result.kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer);
        std::uint32_t header=0;CHECK(alternate.Read32(f.cb(),&header) && header==request[0]);
        CHECK(device->writes==0 && shared->Epoch(offset)==epoch);
        CHECK(backend.Find(archive) && backend.Find(foreign) && backend.size()==2);
        CHECK(f.kernel.handles().OpenHandleCount()==handles && CommitInventory(f.temp.root)==inventory);
    }
    CloseSaveReply(f,archive);
    CHECK(backend.Find(foreign) && backend.size()==1 && CommitInventory(f.temp.root)==inventory);
}

void FormatRequestAndReplyPreflight() {
    Fixture unconfigured(false); unconfigured.Unsupported(Fixture::FormatRequest());
    CHECK(fs::is_empty(unconfigured.Root()));
    Fixture f(true,false); f.Unsupported(Fixture::FormatRequest()); f.NewSession();
    for (const auto& [word,value]:std::array<std::pair<unsigned,std::uint32_t>,12>{{
            {0,0x084C0240},{1,7},{2,2},{3,2},{4,0},{4,0xFFFFFFFF},
            {5,kSaveDataFormatMaxEntries+1},{6,kSaveDataFormatMaxEntries+1},
            {7,kSaveDataFormatMaxBuckets+1},{8,kSaveDataFormatMaxBuckets+1},{9,2},{10,0x4802}}}) {
        auto request=Fixture::FormatRequest();request[word]=value;f.Unsupported(request);
        CHECK(fs::is_empty(f.Root()));
    }
    for (const auto address:{0U,0x08000000U,Fixture::input+1}) {
        const auto request=Fixture::FormatRequest(address); f.Put(request);
        CHECK(f.Call().kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer);
        CHECK(f.Read()==request && fs::is_empty(f.Root()));
    }
    const auto request=Fixture::FormatRequest();
    for (unsigned mode=0;mode<2;++mode) {
        GuestMemory protected_memory;
        const auto size=mode?sizeof(IpcCommandBuffer)-4:sizeof(IpcCommandBuffer);
        CHECK(protected_memory.Map(f.cb(),size,mode?RW:MemoryPermission::Read));
        CHECK(protected_memory.Map(Fixture::input,1,MemoryPermission::Read));
        CHECK(protected_memory.LoadBytes(Fixture::input,std::array<std::uint8_t,1>{0x7D}));
        CHECK(protected_memory.LoadBytes(f.cb(),{reinterpret_cast<const std::uint8_t*>(request.data()),size}));
        f.Put(request);
        CHECK(f.Call(&protected_memory).kind==a32::ExitKind::Fallthrough && f.cpu.r[0]==kResultInvalidPointer);
        CHECK(fs::is_empty(f.Root()) && f.Read()==request);
    }
    // A callback can claim writability and still reject the eventual reply.
    // The format handler must stop before filesystem publication or any write.
    for (bool shared_reply:{false,true}) {
        GuestMemory alternate;
        CHECK(alternate.Map(Fixture::input,1,MemoryPermission::Read));
        CHECK(alternate.LoadBytes(Fixture::input,std::array<std::uint8_t,1>{0x7D}));
        const auto base=f.cb()&~0xFFFU,offset=f.cb()&0xFFFU;
        auto device=std::make_shared<FailingReplyDevice>();
        auto shared=std::make_shared<ServiceSharedMemoryObject>();
        const auto data=std::span(reinterpret_cast<const std::uint8_t*>(request.data()),sizeof(request));
        if (shared_reply) {
            CHECK(shared->Write(offset,data)); CHECK(alternate.MapSharedServicePage(base,shared,RW));
        } else {
            std::memcpy(device->bytes.data()+offset,data.data(),data.size());
            CHECK(alternate.MapDeviceMemory(base,device,RW));
        }
        f.Put(request); const auto before=f.cpu;
        CHECK(f.Call(&alternate).kind==a32::ExitKind::Svc && f.router.unsupported_request());
        CHECK(f.cpu.r==before.r && fs::is_empty(f.Root()) && device->writes==0);
        const auto after=shared_reply?shared->bytes():std::span<const std::uint8_t>(device->bytes);
        CHECK(std::memcmp(after.data()+offset,data.data(),data.size())==0);
    }
    auto path_device=std::make_shared<FailingReplyDevice>();
    CHECK(f.memory.MapDeviceMemory(0x18000000,path_device,MemoryPermission::Read));
    f.Unsupported(Fixture::FormatRequest(0x18000000));
    CHECK(path_device->reads==0 && path_device->writes==0 && fs::is_empty(f.Root()));
    // Empty's byte is not required to be NUL; the one-byte end-of-space span is valid.
    CHECK(f.memory.Map(0xFFFFFFFF,1,MemoryPermission::Read));
    CHECK(f.memory.LoadBytes(0xFFFFFFFF,std::array<std::uint8_t,1>{0xFF}));
    f.FormatReply(Fixture::FormatRequest(0xFFFFFFFF)); CheckFormatMetadata(Title(f.Root()));
}

void FormatContainmentAndExistingData() {
    const SaveDataFormatInfo info{4096,5,7,true};
    for (unsigned blocked=0;blocked<3;++blocked) {
        TempDir temp,outside; SaveDataArchiveMounts backend;
        CHECK(backend.ConfigureRoot(temp.root));
        auto parent=temp.root;
        const std::array<const char*,3> names{"00040000","000ad500","data"};
        for (unsigned i=0;i<blocked;++i) { parent/=names[i];fs::create_directory(parent); }
        Write(outside.root/"sentinel","unchanged");
        fs::create_directory_symlink(outside.root,parent/names[blocked]);
        CHECK(backend.FormatMissing(kProgram,info)==kResultFsInvalidPath && backend.size()==0);
        fs::remove(parent/names[blocked]);Write(parent/names[blocked],"not a directory");
        CHECK(backend.FormatMissing(kProgram,info)==kResultFsInvalidPath);
        CHECK(Bytes(parent/names[blocked])=="not a directory" && Bytes(outside.root/"sentinel")=="unchanged");
        CHECK(!fs::exists(Title(outside.root)));
    }
    Fixture empty_data;fs::create_directories(Title(empty_data.Root()).parent_path());
    empty_data.Unsupported(Fixture::FormatRequest());
    CHECK(fs::is_empty(Title(empty_data.Root()).parent_path()));
    TempDir temp,outside;
    const auto selected=temp.root/"container",moved=temp.root/"moved";
    fs::create_directory(selected);Write(outside.root/"sentinel","outside");
    SaveDataArchiveMounts backend;CHECK(backend.ConfigureRoot(selected));
    fs::rename(selected,moved);fs::create_directory_symlink(outside.root,selected);
    CHECK(backend.FormatMissing(kProgram,info)==0);
    CheckFormatMetadata(Title(moved));
    CHECK(!fs::exists(Title(outside.root)) && Bytes(outside.root/"sentinel")=="outside");
}

void PartialMetadataFailureRollsBack() {
    Fixture f;
    const auto child=::fork();
    if (child<0) throw std::runtime_error("fork failed");
    if (child==0) {
        // Force a real partial metadata write, after all parent staging exists.
        // Limit and signal disposition affect only this isolated child.
        struct rlimit limit{};
        if (::getrlimit(RLIMIT_FSIZE,&limit)!=0 || limit.rlim_max<8) _exit(90);
        limit.rlim_cur=8;
        if (::setrlimit(RLIMIT_FSIZE,&limit)!=0 || std::signal(SIGXFSZ,SIG_IGN)==SIG_ERR) _exit(90);
        const auto request=Fixture::FormatRequest();f.Put(request);const auto before=f.cpu;
        const auto handles=f.kernel.handles().OpenHandleCount();
        const bool good=f.Call().kind==a32::ExitKind::Svc && f.router.unsupported_request() &&
            !f.router.last_host_error().empty() && f.Read()==request && f.cpu.r==before.r &&
            f.endpoint->save_archives().size()==0 && f.kernel.handles().OpenHandleCount()==handles &&
            fs::is_empty(f.Root());
        _exit(good?0:91);
    }
    int status=0;CHECK(::waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    CHECK(fs::is_empty(f.Root()) && f.endpoint->save_archives().size()==0);
    f.FormatReply(Fixture::FormatRequest());CheckFormatMetadata(Title(f.Root()));
}
#endif
} // namespace

int main() {
    BackendConfigurationAndBinding(); MissingReplyAndIgnoredEmptyByte(); ExistingSaveAndSessionIsolation();
    RequestAndReplyPreflight(); DistinctSharedExtdataAndUnsupportedSaveFiles(); NoFollowAndPinnedContainer();
    FormatCapability();
#if defined(__unix__) || defined(__APPLE__)
    HostFailureStopsWithoutReplyOrAllocation();
#endif
#if defined(__linux__)
    FirstFormatAndTitleBinding(); FormatRequestAndReplyPreflight();
    FormatContainmentAndExistingData(); PartialMetadataFailureRollsBack();
    SaveFileLifecycleAndGrowth(); SaveFilePublicationAndContainment(); SaveFilePreflightAndCapacity();
    SaveCommitAcknowledgementAndOwnership(); SaveCommitRequestAndReplyPreflight();
    SaveArchiveCloseAndLiveFiles(); SaveArchiveCloseOwnershipAndReplyPreflight();
#endif
    if (failures) return EXIT_FAILURE;
    std::cout << "PASS: per-title SaveData storage, file growth, commit acknowledgement, archive close/live-file lifetime and private reply preflight\n";
    return EXIT_SUCCESS;
}
