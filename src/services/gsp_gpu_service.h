#pragma once
#include "runtime/ctr_ipc.h"

namespace lego::ctr {
// First uncontended AcquireRight only. Session identity is real ownership, not
// a guest/GSP thread slot. Numeric relay slots, shared memory, GPU execution,
// repeat/contended acquisition and release commands remain unreconstructed.
class GspGpuService final : public IpcService {
public:
    GspGpuService() : shared_(std::make_shared<SharedState>()) {}
    std::shared_ptr<IpcService> CreateSessionHandler() override {
        return std::shared_ptr<IpcService>(new GspGpuService(shared_));
    }
    bool CanHandle(const IpcCommandBuffer& command) const noexcept override {
        return identity_ && shared_->owner.expired() &&
               command[0]==IpcMakeHeader(0x0016,1,2) && command[1]==0 &&
               command[2]==IpcCopyHandleDesc();
    }
    Result Handle(IpcRouter& router,Kernel& kernel,GuestMemory&,ThreadObject& thread,
                  IpcCommandBuffer& command) override {
        if(!CanHandle(command))return kResultNotFound;
        const auto process=std::dynamic_pointer_cast<ProcessObject>(kernel.handles().Get(command[3]));
        if(!process)return kResultInvalidHandle;
        if(process!=kernel.current_process()) {
            router.RequestHostStop("GSP AcquireRight for another process is unsupported");
            return kResultSuccess; // No IPC reply is committed.
        }
        // Resolve the copied process object, including the current-process
        // pseudo handle. Never interpret the guest handle word as a process ID.
        identity_->process_id=process->process_id;
        identity_->client_thread_id=thread.thread_id;
        shared_->owner=identity_;
        command.fill(0);command[0]=IpcMakeHeader(0x0016,1,0);command[1]=kResultSuccess;
        return kResultSuccess;
    }
    [[nodiscard]] bool rights_held() const noexcept { return !shared_->owner.expired(); }
    [[nodiscard]] bool owns_rights() const noexcept {
        return identity_ && shared_->owner.lock()==identity_;
    }
    [[nodiscard]] std::optional<std::uint32_t> owner_process_id() const noexcept {
        if(const auto owner=shared_->owner.lock())return owner->process_id;
        return std::nullopt;
    }
    [[nodiscard]] std::optional<std::uint32_t> active_client_thread_id() const noexcept {
        if(const auto owner=shared_->owner.lock())return owner->client_thread_id;
        return std::nullopt;
    }
private:
    struct SessionIdentity {
        std::uint32_t process_id{},client_thread_id{};
    };
    struct SharedState { std::weak_ptr<SessionIdentity> owner; };
    explicit GspGpuService(std::shared_ptr<SharedState> shared)
        : shared_(std::move(shared)),identity_(std::make_shared<SessionIdentity>()) {}
    std::shared_ptr<SharedState> shared_;
    // The registration endpoint has no identity. Kernel handle duplicates share
    // a session; final session destruction releases ownership via the weak ref.
    std::shared_ptr<SessionIdentity> identity_;
};
} // namespace lego::ctr
