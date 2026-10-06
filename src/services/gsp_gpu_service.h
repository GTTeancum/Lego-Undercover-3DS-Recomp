#pragma once
#include <array>
#include "runtime/ctr_ipc.h"
#include "runtime/ctr_shared_memory.h"
#include "services/pica_startup.h"
#include "services/gsp_display_transfer.h"
#include "services/gsp_memory_fill.h"
#include "services/gsp_display_events.h"

namespace lego::ctr {
// ErrCodes::FirstInitialization(519), GX(10), Success summary/level.
inline constexpr Result kResultGspFirstInitialization = 0x00002A07U;
inline constexpr std::uint32_t kGspRelaySlots = 4;

// Bounded startup GSP state. Registration and passive MMIO writes do not
// generate GPU work by themselves. The queue supports CacheFlush and staged
// non-drawing PICA startup lists, including their genuine P3D IRQ requests.
// A bounded RGBA4 DisplayTransfer uses explicitly configured device VRAM.
// Bounded VRAM MemoryFill updates actual bytes before its PSC notification.
// Drawing, other transfers and active MMIO triggers remain unsupported.
// Display-period notification is separate from rendering and explicitly scheduled.
class GspGpuService final : public IpcService {
public:
    GspGpuService() : shared_(std::make_shared<SharedState>()) {}
    // Configure only before clients connect; never reset/replace VRAM in flight.
    bool ConfigureVram(std::shared_ptr<GpuVramBank> bank) {
        if (!bank || shared_->vram) return false;
        for (const auto& slot:shared_->slots) if (!slot.expired()) return false;
        shared_->vram=std::move(bank); return true;
    }
    [[nodiscard]] std::shared_ptr<GpuVramBank> vram_bank() const noexcept { return shared_->vram; }
    Result CreateSessionHandler(std::shared_ptr<IpcService>* out) override {
        if (!out) return kResultInvalidPointer;
        for (std::uint32_t slot=0; slot<kGspRelaySlots; ++slot) {
            if (!shared_->slots[slot].expired()) continue;
            auto handler=std::shared_ptr<GspGpuService>(new GspGpuService(shared_,slot));
            shared_->slots[slot]=handler->identity_;
            *out=std::move(handler);
            return kResultSuccess;
        }
        return kResultMaxConnectionsReached;
    }
    bool CanHandle(const IpcCommandBuffer& command) const noexcept override {
        if (!identity_) return false; // Registration endpoint is never a client.
        if (command[0]==IpcMakeHeader(0x000B,1,0)) return true;
        if (command[0]==IpcMakeHeader(0x000C,0,0)) return true;
        if (command[0]==IpcMakeHeader(0x0001,2,2))
            return command[2]<=0x3FFFF && command[3]==((command[2]<<14)|2U);
        if (command[0]==IpcMakeHeader(0x0002,2,4))
            return command[2]<=0x3FFFF && command[3]==((command[2]<<14)|2U) &&
                   command[5]==((command[2]<<14)|0x402U);
        if (command[0]==IpcMakeHeader(0x0013,1,2))
            return (command[1]&~1U)==0 && command[2]==IpcCopyHandleDesc();
        return shared_->owner.expired() &&
               command[0]==IpcMakeHeader(0x0016,1,2) && command[1]==0 &&
               command[2]==IpcCopyHandleDesc();
    }
    Result Handle(IpcRouter& router,Kernel& kernel,GuestMemory& memory,ThreadObject& thread,
                  IpcCommandBuffer& command) override {
        if(!CanHandle(command))return kResultNotFound;
        if (IpcCommandId(command[0])==0x000B) {
            // Pinned IPC Pop<bool> consumes the low byte, not the full word.
            // SetLcdForceBlack builds a zero RGB ColorFill and changes bit 24.
            // Both LCD controls share it; framebuffer pixels and IRQs do not.
            // Router preflights the complete response before this state change.
            const bool enabled=(command[1]&0xFFU)!=0;
            shared_->lcd_color_fill_words.fill(enabled ? 0x01000000U : 0U);
            command.fill(0);
            command[0]=IpcMakeHeader(0x000B,1,0);
            command[1]=kResultSuccess;
            return kResultSuccess;
        }
        if (IpcCommandId(command[0])==0x000C)
            return TriggerCommandQueue(router,kernel,memory,thread,command);
        if (IpcCommandId(command[0])==0x0001 || IpcCommandId(command[0])==0x0002)
            return WriteHwRegisters(router,memory,thread,command);
        if (IpcCommandId(command[0])==0x0013) {
            const auto event=std::dynamic_pointer_cast<EventObject>(kernel.handles().Get(command[3]));
            if (!event) return kResultInvalidHandle;
            ::lego::ctr::Handle handle=0;
            const Result result=kernel.handles().Create(&handle,shared_->memory);
            if (result!=kResultSuccess) return result;
            // All fallible validation/allocation precedes registration. This is
            // a copy: retaining the event does not close or signal the caller's handle.
            const Result reply=shared_->first_registration ? kResultGspFirstInitialization : kResultSuccess;
            identity_->event=event;
            identity_->flags=command[1];
            identity_->registered=true;
            shared_->first_registration=false;
            command.fill(0); command[0]=IpcMakeHeader(0x0013,2,2);
            command[1]=reply; command[2]=identity_->slot;
            command[3]=IpcCopyHandleDesc(); command[4]=handle;
            return kResultSuccess;
        }
        const auto process=std::dynamic_pointer_cast<ProcessObject>(kernel.handles().Get(command[3]));
        if(!process)return kResultInvalidHandle;
        if(process!=kernel.current_process()) {
            router.RequestHostStop("GSP AcquireRight for another process is unsupported");
            return kResultSuccess;
        }
        identity_->process_id=process->process_id;
        identity_->client_thread_id=thread.thread_id;
        shared_->owner=identity_;
        command.fill(0);command[0]=IpcMakeHeader(0x0016,1,0);command[1]=kResultSuccess;
        return kResultSuccess;
    }
    [[nodiscard]] bool rights_held() const noexcept { return !shared_->owner.expired(); }
    [[nodiscard]] bool owns_rights() const noexcept { return identity_ && shared_->owner.lock()==identity_; }
    [[nodiscard]] std::optional<std::uint32_t> owner_process_id() const noexcept {
        if(const auto owner=shared_->owner.lock())return owner->process_id;
        return std::nullopt;
    }
    [[nodiscard]] std::optional<std::uint32_t> active_client_thread_id() const noexcept {
        if(const auto owner=shared_->owner.lock())return owner->client_thread_id;
        return std::nullopt;
    }
    [[nodiscard]] std::optional<std::uint32_t> relay_slot() const noexcept {
        return identity_ ? std::optional(identity_->slot) : std::nullopt;
    }
    [[nodiscard]] bool registered() const noexcept { return identity_ && identity_->registered; }
    [[nodiscard]] std::uint32_t relay_flags() const noexcept { return identity_ ? identity_->flags : 0; }
    [[nodiscard]] std::shared_ptr<EventObject> relay_event() const noexcept { return identity_ ? identity_->event : nullptr; }
    [[nodiscard]] std::shared_ptr<ServiceSharedMemoryObject> shared_memory() const noexcept { return shared_->memory; }
    [[nodiscard]] bool first_registration_pending() const noexcept { return shared_->first_registration; }
    [[nodiscard]] std::optional<std::uint32_t> register_word(std::uint32_t relative) const noexcept {
        const auto slot=RegisterSlot(relative);
        return slot ? std::optional<std::uint32_t>(shared_->register_words[*slot]) : std::nullopt;
    }
    // Separate LCD control state, NOT PICA registers, pixels, or scanout.
    // Screen 0 is top; screen 1 is bottom. Invalid screen indexes are not aliased.
    [[nodiscard]] std::optional<std::uint32_t> lcd_color_fill_word(std::uint32_t screen) const noexcept {
        if (screen>=shared_->lcd_color_fill_words.size()) return std::nullopt;
        return shared_->lcd_color_fill_words[screen];
    }
    [[nodiscard]] const PicaUploadState& pica_uploads() const noexcept { return shared_->pica_uploads; }
    [[nodiscard]] const PicaListResult& last_pica_result() const noexcept { return shared_->last_pica_result; }
    bool PrepareDisplayPeriod(DisplayPeriodPlan& out, const char*& error) const noexcept;
    bool CommitDisplayPeriod(Kernel& kernel, const DisplayPeriodPlan& plan) noexcept;
private:
    // The pinned GPU::WriteReg routes two 4 KiB pages at 0x1EF00000 and
    // bounds word indexes by PicaCore::Regs::NUM_REGS (0x732). GSP adds 0x1EB00000.
    static constexpr std::size_t kGpuWords=kPicaGpuWords;
    static constexpr std::optional<std::size_t> RegisterSlot(std::uint32_t relative) noexcept {
        if (relative<0x00400000U || (relative&3U)!=0) return std::nullopt;
        const auto index=(relative-0x00400000U)/4;
        return index<kGpuWords ? std::optional<std::size_t>(index) : std::nullopt;
    }
    static constexpr bool TriggersAction(std::uint32_t relative,std::uint32_t value) noexcept {
        switch(relative) {
        case 0x0040001CU: // memory_fill_config[0].control.trigger
        case 0x0040002CU: // memory_fill_config[1].control.trigger
        case 0x00400C18U: // display_transfer_config.trigger
            return (value&1U)!=0;
        case 0x004018F0U: // internal.pipeline.command_buffer.trigger[0]
        case 0x004018F4U: // internal.pipeline.command_buffer.trigger[1]
            return value!=0;
        default: return false;
        }
    }
    Result WriteHwRegisters(IpcRouter&,GuestMemory&,ThreadObject&,IpcCommandBuffer&);
    Result TriggerCommandQueue(IpcRouter&,Kernel&,GuestMemory&,ThreadObject&,IpcCommandBuffer&);
    struct SessionIdentity {
        std::uint32_t slot{},process_id{},client_thread_id{},flags{};
        bool registered{};
        std::shared_ptr<EventObject> event;
    };
    struct SharedState {
        SharedState() {
            // Explicit REFERENCE-HLE cold-start image: PicaCore::Regs regs{} plus
            // all assignments in pinned PicaCore::InitializeRegs. These include
            // compatibility defaults and are NOT a measured hardware reset dump.
            register_words[0x434]=1;          // internal.irq_autostop
            register_words[0x430]=0xFFFFFFF0; // internal.irq_mask
            register_words[0x420]=0x12345678; // internal.irq_compare (compatibility)
            register_words[0x11A]=0x181E6000; // top address_left1
            register_words[0x11B]=0x1822C800; // top address_left2
            register_words[0x125]=0x18273000; // top address_right1
            register_words[0x126]=0x182B9800; // top address_right2
            register_words[0x15A]=0x1848F000; // bottom address_left1
            register_words[0x15B]=0x184C7800; // bottom address_left2
            register_words[0x117]=(400U<<16)|240U; // top size
            register_words[0x124]=720;       // top stride
            register_words[0x11C]=1;         // top RGB8
            register_words[0x157]=(320U<<16)|240U; // bottom size
            register_words[0x164]=720;       // bottom stride
            register_words[0x15C]=1;         // bottom RGB8
            register_words[0x689]=0xA0000001; // GS input count + VS shader mode
        }
        std::weak_ptr<SessionIdentity> owner;
        std::array<std::weak_ptr<SessionIdentity>,kGspRelaySlots> slots;
        std::shared_ptr<ServiceSharedMemoryObject> memory=std::make_shared<ServiceSharedMemoryObject>();
        bool first_registration{true};
        // REGS_BEGIN 0x1EB00000 + relative -> GPU base 0x1EF00000.
        // internal begins at GPU byte 0x1000. Only passive stores / disabled
        // action triggers are modeled. Command-list register execution is distinct.
        // Reset policy is the pinned HLE constructor above, not physical hardware.
        PicaGpuRegisters register_words{};
        // Matches the two color-fill words in pinned PicaCore::RegsLcd regs_lcd{}.
        // Other LCD registers and presentation remain unmodeled; no CPU MMIO map.
        std::array<std::uint32_t,2> lcd_color_fill_words{};
        PicaUploadState pica_uploads{};
        PicaListResult last_pica_result{};
        std::shared_ptr<GpuVramBank> vram;
        std::array<std::array<std::uint32_t,7>,2> display_cached{};
        std::uint64_t display_generation{};

    };
    GspGpuService(std::shared_ptr<SharedState> shared,std::uint32_t slot)
        : shared_(std::move(shared)),identity_(std::make_shared<SessionIdentity>()) {identity_->slot=slot;}
    std::shared_ptr<SharedState> shared_;
    // Final client-session reference retires its event, slot and ownership;
    // duplicated handles retain the SAME identity, not another numeric slot.
    std::shared_ptr<SessionIdentity> identity_;
};
} // namespace lego::ctr
