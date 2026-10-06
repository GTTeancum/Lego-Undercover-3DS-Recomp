#include "services/gsp_gpu_service.h"
#include "runtime/ctr_svc_bridge.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace {
using namespace lego::ctr;
int failures = 0;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL " << __LINE__ << ": " #x "\n"; ++failures; } } while (0)

struct OtherEffects {
    PicaGpuRegisters registers{};
    PicaUploadState uploads{};
    std::array<std::uint8_t, 4096> page{};
    std::array<std::uint64_t, 512> epochs{};
    std::vector<std::uint8_t> vram;
    std::optional<std::uint32_t> owner, client_thread;
    std::size_t handles{}, threads{};
    std::uint64_t time{};
    bool first_registration{}, signaled{}, pending_wake{};
    ThreadStatus status{};
    bool operator==(const OtherEffects&) const = default;
};
struct Fixture {
    Kernel kernel{7, 3};
    GuestMemory memory;
    IpcRouter router;
    SvcBridge bridge{kernel, &router};
    std::shared_ptr<GspGpuService> endpoint = std::make_shared<GspGpuService>();
    std::shared_ptr<EventObject> retained_event;
    Handle handle{}, event_handle{};
    a32::GuestState cpu{};

    explicit Fixture(bool with_vram = false) {
        CHECK(memory.EnsureTlsMappings(kernel));
        if (with_vram) {
            auto bank = GpuVramBank::ReferenceZero();
            std::vector<std::uint8_t> pattern(kGpuVramBytes);
            for (std::size_t i = 0; i < pattern.size(); ++i)
                pattern[i] = static_cast<std::uint8_t>(i * 37U + (i >> 8) + 19U);
            CHECK(bank->Write(0, pattern)); // Synthetic test pixels, NEVER game input.
            CHECK(endpoint->ConfigureVram(std::move(bank)));
        }
        CHECK(router.RegisterService("gsp::Gpu", endpoint) == 0);
        Connect();
    }
    void Connect() { CHECK(router.ConnectToService(kernel, "gsp::Gpu", &handle) == 0); }
    std::shared_ptr<GspGpuService> Session() {
        auto session = std::dynamic_pointer_cast<ClientSessionObject>(kernel.handles().Get(handle));
        if (!session) throw std::runtime_error("missing client session");
        auto result = std::dynamic_pointer_cast<GspGpuService>(session->service);
        if (!result) throw std::runtime_error("wrong client service");
        return result;
    }
    std::uint32_t cb() const { return kernel.current_thread()->tls_address + kIpcCommandBufferOffset; }
    void Put(const IpcCommandBuffer& q) {
        for (unsigned i = 0; i < q.size(); ++i) CHECK(memory.Write32(cb() + 4 * i, q[i]));
        for (unsigned i = 0; i < 16; ++i) cpu.r[i] = 0x33220000U + i;
        cpu.r[0] = handle; cpu.r[15] = 0x0025947C;
        cpu.cpsr = 0xA0000010; cpu.fpscr = 0x23000010;
    }
    IpcCommandBuffer Read() {
        IpcCommandBuffer q{};
        for (unsigned i = 0; i < q.size(); ++i) CHECK(memory.Read32(cb() + 4 * i, &q[i]));
        return q;
    }
    a32::ExecutionResult Call(GuestMemory* alternate = nullptr) {
        return bridge.Handle({a32::ExitKind::Svc, cpu.r[15], a32::FallbackReason::None,
                              kSvcSendSyncRequest}, cpu, alternate ? alternate : &memory);
    }
    std::array<std::uint32_t, 2> Lcd() const {
        return {endpoint->lcd_color_fill_word(0).value(), endpoint->lcd_color_fill_word(1).value()};
    }
    OtherEffects Capture() const {
        OtherEffects state;
        for (std::size_t i = 0; i < kPicaGpuWords; ++i)
            state.registers[i] = endpoint->register_word(0x00400000U + static_cast<std::uint32_t>(4 * i)).value();
        state.uploads = endpoint->pica_uploads();
        const auto page = endpoint->shared_memory();
        std::copy(page->bytes().begin(), page->bytes().end(), state.page.begin());
        for (std::size_t i = 0; i < state.epochs.size(); ++i) state.epochs[i] = page->Epoch(8 * i);
        if (auto bank = endpoint->vram_bank()) state.vram.assign(bank->bytes().begin(), bank->bytes().end());
        state.owner = endpoint->owner_process_id(); state.client_thread = endpoint->active_client_thread_id();
        state.handles = kernel.handles().OpenHandleCount(); state.threads = kernel.threads().size();
        state.time = kernel.now_ns(); state.first_registration = endpoint->first_registration_pending();
        state.signaled = retained_event && retained_event->signaled();
        state.pending_wake = kernel.current_thread()->pending_wake;
        state.status = kernel.current_thread()->status;
        return state;
    }
    void Set(std::uint32_t value) {
        IpcCommandBuffer q{};
        q.fill(0xDEAD1234U); // Unused words must not become a reply or an implicit descriptor.
        q[0] = 0x000B0040; q[1] = value;
        Put(q);
        const auto old_cpu = cpu;
        const auto before = Capture();
        CHECK(Call().kind == a32::ExitKind::Fallthrough);
        CHECK(cpu.r[0] == 0 && cpu.r[15] == 0x00259480);
        for (unsigned i = 1; i < 15; ++i) CHECK(cpu.r[i] == old_cpu.r[i]);
        CHECK(cpu.cpsr == old_cpu.cpsr && cpu.fpscr == old_cpu.fpscr);
        CHECK(Read() == (IpcCommandBuffer{0x000B0040, 0}));
        CHECK(router.last_request() == q && router.last_session_name() == "gsp::Gpu");
        CHECK(!router.unsupported_request() && router.last_host_error().empty());
        const std::uint32_t expected = (value & 0xFFU) != 0 ? 0x01000000U : 0U;
        CHECK(Lcd() == (std::array<std::uint32_t, 2>{expected, expected}));
        CHECK(Session()->lcd_color_fill_word(0) == expected);
        CHECK(Session()->lcd_color_fill_word(1) == expected);
        CHECK(Capture() == before);
    }
    void Stop(const IpcCommandBuffer& q) {
        Put(q);
        const auto old_cpu = cpu;
        const auto old_lcd = Lcd();
        const auto before = Capture();
        CHECK(Call().kind == a32::ExitKind::Svc);
        CHECK(cpu.r == old_cpu.r && cpu.cpsr == old_cpu.cpsr && cpu.fpscr == old_cpu.fpscr);
        CHECK(Read() == q && router.unsupported_request());
        CHECK(Lcd() == old_lcd && Capture() == before);
    }
    void RegisterAndAcquire() {
        CHECK(kernel.CreateEvent(&event_handle, static_cast<std::uint32_t>(ResetType::OneShot)) == 0);
        retained_event = std::dynamic_pointer_cast<EventObject>(kernel.handles().Get(event_handle));
        CHECK(retained_event && !retained_event->signaled());
        Put({0x00130042, 1, IpcCopyHandleDesc(), event_handle});
        CHECK(Call().kind == a32::ExitKind::Fallthrough && cpu.r[0] == 0);
        CHECK(Read()[1] == kResultGspFirstInitialization && Session()->registered());
        Put({0x00160042, 0, IpcCopyHandleDesc(), kCurrentProcessPseudoHandle});
        CHECK(Call().kind == a32::ExitKind::Fallthrough && cpu.r[0] == 0);
        CHECK(Session()->owns_rights());
    }
};

void LowByteAndReply() {
    Fixture f;
    CHECK(f.Lcd() == (std::array<std::uint32_t, 2>{0, 0}));
    CHECK(!f.endpoint->lcd_color_fill_word(2) && !f.endpoint->lcd_color_fill_word(0xFFFFFFFFU));
    CHECK(!f.endpoint->CanHandle(IpcCommandBuffer{0x000B0040, 0}));
    // Exact original request, without the poisoned unused words used by Set().
    f.Put({0x000B0040, 0});
    CHECK(f.Call().kind == a32::ExitKind::Fallthrough && f.cpu.r[0] == 0);
    CHECK(f.Read() == (IpcCommandBuffer{0x000B0040, 0}));
    f.Set(1); f.Set(0); f.Set(0); f.Set(0x01000000); f.Set(0x100); f.Set(0xFFFFFFFFU);
    for (std::uint32_t low = 0; low < 256; ++low) {
        f.Set(low);
        f.Set(0xA5B6C700U | low); // Upper 24 bits do not select the boolean.
    }
    CHECK(!f.endpoint->vram_bank() && !f.endpoint->rights_held());
    CHECK(!f.Session()->registered());
}

void SharedStateAndNoPixelEffects() {
    Fixture f(true);
    f.RegisterAndAcquire();
    const auto first = f.handle;
    const auto first_session = f.Session();
    f.Set(1);
    f.Connect();
    CHECK(!f.Session()->owns_rights() && !f.Session()->registered());
    CHECK(f.Session()->lcd_color_fill_word(0) == 0x01000000U);
    f.Set(0); // A connected nonowner changes the same global LCD controls.
    CHECK(first_session->lcd_color_fill_word(1) == 0U);
    Handle duplicate = 0;
    CHECK(f.kernel.DuplicateHandle(&duplicate, f.handle) == 0);
    CHECK(f.kernel.CloseHandle(f.handle) == 0);
    f.handle = duplicate;
    f.Set(1);
    CHECK(f.kernel.CloseHandle(f.event_handle) == 0);
    f.Set(0); // Registered event object remains valid but is not signaled.
    CHECK(!f.retained_event->signaled() && first_session->registered());
    CHECK(f.kernel.CloseHandle(first) == 0);
    Fixture independent;
    independent.Set(1);
    CHECK(f.Lcd() == (std::array<std::uint32_t, 2>{0, 0}));
}

void RejectedRequestsPreserveState() {
    Fixture f;
    f.Set(1);
    for (const auto header : {0x000B0000U, 0x000B0080U, 0x000B0041U, 0x000B0042U,
                              0x000B1040U, 0x000A0040U, 0xFFFF0040U})
        f.Stop({header, 0});
    const auto original = f.handle;
    f.Put({0x000B0040, 0});
    const auto before = f.Capture();
    f.cpu.r[0] = 0;
    CHECK(f.Call().kind == a32::ExitKind::Fallthrough && f.cpu.r[0] == kResultInvalidHandle);
    CHECK(f.Lcd()[0] == 0x01000000U && f.Capture() == before);
    CHECK(f.Read() == (IpcCommandBuffer{0x000B0040, 0}));
    f.handle = original;
    for (unsigned mode = 0; mode < 3; ++mode) {
        GuestMemory protected_memory;
        const IpcCommandBuffer q{0x000B0040, 0};
        const auto bytes = mode == 1 ? 8U : static_cast<unsigned>(sizeof(q));
        const auto permission = mode == 0 ? MemoryPermission::Read :
                                mode == 1 ? MemoryPermission::Read | MemoryPermission::Write :
                                            MemoryPermission::Write;
        CHECK(protected_memory.Map(f.cb(), bytes, permission));
        CHECK(protected_memory.LoadBytes(f.cb(), {reinterpret_cast<const std::uint8_t*>(q.data()), bytes}));
        f.Put(q);
        const auto effects = f.Capture();
        CHECK(f.Call(&protected_memory).kind == a32::ExitKind::Fallthrough);
        CHECK(f.cpu.r[0] == kResultInvalidPointer);
        CHECK(f.Lcd()[0] == 0x01000000U && f.Lcd()[1] == 0x01000000U);
        CHECK(f.Capture() == effects);
        if (mode != 2) {
            std::uint32_t word = 0;
            CHECK(protected_memory.Read32(f.cb(), &word) && word == q[0]);
        }
    }
}

void FullHandleTable() {
    Fixture f;
    bool exhausted = false;
    for (unsigned i = 0; i < 65536; ++i) {
        Handle copy = 0;
        const auto result = f.kernel.DuplicateHandle(&copy, f.handle);
        if (result != 0) {
            CHECK(result == kResultOutOfHandles);
            exhausted = true;
            break;
        }
    }
    CHECK(exhausted);
    f.Set(1); f.Set(0); // No handle allocation is needed for blanking.
}
} // namespace

int main() {
    LowByteAndReply();
    SharedStateAndNoPixelEffects();
    RejectedRequestsPreserveState();
    FullHandleTable();
    if (failures) return EXIT_FAILURE;
    std::cout << "PASS: LCD low-byte boolean, shared two-screen controls, protected replies; no pixel/IRQ/time effects\n";
    return EXIT_SUCCESS;
}
