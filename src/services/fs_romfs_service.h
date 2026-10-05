#pragma once
#include "runtime/ctr_ipc.h"
#include <filesystem>
#include <span>
#include <string>

namespace lego::ctr {
// Derived from the verified USA CCI, not guessed or a blank replacement image.
inline constexpr std::uint64_t kLegoRawRomfsBytes=769179648ULL;
inline constexpr std::uint64_t kLegoRomfsViewOffset=0x1000;
inline constexpr const char* kLegoRawRomfsSha256=
    "6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf";
inline constexpr std::uint32_t kMaxRomfsRead=1024U*1024U; // Host work bound only.

// An explicitly supplied image, held by one O_RDONLY descriptor. The launcher
// supplies the pinned game identity; ROM-free tests supply their own fixture hash.
class RomfsImage final {
public:
    static std::shared_ptr<const RomfsImage> OpenVerified(const std::filesystem::path&,
        std::uint64_t raw_bytes, std::uint64_t view_offset, const std::string& sha256);
    ~RomfsImage();
    RomfsImage(const RomfsImage&)=delete;
    RomfsImage& operator=(const RomfsImage&)=delete;
    [[nodiscard]] std::uint64_t size() const noexcept { return raw_bytes_-view_offset_; }
    [[nodiscard]] const std::string& sha256() const noexcept { return sha256_; }
    std::uint32_t Read(std::uint64_t offset, std::span<std::uint8_t> output) const;
private:
    RomfsImage()=default;
    void CheckUnchanged() const;
    void ReadRaw(std::uint64_t offset, std::span<std::uint8_t> output) const;
    int fd_{-1};
    std::uint64_t raw_bytes_{},view_offset_{};
    std::int64_t modified_seconds_{},modified_nanoseconds_{};
    std::string sha256_;
};

// Separate read-only endpoint. It cannot reach the writable extdata backend.
class RomfsFileService final : public IpcService {
public:
    explicit RomfsFileService(std::shared_ptr<const RomfsImage> image);
    bool CanHandle(const IpcCommandBuffer&) const noexcept override;
    Result Handle(IpcRouter&,Kernel&,GuestMemory&,ThreadObject&,IpcCommandBuffer&) override;
    [[nodiscard]] const RomfsImage& image() const noexcept { return *image_; }
private:
    std::shared_ptr<const RomfsImage> image_;
};
} // namespace lego::ctr
