#pragma once
#include "runtime/ctr_ipc.h"
#include "services/fs_shared_archive.h"

namespace lego::ctr {
// Bounded FS endpoint for the verified single-program runner. Shared archive
// opens/creation require an explicit host mount. Only file GetSize is supported.
class FsUserService final : public IpcService {
public:
    explicit FsUserService(std::uint64_t program_id);
    bool ConfigureSharedExtdataRoot(const std::filesystem::path& root) {
        return shared_->archives.ConfigureRoot(root);
    }
    [[nodiscard]] const SharedArchiveMounts& archives() const noexcept { return shared_->archives; }
    std::shared_ptr<IpcService> CreateSessionHandler() override;
    bool CanHandle(const IpcCommandBuffer& command) const noexcept override;
    Result Handle(IpcRouter&, Kernel&, GuestMemory&, ThreadObject&,
                  IpcCommandBuffer& command) override;

    [[nodiscard]] bool initialized() const noexcept { return initialized_; }
    [[nodiscard]] std::uint32_t process_id() const noexcept { return process_id_; }
    [[nodiscard]] std::uint64_t program_id() const noexcept { return program_id_; }
    [[nodiscard]] std::uint32_t sdk_version() const noexcept { return sdk_version_; }
    [[nodiscard]] std::uint32_t priority() const noexcept { return shared_->priority; }
private:
    struct SharedState {
        std::uint64_t program_id;
        std::uint32_t priority{0xFFFFFFFFU};
        SharedArchiveMounts archives;
    };
    explicit FsUserService(std::shared_ptr<SharedState> shared);
    std::shared_ptr<SharedState> shared_;
    bool initialized_{};
    std::uint32_t process_id_{};
    std::uint64_t program_id_{};
    std::uint32_t sdk_version_{}; // Diagnostic only; pinned HLE ignores version.
};
} // namespace lego::ctr
