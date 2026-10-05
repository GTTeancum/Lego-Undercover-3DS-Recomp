#include "services/fs_user_service.h"
#include "services/fs_file_service.h"
#include <utility>
#include <string>

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
    if (!initialized_) return false;
    if (command[0] == IpcMakeHeader(0x0862, 1, 0)) return true;
    if (command[0] == IpcMakeHeader(0x0803,8,4)) {
        // Observed SelfNCCH/Empty/Binary RomFS path. Other archive types/modes
        // are still host stops, even though pinned SelfNCCH ignores open mode.
        return shared_->romfs && command[2]==3 && command[3]==1 && command[4]==1 &&
               command[5]==2 && command[6]==12 && command[7]==1 && command[8]==0 &&
               command[9]==0x00004802 && command[11]==0x00030002;
    }
    if (command[0] == IpcMakeHeader(0x0802, 7, 2)) {
        const auto bytes = command[5];
        return shared_->archives.CanOpenFiles() && command[4] == 4 &&
               bytes >= 4 && bytes <= 0x1000 && (bytes & 1) == 0 &&
               command[8] == ((bytes << 14) | 2) && (command[6] & ~7U) == 0;
    }
    if (command[0] == IpcMakeHeader(0x0808, 8, 2)) {
        const auto bytes = command[5];
        const auto size = std::uint64_t(command[7]) | (std::uint64_t(command[8]) << 32);
        return shared_->archives.CanCreateFiles() && command[4] == 4 &&
               bytes >= 4 && bytes <= 0x1000 && (bytes & 1) == 0 &&
               command[9] == ((bytes << 14) | 2) && size <= kMaxSharedCreateSize;
    }
    return shared_->archives.configured() &&
           command[0] == IpcMakeHeader(0x080C, 3, 2) && command[1] == 7 &&
           command[2] == 2 && command[3] == 12 && command[4] == 0x00030002;

}

Result FsUserService::Handle(IpcRouter& router, Kernel& kernel, GuestMemory& memory, ThreadObject&,
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
    } else if (id == 0x0862) {
        // Matches pinned FS_USER's service-global field; not thread priority
        // and not yet a modeled filesystem worker scheduler.
        shared_->priority = command[1];
    }
    if (id == 0x0803) {
        const auto archive_address=command[10], file_address=command[12];
        if (std::uint64_t(archive_address)+1>0x100000000ULL ||
            std::uint64_t(file_address)+12>0x100000000ULL ||
            !memory.IsReadable(archive_address,1) || !memory.IsReadable(file_address,12))
            return kResultInvalidPointer;
        // The Empty path's one byte is incidental (0xE8 in the actual request),
        // NOT a NUL-terminated filename. Validate its span, do not interpret it.
        for (unsigned i=0;i<3;++i) {
            std::uint32_t word=0;
            if (!memory.Read32(file_address+4*i,&word)) return kResultInvalidPointer;
            if (word!=0) {
                router.RequestHostStop("unsupported SelfNCCH binary path");
                return kResultSuccess;
            }
        }
        ::lego::ctr::Handle handle=0;
        Result result;
        try {
            auto endpoint=std::make_shared<RomfsFileService>(shared_->romfs);
            result=kernel.handles().Create(&handle,
                std::make_shared<ClientSessionObject>("fs:RomFS",std::move(endpoint)));
        } catch(const std::exception& error) {
            router.RequestHostStop(error.what()); return kResultSuccess;
        }
        // The transient SelfNCCH archive is not inserted into the shared-extdata
        // table. The returned file endpoint itself retains the actual image.
        command.fill(0); command[0]=IpcMakeHeader(id,1,2); command[1]=result;
        command[2]=IpcMoveHandleDesc(); command[3]=handle;
        return kResultSuccess;
    }
    if (id == 0x0802) {
        const auto bytes = command[5];
        const auto address = command[9];
        if (std::uint64_t(address) + bytes > 0x100000000ULL || !memory.IsReadable(address, bytes))
            return kResultInvalidPointer;
        Result result = kResultFsInvalidPath;
        ::lego::ctr::Handle file_handle = 0;
        try {
            std::u16string path;
            path.reserve(bytes / 2);
            for (std::uint32_t offset = 0; offset < bytes; offset += 2) {
                std::uint16_t unit = 0;
                if (!memory.Read16(address + offset, &unit)) return kResultInvalidPointer;
                path += static_cast<char16_t>(unit);
            }
            if (path.back() == 0) {
                path.pop_back();
                const auto archive = std::uint64_t(command[2]) | (std::uint64_t(command[3]) << 32);
                std::unique_ptr<SharedArchiveFile> file;
                result = shared_->archives.OpenFile(archive, path, command[6], &file);
                if (result == kResultSuccess) {
                    auto service = std::make_shared<FsFileService>(std::move(file));
                    result = kernel.handles().Create(&file_handle,
                        std::make_shared<ClientSessionObject>("fs:File", std::move(service)));
                }
            }
        } catch (const std::exception& error) {
            router.RequestHostStop(error.what());
            return kResultSuccess; // No response committed; all temporary owners unwind.
        }
        // Same reply shape on success and failure; a failed open moves null.
        // Transaction/attributes are ignored by the pinned extdata backend.
        command.fill(0);
        command[0] = IpcMakeHeader(id, 1, 2);
        command[1] = result;
        command[2] = IpcMoveHandleDesc();
        command[3] = file_handle;
        return kResultSuccess;
    }
    if (id == 0x0808) {
        const auto bytes = command[5];
        const auto address = command[10];
        if (std::uint64_t(address) + bytes > 0x100000000ULL || !memory.IsReadable(address, bytes))
            return kResultInvalidPointer;
        std::u16string path;
        path.reserve(bytes / 2);
        for (std::uint32_t offset = 0; offset < bytes; offset += 2) {
            std::uint16_t unit = 0;
            if (!memory.Read16(address + offset, &unit)) return kResultInvalidPointer;
            path += static_cast<char16_t>(unit);
        }
        const auto archive = std::uint64_t(command[2]) | (std::uint64_t(command[3]) << 32);
        const auto size = std::uint64_t(command[7]) | (std::uint64_t(command[8]) << 32);
        Result result = kResultFsInvalidPath;
        if (path.back() == 0) {
            path.pop_back();
            // Transaction ID and attributes are ignored by the pinned HLE path.
            // This does not model transactions or store host file attributes.
            try {
                result = shared_->archives.CreateFile(archive, path, size);
            } catch (const std::exception& error) {
                router.RequestHostStop(error.what());
                return kResultSuccess; // Router stops; no guest response is written.
            }
        }
        command.fill(0);
        command[0] = IpcMakeHeader(id, 1, 0);
        command[1] = result;
        return kResultSuccess;
    }
    if (id == 0x080C) {
        const auto address = command[5];
        if (std::uint64_t(address) + 12 > 0x100000000ULL || !memory.IsReadable(address, 12))
            return kResultInvalidPointer;
        std::uint32_t media = 0, low_id = 0, high_id = 0;
        if (!memory.Read32(address, &media) || !memory.Read32(address+4, &low_id) ||
            !memory.Read32(address+8, &high_id)) return kResultInvalidPointer;
        // Pinned SharedExtSaveData ignores media and replaces high_id with
        // 0x48000. No caller-provided string becomes a host path component.
        (void)media;
        (void)high_id;
        std::uint64_t handle = 0;
        const Result result = shared_->archives.Open(low_id, &handle);
        command.fill(0);
        command[0] = IpcMakeHeader(id, 3, 0);
        command[1] = result;
        command[2] = static_cast<std::uint32_t>(handle);
        command[3] = static_cast<std::uint32_t>(handle >> 32);
        return kResultSuccess;
    }
    command.fill(0);
    command[0] = IpcMakeHeader(id, 1, 0);
    command[1] = kResultSuccess;
    return kResultSuccess;
}
} // namespace lego::ctr
