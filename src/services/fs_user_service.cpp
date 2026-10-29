#include "services/fs_user_service.h"
#include "services/fs_file_service.h"
#include <utility>
#include <string>

namespace lego::ctr {
FsUserService::FsUserService(std::uint64_t program_id)
    : shared_(std::make_shared<SharedState>(SharedState{program_id})) {}

FsUserService::FsUserService(std::shared_ptr<SharedState> shared)
    : shared_(std::move(shared)) {}

Result FsUserService::CreateSessionHandler(std::shared_ptr<IpcService>* out) {
    if (!out) return kResultInvalidPointer;
    // A new connection gets its own initialization/program binding. Ordinary
    // DuplicateHandle keeps the same ClientSessionObject and handler instead.
    *out = std::shared_ptr<IpcService>(new FsUserService(shared_));
    return kResultSuccess;
}

bool FsUserService::CanHandle(const IpcCommandBuffer& command) const noexcept {
    if (shared_->program_id == 0) return false;
    if (command[0] == IpcMakeHeader(0x0861, 1, 2))
        return command[2] == IpcCallingPidDesc();
    if (!initialized_) return false;
    if (command[0] == IpcMakeHeader(0x0862, 1, 0)) return true;
    if (command[0] == IpcMakeHeader(0x0863, 0, 0)) return true;
    if (command[0] == IpcMakeHeader(0x084C,9,2)) {
        // Bounded first-time FormatSaveData: only the initialized program's
        // Empty path and canonical bool are supported. Bucket counts are
        // parsed/bounded but ignored by the pinned local archive backend.
        return shared_->save_archives.CanFormat() && command[1]==4 && command[2]==1 &&
               command[3]==1 && command[4]>0 && command[4]<=kSaveDataFormatMaxBytes/512U &&
               command[5]<=kSaveDataFormatMaxEntries && command[6]<=kSaveDataFormatMaxEntries &&
               command[7]<=kSaveDataFormatMaxBuckets && command[8]<=kSaveDataFormatMaxBuckets &&
               command[9]<=1 && command[10]==0x00004002;
    }
    if (command[0] == IpcMakeHeader(0x0803,8,4)) {
        // Observed SelfNCCH/Empty/Binary RomFS path. Other archive types/modes
        // are still host stops, even though pinned SelfNCCH ignores open mode.
        return shared_->romfs && command[2]==3 && command[3]==1 && command[4]==1 &&
               command[5]==2 && command[6]==12 && command[7]==1 && command[8]==0 &&
               command[9]==0x00004802 && command[11]==0x00030002;
    }
    if (command[0] == IpcMakeHeader(0x0802, 7, 2)) {
        const auto archive = std::uint64_t(command[2]) | (std::uint64_t(command[3]) << 32);
        const auto bytes = command[5];
        if (command[4]!=4 || bytes<4 || bytes>0x1000 || (bytes&1)!=0 ||
            command[8]!=((bytes<<14)|2)) return false;
        if (IsSaveDataArchiveHandle(archive))
            return shared_->save_archives.CanOpenFiles() && command[6]==7 && command[7]==0;
        return shared_->archives.CanOpenFiles() && (command[6]&~7U)==0;
    }
    if (command[0] == IpcMakeHeader(0x0808, 8, 2)) {
        const auto archive = std::uint64_t(command[2]) | (std::uint64_t(command[3]) << 32);
        if (IsSaveDataArchiveHandle(archive)) return false;
        const auto bytes = command[5];
        const auto size = std::uint64_t(command[7]) | (std::uint64_t(command[8]) << 32);
        return shared_->archives.CanCreateFiles() && command[4] == 4 &&
               bytes >= 4 && bytes <= 0x1000 && (bytes & 1) == 0 &&
               command[9] == ((bytes << 14) | 2) && size <= kMaxSharedCreateSize;
    }
    if (command[0] == IpcMakeHeader(0x080C, 3, 2) && command[1] == 4)
        return shared_->save_archives.configured() && command[2] == 1 &&
               command[3] == 1 && command[4] == 0x00004002;
    return shared_->archives.configured() &&
           command[0] == IpcMakeHeader(0x080C, 3, 2) && command[1] == 7 &&
           command[2] == 2 && command[3] == 12 && command[4] == 0x00030002;

}

Result FsUserService::Handle(IpcRouter& router, Kernel& kernel, GuestMemory& memory, ThreadObject& thread,
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
    if (id == 0x0863) {
        // Pinned FS_USER GetPriority returns the module's stored u32 unchanged.
        // UINT32_MAX only triggers a reference log; it is not replaced with a
        // thread/default priority. This does not implement worker scheduling.
        const auto priority = shared_->priority;
        command.fill(0);
        command[0] = IpcMakeHeader(id, 2, 0);
        command[1] = kResultSuccess;
        command[2] = priority;
        return kResultSuccess;
    }
    if (id == 0x084C) {
        const auto address=command[11];
        const auto reply64=std::uint64_t(thread.tls_address)+kIpcCommandBufferOffset;
        if (std::uint64_t(address)+1>0x100000000ULL || !memory.IsReadable(address,1) ||
            reply64+sizeof(command)>0x100000000ULL ||
            !memory.IsReadable(static_cast<std::uint32_t>(reply64),sizeof(command)) ||
            !memory.IsWritable(static_cast<std::uint32_t>(reply64),sizeof(command)))
            return kResultInvalidPointer;
        // Empty transport is opaque, not a NUL filename or a selected host path.
        // Keep both it and the eventual response on private guest backing.
        if (!memory.IsCachelessPrivateRange(address,1) ||
            !memory.IsCachelessPrivateRange(static_cast<std::uint32_t>(reply64),sizeof(command))) {
            router.RequestHostStop("SaveData format requires private path and reply backing");
            return kResultSuccess;
        }
        Result result;
        try {
            // Router writability alone could admit a device callback or a later
            // allocation of reservation metadata. Prepare before filesystem
            // publication so the synchronous ordinary reply cannot do either.
            if (!memory.PrepareDeviceWrite(static_cast<std::uint32_t>(reply64),sizeof(command))) {
                router.RequestHostStop("SaveData format reply preparation failed");
                return kResultSuccess;
            }
            const SaveDataFormatInfo info{command[4]*512U,command[5],command[6],command[9]!=0};
            result=shared_->save_archives.FormatMissing(program_id_,info);
        } catch (const std::exception& error) {
            router.RequestHostStop(error.what());
            return kResultSuccess;
        }
        command.fill(0);
        command[0]=IpcMakeHeader(id,1,0);
        command[1]=result;
        return kResultSuccess;
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
        const auto archive = std::uint64_t(command[2]) | (std::uint64_t(command[3]) << 32);
        const bool savedata=IsSaveDataArchiveHandle(archive);
        if (std::uint64_t(address) + bytes > 0x100000000ULL || !memory.IsReadable(address, bytes))
            return kResultInvalidPointer;
        const auto reply64=std::uint64_t(thread.tls_address)+kIpcCommandBufferOffset;
        if (savedata) {
            if (reply64+sizeof(command)>0x100000000ULL ||
                !memory.IsReadable(static_cast<std::uint32_t>(reply64),sizeof(command)) ||
                !memory.IsWritable(static_cast<std::uint32_t>(reply64),sizeof(command)))
                return kResultInvalidPointer;
            if (!memory.IsCachelessPrivateRange(address,bytes) ||
                !memory.IsCachelessPrivateRange(static_cast<std::uint32_t>(reply64),sizeof(command))) {
                router.RequestHostStop("SaveData OpenFile requires private path and reply backing");
                return kResultSuccess;
            }
        }
        Result result = kResultFsInvalidPath;
        ::lego::ctr::Handle file_handle = 0;
        SaveDataFileOpenPlan plan;
        try {
            if (savedata && !memory.PrepareDeviceWrite(static_cast<std::uint32_t>(reply64),sizeof(command))) {
                router.RequestHostStop("SaveData OpenFile reply preparation failed");
                return kResultSuccess;
            }
            std::u16string path;
            path.reserve(bytes / 2);
            for (std::uint32_t offset = 0; offset < bytes; offset += 2) {
                std::uint16_t unit = 0;
                if (!memory.Read16(address + offset, &unit)) return kResultInvalidPointer;
                path += static_cast<char16_t>(unit);
            }
            if (path.back() == 0) {
                path.pop_back();
                if (savedata) {
                    const auto* mount=shared_->save_archives.Find(archive);
                    if (!mount || mount->program_id!=program_id_) result=kResultFsInvalidArchiveHandle;
                    else if (kernel.handles().OpenHandleCount()==HandleTable::kMaxCount)
                        result=kResultOutOfHandles;
                    else {
                        result=shared_->save_archives.StageOpenFile(archive,program_id_,path,command[6],plan);
                        if (result==kResultSuccess) {
                            auto service=std::make_shared<FsFileService>(plan.TakeFile());
                            auto client=std::make_shared<ClientSessionObject>("fs:File",std::move(service));
                            // This synchronous handler cannot interleave another
                            // handle allocation. Create is fixed-array/noexcept;
                            // capacity, endpoint and reply are ready before the
                            // new name appears. Failed publication consumes no
                            // handle generation or caller-visible file name.
                            plan.Publish();
                            result=kernel.handles().Create(&file_handle,std::move(client));
                            if (result!=kResultSuccess) {
                                router.RequestHostStop("SaveData file published but handle-capacity invariant failed");
                                return kResultSuccess;
                            }
                        }
                    }
                } else {
                    std::unique_ptr<SharedArchiveFile> file;
                    result = shared_->archives.OpenFile(archive, path, command[6], &file);
                    if (result == kResultSuccess) {
                        auto service = std::make_shared<FsFileService>(std::move(file));
                        result = kernel.handles().Create(&file_handle,
                            std::make_shared<ClientSessionObject>("fs:File", std::move(service)));
                    }
                }
            }
        } catch (const std::exception& error) {
            if (!plan.Rollback())
                router.RequestHostStop("SaveData OpenFile failed and owned staging cleanup failed");
            else router.RequestHostStop(error.what());
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
        if (command[1] == 4) {
            if (std::uint64_t(address) + 1 > 0x100000000ULL || !memory.IsReadable(address, 1))
                return kResultInvalidPointer;
            // PathType::Empty ignores the observed byte 0x3C. It is transport
            // data, not a NUL-terminated name or a caller-selected title path.
            std::uint64_t handle = 0;
            Result result;
            try {
                result = shared_->save_archives.Open(program_id_, &handle);
            } catch (const std::exception& error) {
                router.RequestHostStop(error.what());
                return kResultSuccess; // The original request remains uncommitted.
            }
            command.fill(0);
            command[0] = IpcMakeHeader(id, 3, 0);
            command[1] = result;
            command[2] = static_cast<std::uint32_t>(handle);
            command[3] = static_cast<std::uint32_t>(handle >> 32);
            return kResultSuccess;
        }
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
