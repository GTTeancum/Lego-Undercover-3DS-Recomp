#include "services/fs_user_service.h"
#include "runtime/ctr_svc_bridge.h"
#include "host/sha256.h"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace {
using namespace lego::ctr;
namespace fs=std::filesystem;
int failures=0;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}}while(0)
struct Temp {
    fs::path root;
    Temp() {
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0;i<64;++i) {
            auto p=fs::temp_directory_path()/("lego-romfs-"+std::to_string(stamp)+"-"+std::to_string(i));
            if(fs::create_directory(p)){root=p;return;}
        }
        throw std::runtime_error("cannot allocate owned test root");
    }
    ~Temp(){std::error_code e;fs::remove_all(root,e);}
};
void Save(const fs::path& p,const std::vector<std::uint8_t>& b) {
    std::ofstream f(p,std::ios::binary);if(!f.write(reinterpret_cast<const char*>(b.data()),b.size()))
        throw std::runtime_error("fixture write failed");
}
template<class F> void Throws(F action){bool caught=false;try{action();}catch(const std::exception&){caught=true;}CHECK(caught);}
struct Fixture {
    Temp temp;Kernel kernel;GuestMemory memory;IpcRouter router;SvcBridge bridge{kernel,&router};
    std::shared_ptr<FsUserService> endpoint=std::make_shared<FsUserService>(0x00040000000AD500ULL);
    std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x3000);
    std::shared_ptr<const RomfsImage> image;
    Handle user{};a32::GuestState cpu{};
    static constexpr std::uint32_t input=0x08000000,output=0x09000000;
    fs::path Path()const{return temp.root/"romfs.bin";}
    std::uint32_t cb()const{return kernel.current_thread()->tls_address+kIpcCommandBufferOffset;}
    Fixture(bool configured=true) {
        for(unsigned i=0;i<bytes.size();++i)bytes[i]=static_cast<std::uint8_t>(i*13U+(i>>8));
        Save(Path(),bytes);
        if(configured) {image=RomfsImage::OpenVerified(Path(),bytes.size(),0x1000,lego::host::Sha256(bytes));endpoint->ConfigureRomfs(image);}
        CHECK(memory.EnsureTlsMappings(kernel));CHECK(memory.Map(input,0x1000,MemoryPermission::Read));
        CHECK(memory.Map(output,0x3000,MemoryPermission::Read|MemoryPermission::Write));
        const std::array<std::uint8_t,16> path{0xE8};CHECK(memory.LoadBytes(input,path));
        CHECK(router.RegisterService("fs:USER",endpoint)==0);NewUser();
    }
    void NewUser(bool initialize=true){CHECK(router.ConnectToService(kernel,"fs:USER",&user)==0);if(initialize){Put({0x08610042,0x040203C8,0x20,0});CHECK(Call().kind==a32::ExitKind::Fallthrough);}}
    void Put(const IpcCommandBuffer& q,Handle target=0){for(unsigned i=0;i<q.size();++i)CHECK(memory.Write32(cb()+4*i,q[i]));for(unsigned i=0;i<16;++i)cpu.r[i]=0xAA000000+i;cpu.r[0]=target?target:user;cpu.r[15]=0x0025947C;cpu.cpsr=0x20000010;}
    IpcCommandBuffer Read(){IpcCommandBuffer q{};for(unsigned i=0;i<q.size();++i)CHECK(memory.Read32(cb()+4*i,&q[i]));return q;}
    a32::ExecutionResult Call(GuestMemory* other=nullptr){return bridge.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcSendSyncRequest},cpu,other?other:&memory);}
    IpcCommandBuffer OpenRequest(){return {0x08030204,0,3,1,1,2,12,1,0,0x4802,input,0x30002,input+4};}
    Handle Open(){const auto mounts=endpoint->archives().size(),handles=kernel.handles().OpenHandleCount();Put(OpenRequest());CHECK(Call().kind==a32::ExitKind::Fallthrough&&cpu.r[0]==0);const auto q=Read();CHECK(q==IpcCommandBuffer({0x08030042,0,IpcMoveHandleDesc(),q[3]}));CHECK(q[3]!=0&&kernel.handles().OpenHandleCount()==handles+1);CHECK(endpoint->archives().size()==mounts);return q[3];}
    IpcCommandBuffer ReadRequest(std::uint64_t off=0,std::uint32_t n=40,std::uint32_t addr=output){return {0x080200C2,static_cast<std::uint32_t>(off),static_cast<std::uint32_t>(off>>32),n,(n<<4)|0xC,addr};}
    void Unsupported(const IpcCommandBuffer& q,Handle target=0){Put(q,target);const auto before=cpu;const auto handles=kernel.handles().OpenHandleCount();CHECK(Call().kind==a32::ExitKind::Svc&&router.unsupported_request());CHECK(cpu.r==before.r&&cpu.cpsr==before.cpsr&&Read()==q);CHECK(kernel.handles().OpenHandleCount()==handles&&kernel.now_ns()==0);}
    void Transfer(Handle file,std::uint64_t offset,std::uint32_t length,std::uint32_t expected){
        const std::vector<std::uint8_t> sentinel(0x3000,0xA5);CHECK(memory.LoadBytes(output,sentinel));
        const auto q=ReadRequest(offset,length);Put(q,file);const auto before=cpu;const auto handles=kernel.handles().OpenHandleCount();
        CHECK(Call().kind==a32::ExitKind::Fallthrough&&cpu.r[0]==0);CHECK(Read()==IpcCommandBuffer({0x08020082,0,expected,q[4],output}));
        CHECK(cpu.r[15]==before.r[15]+4);for(unsigned i=1;i<15;++i)CHECK(cpu.r[i]==before.r[i]);
        for(unsigned i=0;i<sentinel.size();++i){std::uint8_t v=0;CHECK(memory.Read8(output+i,&v));CHECK(v==(i<expected?bytes[0x1000+offset+i]:0xA5));}
        CHECK(kernel.now_ns()==0&&kernel.handles().OpenHandleCount()==handles);
    }
};
void ReadsAndLifetime(){
    Fixture f;auto first=f.Open();auto second=f.Open();CHECK(first!=second);
    CHECK(!f.router.HasService("fs:RomFS"));
    const auto object=std::dynamic_pointer_cast<ClientSessionObject>(f.kernel.handles().Get(first));
    CHECK(object&&object->name=="fs:RomFS"&&std::dynamic_pointer_cast<RomfsFileService>(object->service));
    f.Transfer(first,0,40,40);f.Transfer(first,0x123,0x100,0x100);f.Transfer(first,0x1FF8,24,8);
    f.Transfer(first,0x2000,24,0);f.Transfer(first,UINT64_MAX,24,0);f.Transfer(first,0,0,0);
    f.Put({0x08040000},first);CHECK(f.Call().kind==a32::ExitKind::Fallthrough);CHECK(f.Read()==IpcCommandBuffer({0x080400C0,0,0x2000,0}));
    Handle duplicate{};CHECK(f.kernel.handles().Duplicate(&duplicate,first)==0);
    for(unsigned i=0;i<2;++i){f.Put({0x08080000},first);CHECK(f.Call().kind==a32::ExitKind::Fallthrough);CHECK(f.Read()==IpcCommandBuffer({0x08080040,0}));}
    CHECK(f.kernel.CloseHandle(first)==0);f.Transfer(duplicate,0,40,40);f.Transfer(second,0,40,40);
    CHECK(f.kernel.CloseHandle(f.user)==0);f.Transfer(second,0,40,40);
    // Unsupported modifications cannot change the input image.
    for(auto q:{IpcCommandBuffer{0x08030102,0,0,4,1,0x4A,Fixture::input},IpcCommandBuffer{0x08050080,2,0},IpcCommandBuffer{0x08090000},IpcCommandBuffer{0x08010080}})f.Unsupported(q,second);
    std::vector<std::uint8_t> actual(f.bytes.size());std::ifstream in(f.Path(),std::ios::binary);in.read(reinterpret_cast<char*>(actual.data()),actual.size());CHECK(actual==f.bytes);
    f.NewUser();f.Transfer(f.Open(),0,40,40);
    auto session=std::make_shared<RomfsFileService>(f.image);std::weak_ptr<const RomfsImage> weak=f.image;
    // Each session pins the shared input, not a raw borrowed pointer.
    CHECK(!weak.expired()&&session->image().size()==0x2000);
}
void Guards(){
    Fixture f;auto file=f.Open();auto base=f.OpenRequest();
    for(auto i:{0U,2U,3U,4U,5U,6U,7U,8U,9U,11U}){auto q=base;q[i]^=1;f.Unsupported(q);}
    f.NewUser(false);f.Unsupported(base);f.NewUser();
    Fixture none(false);none.Unsupported(none.OpenRequest());
    for(auto addr:{0xDEAD0000U,0xFFFFFFFCU}){auto q=base;q[12]=addr;f.Put(q);CHECK(f.Call().kind==a32::ExitKind::Fallthrough&&f.cpu.r[0]==kResultInvalidPointer);CHECK(f.Read()==q);}
    auto q=base;q[10]=0xDEAD0000;f.Put(q);CHECK(f.Call().kind==a32::ExitKind::Fallthrough&&f.cpu.r[0]==kResultInvalidPointer);
    const std::array<std::uint8_t,4> badpath{5,0,0,0};CHECK(f.memory.LoadBytes(Fixture::input+4,badpath));f.Unsupported(base);
    for(auto p:{Fixture::output+0x3000-2,0xDEAD0000U,0xFFFFFFFCU}){q=f.ReadRequest(0,40,p);f.Put(q,file);CHECK(f.Call().kind==a32::ExitKind::Fallthrough&&f.cpu.r[0]==kResultInvalidPointer);CHECK(f.Read()==q);}
    q=f.ReadRequest(0,0,0xFFFFFFFF);f.Put(q,file);CHECK(f.Call().kind==a32::ExitKind::Fallthrough&&f.Read()[2]==0);
    for(auto descriptor:{0x28AU,0x28EU,0x280U,0x27CU}){q=f.ReadRequest();q[4]=descriptor;f.Unsupported(q,file);}
    q=f.ReadRequest(0,kMaxRomfsRead+1);f.Unsupported(q,file);
    q=f.ReadRequest(0,40,f.cb());f.Unsupported(q,file);CHECK(!f.router.last_host_error().empty());
    for(bool partial:{false,true}){
        q=f.ReadRequest();f.Put(q,file);GuestMemory protected_memory;
        const unsigned n=partial?16:sizeof(IpcCommandBuffer);
        CHECK(protected_memory.Map(f.cb(),n,partial?MemoryPermission::Read|MemoryPermission::Write:MemoryPermission::Read));
        CHECK(protected_memory.LoadBytes(f.cb(),{reinterpret_cast<const std::uint8_t*>(q.data()),n}));
        CHECK(f.Call(&protected_memory).kind==a32::ExitKind::Fallthrough&&f.cpu.r[0]==kResultInvalidPointer);
    }
    // Read-only output cannot be silently promoted to writable.
    CHECK(f.memory.Map(0x0A000000,0x1000,MemoryPermission::Read));q=f.ReadRequest(0,40,0x0A000000);f.Put(q,file);
    CHECK(f.Call().kind==a32::ExitKind::Fallthrough&&f.cpu.r[0]==kResultInvalidPointer);
}
void IntegrityAndFailures(){
    Fixture f;auto file=f.Open();const auto hash=lego::host::Sha256(f.bytes);
    Throws([&]{RomfsImage::OpenVerified(f.Path(),f.bytes.size()+1,0x1000,hash);});
    Throws([&]{RomfsImage::OpenVerified(f.Path(),f.bytes.size(),0x1000,std::string(64,'0'));});
    Throws([&]{RomfsImage::OpenVerified(f.Path(),f.bytes.size(),f.bytes.size(),hash);});
    Throws([&]{RomfsImage::OpenVerified(f.temp.root/"missing",f.bytes.size(),0x1000,hash);});
    fs::create_symlink(f.Path(),f.temp.root/"symlink");Throws([&]{RomfsImage::OpenVerified(f.temp.root/"symlink",f.bytes.size(),0x1000,hash);});
    auto another=RomfsImage::OpenVerified(f.Path(),f.bytes.size(),0x1000,hash);Throws([&]{f.endpoint->ConfigureRomfs(another);});
    f.endpoint->ConfigureRomfs(f.image);Throws([&]{f.endpoint->ConfigureRomfs({});});
    // Replacement by name cannot redirect a session to different data.
    fs::rename(f.Path(),f.temp.root/"original");Save(f.Path(),std::vector<std::uint8_t>(f.bytes.size(),0));f.Transfer(file,0,40,40);
    fs::resize_file(f.temp.root/"original",0x1800);
    const std::vector<std::uint8_t> unchanged(40,0xA5);CHECK(f.memory.LoadBytes(Fixture::output,unchanged));
    const auto q=f.ReadRequest();f.Unsupported(q,file);CHECK(!f.router.last_host_error().empty());
    for(unsigned i=0;i<40;++i){std::uint8_t v{};CHECK(f.memory.Read8(Fixture::output+i,&v)&&v==0xA5);}
    // Missing/corrupt input errors never create an empty replacement.
    CHECK(!fs::exists(f.temp.root/"missing"));
}
void ReservationsAndExhaustion(){
    Fixture f;auto file=f.Open();std::uint64_t value{},token{};std::uint32_t fault{};
    CHECK(f.memory.LoadExclusive(Fixture::output,4,&value,&token,&fault));
    f.Put(f.ReadRequest(),file);CHECK(f.Call().kind==a32::ExitKind::Fallthrough);
    CHECK(f.memory.StoreExclusive(Fixture::output,4,value,token,&fault)==a32::ExclusiveStoreResult::ReservationLost);
    std::vector<Handle> handles;
    while(true){Handle h{};auto r=f.kernel.handles().Duplicate(&h,file);if(r!=0)break;handles.push_back(h);}
    const auto count=f.kernel.handles().OpenHandleCount();f.Put(f.OpenRequest());
    CHECK(f.Call().kind==a32::ExitKind::Fallthrough&&f.cpu.r[0]==0);
    CHECK(f.Read()==IpcCommandBuffer({0x08030042,kResultOutOfHandles,IpcMoveHandleDesc(),0}));CHECK(f.kernel.handles().OpenHandleCount()==count);
    for(auto h:handles)CHECK(f.kernel.CloseHandle(h)==0);
}
}
int main(){
#if defined(__unix__) || defined(__APPLE__)
    ReadsAndLifetime();Guards();IntegrityAndFailures();ReservationsAndExhaustion();
#endif
    if(failures)return EXIT_FAILURE;
    std::cout<<"PASS: verified read-only RomFS sessions, mappings, ownership, integrity and guards\n";
}
