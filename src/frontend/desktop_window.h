#pragma once

#include "frontend/display_frame.h"
#include "frontend/screen_layout.h"
#include "runtime/ctr_host_frontend.h"

#include <filesystem>
#include <memory>
#include <string>

namespace lego::frontend {

struct DesktopOptions {
    int width{1280},height{720};
    bool hidden{};
    std::filesystem::path screenshot_path;
};

// The window and renderer are owned and used by the calling main thread.
// No SDL state or dependency enters the guest runtime library.
class DesktopWindow final : public ctr::HostFrontend {
public:
    explicit DesktopWindow(const DesktopOptions& options={});
    ~DesktopWindow() override;
    DesktopWindow(const DesktopWindow&)=delete;
    DesktopWindow& operator=(const DesktopWindow&)=delete;

    ctr::HostFrontendResult Pump(const ctr::GspGpuService& gpu,bool frame_ready,
                                 ctr::HostInputState& input) noexcept override;
    void InputSampled() noexcept override;
    [[nodiscard]] const char* error() const noexcept override;

    // Explicit frame presentation also serves the ROM-free compositor smoke.
    // Production Pump always obtains its frame from the live GSP/VRAM state.
    bool Present(const DualScreenFrame& frame) noexcept;
    bool SaveScreenshot(const std::filesystem::path& new_path) noexcept;
    bool SetTitle(const std::string& title) noexcept;
    [[nodiscard]] std::uint64_t present_count() const noexcept;
    [[nodiscard]] const std::filesystem::path& screenshot_saved_path() const noexcept;
    [[nodiscard]] const ScreenLayout& layout() const noexcept;
    [[nodiscard]] std::uint32_t window_id() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace lego::frontend
