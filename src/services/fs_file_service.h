#pragma once
#include "runtime/ctr_ipc.h"
#include "services/fs_shared_archive.h"
#include <stdexcept>

namespace lego::ctr {
// Host work bound, NOT a discovered firmware transfer limit.
inline constexpr std::uint32_t kMaxFsFileTransfer = 1024U * 1024U;
// FS file-session endpoint, not an fs:USER connection or an archive-table ID.
// Each OpenFile creates a new endpoint holding an actual contained host file.
// DuplicateHandle shares it; the final reference releases the host descriptor.
class FsFileService final : public IpcService {
public:
    explicit FsFileService(std::unique_ptr<SharedArchiveFile> file) : file_(std::move(file)) {
        if (!file_) throw std::invalid_argument("file session needs an open backend");
    }
    [[nodiscard]] const SharedArchiveFile& file() const noexcept { return *file_; }
    bool CanHandle(const IpcCommandBuffer& command) const noexcept override;
    Result Handle(IpcRouter&, Kernel&, GuestMemory&, ThreadObject&,
                  IpcCommandBuffer& command) override;
private:
    std::unique_ptr<SharedArchiveFile> file_;
};
} // namespace lego::ctr
