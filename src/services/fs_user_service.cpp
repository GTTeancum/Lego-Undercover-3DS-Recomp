#include "services/fs_user_service.h"
#include <utility>

namespace lego::ctr {
FsUserService::FsUserService(std::uint64_t program_id)
    : shared_(std::make_shared<SharedState>(SharedState{program_id})) {}

FsUserService::FsUserService(std::shared_ptr<SharedState> shared)
    : shared_(std::move(shared)) {}

std::shared_ptr<IpcService> FsUserService::CreateSessionHandler() {
    // A new connection gets its own initialization/program binding. Ordinary
    // DuplicateHandle keeps the same ClientSessionObject and handler instead.
    return std::shared_ptr<IpcService>(new FsUserService(shared_));
}

bool FsUserService::CanHandle(const IpcCommandBuffer& command) const noexcept {
    if (shared_->program_id == 0) return false;
    if (command[0] == IpcMakeHeader(0x0861, 1, 2))
        return command[2] == IpcCallingPidDesc();
    return initialized_ && command[0] == IpcMakeHeader(0x0862, 1, 0);
}

Result FsUserService::Handle(IpcRouter&, Kernel& kernel, GuestMemory&, ThreadObject&,
                             IpcCommandBuffer& command) {
    if (!CanHandle(command)) return kResultNotFound;
    const auto id = IpcCommandId(command[0]);
    if (id == 0x0861) {
        // CallingPid's following word is a placeholder, NOT a trusted PID.
        // Resolve from the current kernel process in this single-process host.
        process_id_ = kernel.current_process()->process_id;
        program_id_ = shared_->program_id;
        sdk_version_ = command[1];
        initialized_ = true;
    } else {
        // Matches pinned FS_USER's service-global field; not thread priority
        // and not yet a modeled filesystem worker scheduler.
        shared_->priority = command[1];
    }
    command.fill(0);
    command[0] = IpcMakeHeader(id, 1, 0);
    command[1] = kResultSuccess;
    return kResultSuccess;
}
} // namespace lego::ctr
