#include "runtime/ctr_ipc.h"
#include "runtime/ctr_svc_bridge.h"
#include "host/sha256.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
using namespace lego::ctr;
int failures=0;
#define CHECK(x) do { if (!(x)) { std::cerr<<"FAIL line "<<__LINE__<<": " #x "\n"; ++failures; } } while (0)

a32::ExecutionResult Call(SvcBridge& bridge, a32::GuestState& state,
                          std::uint32_t number, GuestMemory* memory=nullptr) {
    const auto exit=bridge.Handle({a32::ExitKind::Svc,0x100000,
                                  a32::FallbackReason::None,number},state,memory);
    return exit;
}
void HashVectors() {
    const auto hash=[](const std::string& value) {
        return lego::host::Sha256(std::span(
            reinterpret_cast<const std::uint8_t*>(value.data()),value.size()));
    };
    CHECK(hash("")=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(hash("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(hash(std::string(1000000,'a'))==
          "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    CHECK(hash("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}
void StartupSvcAbi() {
    Kernel kernel(7,1);
    SvcBridge bridge(kernel);
    GuestMemory memory;
    a32::GuestState state{};
    CHECK(Call(bridge,state,kSvcCreateAddressArbiter).kind==a32::ExitKind::Fallthrough);
    CHECK(state.r[0]==kResultSuccess);
    const auto arbiter=state.r[1];
    CHECK(kernel.handles().Get(arbiter)->type()==KernelObject::Type::AddressArbiter);
    CHECK(state.r[15]==0x100004);

    state.r[1]=kCurrentProcessPseudoHandle;
    Call(bridge,state,kSvcGetProcessId);
    CHECK(state.r[0]==kResultSuccess && state.r[1]==7);
    state.r[1]=kCurrentThreadPseudoHandle;
    Call(bridge,state,kSvcGetProcessId);
    CHECK(state.r[0]==kResultInvalidHandle);

    state.r[0]=3; state.r[1]=0x08000000; state.r[2]=0; state.r[3]=0x1000; state.r[4]=3;
    CHECK(Call(bridge,state,kSvcControlMemory,&memory).kind==a32::ExitKind::Fallthrough);
    CHECK(state.r[0]==kResultSuccess && state.r[1]==0x08000000);
    CHECK(memory.IsWritable(0x08000000,0x1000));
    state.r[0]=3; state.r[1]=0x08001001; state.r[2]=0; state.r[3]=0x1000; state.r[4]=3;
    Call(bridge,state,kSvcControlMemory,&memory);
    CHECK(state.r[0]==kResultMisalignedAddress);

    CHECK(memory.Write32(0x08000000,0));
    state.r[0]=arbiter; state.r[1]=0x08000000; state.r[2]=1; state.r[3]=0;
    Call(bridge,state,kSvcArbitrateAddress,&memory);
    CHECK(state.r[0]==kResultSuccess);
    CHECK(kernel.current_thread()->status==ThreadStatus::Running);

    state.r[1]=kCurrentProcessPseudoHandle;
    Call(bridge,state,kSvcGetResourceLimit);
    CHECK(state.r[0]==kResultSuccess);
    const auto limit=state.r[1];
    CHECK(memory.Write32(0x08000020,static_cast<std::uint32_t>(ResourceLimitType::Commit)));
    for (const auto svc : {kSvcGetResourceLimitLimitValues,kSvcGetResourceLimitCurrentValues}) {
        state.r[0]=0x08000040; state.r[1]=limit; state.r[2]=0x08000020; state.r[3]=1;
        Call(bridge,state,svc,&memory);
        CHECK(state.r[0]==kResultSuccess);
        std::uint64_t value=0; std::uint32_t fault=0;
        CHECK(memory.Read64(0x08000040,&value,&fault));
        CHECK(value>0 && value<=0x04000000);
    }
    kernel.AdvanceTime(20000000000ULL);
    Call(bridge,state,kSvcGetSystemTick);
    CHECK((std::uint64_t(state.r[1])<<32 | state.r[0])==20ULL*268111856ULL);
    const auto before=state;
    CHECK(Call(bridge,state,0x7f).kind==a32::ExitKind::Svc);
    CHECK(state.r==before.r);
}
void MissingServiceTrace() {
    Kernel kernel;
    GuestMemory memory;
    IpcRouter router;
    CHECK(memory.EnsureTlsMappings(kernel));
    CHECK(memory.Map(0x08000000,0x1000,MemoryPermission::Read|MemoryPermission::Write));
    const std::array<std::uint8_t,5> name{'s','r','v',':',0};
    CHECK(memory.LoadBytes(0x08000000,name));
    Handle session=0;
    CHECK(router.ConnectToPort(kernel,memory,0x08000000,&session)==kResultSuccess);
    const auto cb=kernel.current_thread()->tls_address+kIpcCommandBufferOffset;
    // Authored lookup of a service that is deliberately not registered.
    const std::array<std::uint32_t,5> request{0x00050100,0x656e6f6e,0x3a,5,1};
    for (std::size_t i=0;i<request.size();++i) CHECK(memory.Write32(cb+i*4,request[i]));
    CHECK(router.SendSyncRequest(kernel,memory,session)==kResultSuccess);
    CHECK(router.last_session_name()=="srv:");
    CHECK(router.last_lookup_name()=="none:");
    CHECK(router.last_request()[0]==0x00050100);
    CHECK(router.last_request()[1]==request[1]);
    std::uint32_t response=0;
    CHECK(memory.Read32(cb+4,&response));
    CHECK(response==kResultServiceNotRegistered);
}
}  // namespace
int main() {
    HashVectors(); StartupSvcAbi(); MissingServiceTrace();
    if (failures) return EXIT_FAILURE;
    std::cout<<"PASS: startup SVC ABI, SHA-256, and missing-service diagnostics\n";
    return EXIT_SUCCESS;
}
