#include "dsp_live_fixture.h"
#include <iostream>
#include <new>
#include <cstdlib>

using namespace lego::ctr;
namespace { bool forbid_allocation = false; }
void* operator new(std::size_t size) {
    if (forbid_allocation) throw std::bad_alloc();
    if (void* p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }

namespace {
#define CHECK(x) do { if (!(x)) throw std::runtime_error("line " + std::to_string(__LINE__) + ": " #x); } while (0)
constexpr auto RW = MemoryPermission::Read | MemoryPermission::Write;
constexpr std::uint32_t Target = 0x14000000;
struct Fixture {
    Kernel kernel;
    GuestMemory memory;
    IpcRouter router;
    std::shared_ptr<DspDiscoveryService> service;
    Handle session{};
    explicit Fixture(bool fault = false, bool boot = true) {
        service = std::make_shared<DspDiscoveryService>(DspSpecialConfig{},
            DspProbeOptions{true, DspProbeReset::KnownOnly, 100, true});
        CHECK(memory.EnsureTlsMappings(kernel));
        CHECK(memory.Map(0x08000000, 4096, RW));
        auto bytes = dsp_live_fixture::Container(fault);
        CHECK(memory.LoadBytes(0x08000000, bytes));
        CHECK(router.RegisterService("dsp::DSP", service) == 0);
        CHECK(router.ConnectToService(kernel, "dsp::DSP", &session) == 0);
        if (boot) {
            const auto size = static_cast<std::uint32_t>(bytes.size());
            Put({0x001100C2, size, 255, 255, (size << 4) | 10, 0x08000000});
            CHECK(router.SendSyncRequest(kernel, memory, session) == 0);
            CHECK(service->live_device() && service->live_device()->attached());
        }
        CHECK(memory.Map(Target, 0x2000, RW));
        for (unsigned i = 0; i < 0x2000; ++i)
            CHECK(memory.Write8(Target + i, static_cast<std::uint8_t>(i * 73 + 19)));
    }
    std::uint32_t CB() const { return kernel.current_thread()->tls_address + 0x80; }
    void Put(const IpcCommandBuffer& q) {
        for (unsigned i = 0; i < q.size(); ++i) CHECK(memory.Write32(CB() + 4 * i, q[i]));
    }
    IpcCommandBuffer Get() {
        IpcCommandBuffer q{};
        for (unsigned i = 0; i < q.size(); ++i) CHECK(memory.Read32(CB() + 4 * i, &q[i]));
        return q;
    }
    void Success(std::uint32_t address, std::uint32_t size,
                 Handle process = kCurrentProcessPseudoHandle) {
        Put({0x00130082, address, size, 0, process});
        CHECK(router.SendSyncRequest(kernel, memory, session) == 0);
        CHECK(Get() == IpcCommandBuffer({0x00130040, 0}));
    }
    void Stop(const IpcCommandBuffer& q) {
        Put(q);
        CHECK(!router.SendSyncRequest(kernel, memory, session).has_value());
        CHECK(Get() == q);
    }
};
std::vector<std::uint8_t> Bytes(GuestMemory& memory, std::uint32_t address, unsigned size) {
    std::vector<std::uint8_t> out(size);
    for (unsigned i = 0; i < size; ++i) CHECK(memory.Read8(address + i, &out[i]));
    return out;
}
struct UntouchedDevice final : DeviceMemory {
    mutable unsigned calls{};
    std::uint32_t size() const noexcept override { return 4096; }
    bool CanRead(std::uint32_t, std::uint32_t) const noexcept override { ++calls; return true; }
    bool CanWrite(std::uint32_t, std::uint32_t) const noexcept override { ++calls; return true; }
    bool Read(std::uint32_t, std::span<std::uint8_t>) const noexcept override { ++calls; return false; }
    bool Write(std::uint32_t, std::span<const std::uint8_t>) noexcept override { ++calls; return false; }
    std::uint64_t Epoch(std::uint32_t) const noexcept override { ++calls; return 0; }
};
void CoherenceAndNoEffects() {
    Fixture f;
    auto* probe = f.service->execution_probe();
    const auto summary = probe->summary();
    const auto deadline = f.service->next_deadline_ns();
    const auto now = f.kernel.now_ns(), handles = f.kernel.handles().OpenHandleCount();
    const auto threads = f.kernel.threads().size();
    const auto bytes = Bytes(f.memory, Target, 0x2000);
    std::vector<std::uint8_t> dsp(probe->memory().begin(), probe->memory().end());
    std::vector<std::uint8_t> provenance(probe->provenance().begin(), probe->provenance().end());
    std::uint64_t value{}, epoch{}; std::uint32_t fault{};
    CHECK(f.memory.LoadExclusive(Target, 4, &value, &epoch, &fault));
    // Unaligned byte ranges and both ends of a private mapping are coherent.
    for (unsigned n : {1U, 2U, 31U, 32U, 33U, 2048U, 8192U}) f.Success(Target, n);
    f.Success(Target + 1, 2047);
    f.Success(Target + 0x1FFF, 1);
    f.Success(0, 0); f.Success(0xFFFFFFFF, 0); // Explicit zero-work requests.
    Handle process{};
    CHECK(f.kernel.handles().Create(&process, f.kernel.current_process()) == 0);
    f.Success(Target, 2048, process);
    CHECK(f.kernel.handles().Close(process) == 0);
    CHECK(Bytes(f.memory, Target, 0x2000) == bytes);
    std::uint64_t after{}, after_epoch{};
    CHECK(f.memory.LoadExclusive(Target, 4, &after, &after_epoch, &fault));
    CHECK(after == value && after_epoch == epoch);
    CHECK(probe->summary() == summary && f.service->next_deadline_ns() == deadline);
    CHECK(std::equal(dsp.begin(), dsp.end(), probe->memory().begin()));
    CHECK(std::equal(provenance.begin(), provenance.end(), probe->provenance().begin()));
    CHECK(now == f.kernel.now_ns() && handles == f.kernel.handles().OpenHandleCount());
    CHECK(threads == f.kernel.threads().size());
    // Direct success cannot allocate, read the target or reserve extra epochs.
    IpcCommandBuffer q{0x00130082, Target, 2048, 0, kCurrentProcessPseudoHandle};
    forbid_allocation = true;
    Result result = f.service->Handle(f.router, f.kernel, f.memory, *f.kernel.current_thread(), q);
    forbid_allocation = false;
    CHECK(result == 0 && q == IpcCommandBuffer({0x00130040, 0}));
    // Read-only private pages are already coherent; no write permission is needed.
    CHECK(f.memory.Map(0x00400000, 4096, MemoryPermission::Read));
    f.Success(0x00400000, 4096);
    CHECK(f.memory.Map(0xFFFFF000, 4096, RW));
    f.Success(0xFFFFFFFF, 1);
}
void AliasesAndRegions() {
    Fixture f;
    CHECK(f.memory.Map(0x08004000, 8192, RW));
    CHECK(f.memory.MapUserAlias(0x0E000000, 0x08004000, 8192, 3) == 0);
    CHECK(f.memory.Write32(0x08004000, 0xF0E1D2C3));
    CHECK(f.memory.SpansAlias(0x08004000, 4, 0x0E000000, 4));
    f.Success(0x0E000000, 8192);
    std::uint32_t value{}; CHECK(f.memory.Read32(0x0E000000, &value) && value == 0xF0E1D2C3);
    CHECK(f.memory.ProtectUserAlias(0x08004000, 8192, 0) == 0);
    f.Stop({0x00130082, 0x08004000, 1, 0, kCurrentProcessPseudoHandle});
    f.Success(0x0E000000, 8192); // Independent view permissions, same backing.
    for (auto [address, size] : {std::pair{0U, 1U}, {Target - 1, 2U},
           {Target + 0x1FFF, 2U}, {0xFFFFFFFFU, 2U}, {Target, 0xFFFFFFFFU}})
        f.Stop({0x00130082, address, size, 0, kCurrentProcessPseudoHandle});
    CHECK(f.memory.Map(Target + 0x2000, 4096, RW));
    f.Stop({0x00130082, Target + 0x1FFF, 2, 0, kCurrentProcessPseudoHandle}); // Two backing regions.
    CHECK(f.memory.Map(0x00600000, 4096, MemoryPermission::Write));
    f.Stop({0x00130082, 0x00600000, 1, 0, kCurrentProcessPseudoHandle});
    CHECK(f.memory.Map(0x00700000, 4096, MemoryPermission::None));
    f.Stop({0x00130082, 0x00700000, 1, 0, kCurrentProcessPseudoHandle});
    auto shared = std::make_shared<ServiceSharedMemoryObject>();
    CHECK(f.memory.MapSharedServicePage(0x10000000, shared, RW));
    f.Stop({0x00130082, 0x10000000, 1, 0, kCurrentProcessPseudoHandle});
    auto device = std::make_shared<UntouchedDevice>();
    CHECK(f.memory.MapDeviceMemory(0x18000000, device, RW));
    f.Stop({0x00130082, 0x18000000, 1, 0, kCurrentProcessPseudoHandle});
    CHECK(device->calls == 0); // Not even a provenance/peripheral read occurred.
    f.Stop({0x00130082, DspLiveDevice::DataAddress + 0x84, 160, 0, kCurrentProcessPseudoHandle});
    CHECK(!f.memory.IsCachelessPrivateRange(Target, 0));
}
void HandlesAndReplies() {
    Fixture f;
    for (Handle h : {Handle(0), Handle(0x12345678), kCurrentThreadPseudoHandle}) {
        IpcCommandBuffer q{0x00130082, Target, 2048, 0, h}; f.Put(q);
        CHECK(f.router.SendSyncRequest(f.kernel, f.memory, f.session) == kResultInvalidHandle);
        CHECK(f.Get() == q);
    }
    Handle foreign{};
    CHECK(f.kernel.handles().Create(&foreign,
        std::make_shared<ProcessObject>(f.kernel.current_process()->process_id)) == 0);
    f.Stop({0x00130082, Target, 2048, 0, foreign}); // Same PID is not identity.
    CHECK(f.kernel.handles().Close(foreign) == 0);
    IpcCommandBuffer closed{0x00130082, Target, 2048, 0, foreign}; f.Put(closed);
    CHECK(f.router.SendSyncRequest(f.kernel, f.memory, f.session) == kResultInvalidHandle && f.Get() == closed);
    for (unsigned header : {0x00130000U, 0x00130080U, 0x001300C2U, 0x00130083U, 0x00140082U})
        f.Stop({header, Target, 2048, 0, kCurrentProcessPseudoHandle});
    for (unsigned descriptor : {1U, 16U, 0x04000000U})
        f.Stop({0x00130082, Target, 2048, descriptor, kCurrentProcessPseudoHandle});
    IpcCommandBuffer q{0x00130082, Target, 2048, 0, kCurrentProcessPseudoHandle};
    GuestMemory protected_reply;
    CHECK(protected_reply.Map(f.CB(), sizeof(q), MemoryPermission::Read));
    CHECK(protected_reply.LoadBytes(f.CB(), {reinterpret_cast<const std::uint8_t*>(q.data()), sizeof(q)}));
    const auto before = f.service->execution_probe()->summary();
    CHECK(f.router.SendSyncRequest(f.kernel, protected_reply, f.session) == kResultInvalidPointer);
    CHECK(f.service->execution_probe()->summary() == before);
    Fixture unbooted(false, false); unbooted.Stop(q);
    DspDiscoveryService unconfigured; CHECK(!unconfigured.CanHandle(q));
}
void FaultedDevice() {
    Fixture f(true);
    CHECK(!f.service->RunScheduled(f.kernel, *f.service->next_deadline_ns()));
    const auto before = f.service->execution_probe()->summary();
    CHECK(before.state == DspProbeState::Fault);
    f.Stop({0x00130082, Target, 2048, 0, kCurrentProcessPseudoHandle});
    f.Stop({0x00130082, 0, 0, 0, kCurrentProcessPseudoHandle});
    CHECK(f.service->execution_probe()->summary() == before);
}
}
int main() {
    try { CoherenceAndNoEffects(); AliasesAndRegions(); HandlesAndReplies(); FaultedDevice(); }
    catch (const std::exception& e) { forbid_allocation = false; std::cerr << e.what() << '\n'; return 1; }
    std::cout << "PASS: bounded cacheless DSP flush; process identity, coherent private aliases, no target/device/time effects and retained faults\n";
}
