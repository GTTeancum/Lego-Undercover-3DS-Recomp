#pragma once
#include "runtime/ctr_ipc.h"
#include "services/fs_shared_archive.h"
#include <stdexcept>

namespace lego::ctr {
// FS file-session endpoint, not an fs:USER connection or an archive-table ID.
// Each OpenFile creates a new endpoint holding an actual contained host file.
// DuplicateHandle shares it; the final reference releases the host descriptor.
class FsFileService final : public IpcService {
public:
    explicit FsFileService(std::unique_ptr<SharedArchiveFile> file) : file_(std::move(file)) {
        if (!file_) throw std::invalid_argument("file session needs an open backend");
    }
    [[nodiscard]] const SharedArchiveFile& file() const noexcept { return *file_; }
    bool CanHandle(const IpcCommandBuffer& command) const noexcept override {
        return command[0] == IpcMakeHeader(0x0804, 0, 0);
    }
    Result Handle(IpcRouter&, Kernel&, GuestMemory&, ThreadObject&,
                  IpcCommandBuffer& command) override {
        if (!CanHandle(command)) return kResultNotFound;
        const auto size = file_->size();
        command.fill(0);
        command[0] = IpcMakeHeader(0x0804, 3, 0);
        command[1] = kResultSuccess;
        command[2] = static_cast<std::uint32_t>(size);
        command[3] = static_cast<std::uint32_t>(size >> 32);
        return kResultSuccess;
    }
private:
    std::unique_ptr<SharedArchiveFile> file_;
};
} // namespace lego::ctr
