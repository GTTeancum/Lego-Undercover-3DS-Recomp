#include "frontend/desktop_window.h"

#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace lego::frontend {
namespace {
SDL_FRect SdlRect(const Rect& r) noexcept {
    return {static_cast<float>(r.x),static_cast<float>(r.y),
            static_cast<float>(r.width),static_cast<float>(r.height)};
}
std::uint32_t KeyMask(SDL_Scancode key) noexcept {
    switch (key) {
    case SDL_SCANCODE_Z:return 1U<<0; // A
    case SDL_SCANCODE_X:return 1U<<1; // B
    case SDL_SCANCODE_BACKSPACE:return 1U<<2; // Select
    case SDL_SCANCODE_RETURN:return 1U<<3; // Start
    case SDL_SCANCODE_RIGHT:return 1U<<4;
    case SDL_SCANCODE_LEFT:return 1U<<5;
    case SDL_SCANCODE_UP:return 1U<<6;
    case SDL_SCANCODE_DOWN:return 1U<<7;
    case SDL_SCANCODE_W:return 1U<<8; // R
    case SDL_SCANCODE_Q:return 1U<<9; // L
    case SDL_SCANCODE_S:return 1U<<10; // X
    case SDL_SCANCODE_A:return 1U<<11; // Y
    default:return 0;
    }
}
}

struct DesktopWindow::Impl {
    SDL_Window* window{};
    SDL_Renderer* renderer{};
    std::array<SDL_Texture*,2> textures{};
    bool video_initialized{},expanded{},quit{},left_held{},mouse_known{},deferred{};
    bool circle_up{},circle_down{},circle_left{},circle_right{};
    float mouse_x{},mouse_y{};
    std::uint32_t buttons{},pressed_buttons{};
    std::optional<TouchPosition> pending_touch;
    ScreenLayout layout{};
    DualScreenFrame last_frame;
    bool have_frame{};
    std::uint64_t presents{};
    std::filesystem::path screenshot_requested,screenshot_saved;
    std::string failure;

    ~Impl() {
        for (auto* texture:textures) SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        if (video_initialized) SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
    bool Fail(const char* operation) {
        failure=std::string(operation)+": "+SDL_GetError();return false;
    }
    void ReleaseInput() noexcept {
        buttons=pressed_buttons=0;left_held=false;mouse_known=false;pending_touch.reset();
        circle_up=circle_down=circle_left=circle_right=false;
    }
    bool RefreshLayout() {
        if ((SDL_GetWindowFlags(window)&SDL_WINDOW_MINIMIZED)!=0) {
            deferred=true;ReleaseInput();return true;
        }
        int width=0,height=0;
        if (!SDL_GetCurrentRenderOutputSize(renderer,&width,&height)) return Fail("read window dimensions");
        if (width<4 || height<4) {
            deferred=true;ReleaseInput();return true;
        }
        const auto value=ComputeLayout(width,height,expanded);
        if (!value) {failure="window drawable is too small for two-screen presentation";return false;}
        layout=*value;deferred=false;return true;
    }
    bool Draw() {
        if (!RefreshLayout()) return false;
        if (deferred) return true;
        if (!SDL_SetRenderDrawColor(renderer,18,20,24,255) || !SDL_RenderClear(renderer))
            return Fail("clear desktop window");
        const auto source_top=SdlRect(layout.top_source),target_top=SdlRect(layout.top_destination);
        if (!SDL_RenderTexture(renderer,textures[0],&source_top,&target_top)) return Fail("draw main screen");
        const auto& target=layout.bottom_destination;
        const SDL_FRect border{static_cast<float>(target.x-2),static_cast<float>(target.y-2),
                              static_cast<float>(target.width+4),static_cast<float>(target.height+4)};
        if (!SDL_SetRenderDrawColor(renderer,210,214,222,255) || !SDL_RenderFillRect(renderer,&border))
            return Fail("draw touchscreen border");
        const auto source_bottom=SdlRect(layout.bottom_source),target_bottom=SdlRect(target);
        if (!SDL_RenderTexture(renderer,textures[1],&source_bottom,&target_bottom)) return Fail("draw touchscreen");
        return true;
    }
    bool Capture(const std::filesystem::path& new_path) {
        if (new_path.empty() || std::filesystem::exists(new_path)) {
            failure="screenshot requires a new nonempty file path";return false;
        }
        SDL_Surface* surface=SDL_RenderReadPixels(renderer,nullptr);
        if (!surface) return Fail("read rendered screenshot");
        const bool saved=SDL_SaveBMP(surface,new_path.string().c_str());
        SDL_DestroySurface(surface);
        if (!saved) return Fail("save rendered screenshot");
        return true;
    }
    bool Upload(const DualScreenFrame& frame) {
        for (unsigned i=0;i<2;++i) {
            const auto& screen=i ? frame.bottom : frame.top;
            const int width=i ? 320 : 400;
            if (screen.width!=width || screen.height!=240 ||
                screen.rgba.size()!=static_cast<std::size_t>(width)*240*4) {
                failure="invalid decoded screen dimensions or pixel count";return false;
            }
        }
        for (unsigned i=0;i<2;++i) {
            const auto& screen=i ? frame.bottom : frame.top;
            if (!SDL_UpdateTexture(textures[i],nullptr,screen.rgba.data(),screen.width*4))
                return Fail("upload LCD image");
        }
        return true;
    }
    bool Poll() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type==SDL_EVENT_QUIT) {quit=true;ReleaseInput();continue;}
            if (event.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID==SDL_GetWindowID(window)) {
                quit=true;ReleaseInput();continue;
            }
            if ((event.type==SDL_EVENT_WINDOW_FOCUS_LOST || event.type==SDL_EVENT_WINDOW_MOUSE_LEAVE) &&
                event.window.windowID==SDL_GetWindowID(window)) {
                if (event.type==SDL_EVENT_WINDOW_FOCUS_LOST) ReleaseInput();
                else {left_held=false;mouse_known=false;pending_touch.reset();}
                continue;
            }
            if (event.type==SDL_EVENT_WINDOW_MINIMIZED && event.window.windowID==SDL_GetWindowID(window)) {
                deferred=true;ReleaseInput();continue;
            }
            if ((event.type==SDL_EVENT_WINDOW_RESTORED || event.type==SDL_EVENT_WINDOW_EXPOSED) &&
                event.window.windowID==SDL_GetWindowID(window) && have_frame) {
                if (!Draw()) return false;
                if (!deferred) {
                    if (!SDL_RenderPresent(renderer)) return Fail("redraw restored window");
                    ++presents;
                }
                continue;
            }
            if (event.type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED && event.window.windowID==SDL_GetWindowID(window)) {
                left_held=false;mouse_known=false;pending_touch.reset();
                if (!RefreshLayout()) return false;
                if (have_frame) {
                    if (!Draw()) return false;
                    if (!deferred) {
                        if (!SDL_RenderPresent(renderer)) return Fail("redraw resized window");
                        ++presents;
                    }
                }
                continue;
            }
            if (event.type==SDL_EVENT_KEY_DOWN || event.type==SDL_EVENT_KEY_UP) {
                if (event.key.windowID!=SDL_GetWindowID(window)) continue;
                const bool down=event.type==SDL_EVENT_KEY_DOWN;
                const auto key=event.key.scancode;
                if (down && !event.key.repeat) {
                    if (key==SDL_SCANCODE_ESCAPE) {quit=true;ReleaseInput();continue;}
                    if (key==SDL_SCANCODE_TAB) {
                        expanded=!expanded;left_held=false;mouse_known=false;
                        pressed_buttons=0;pending_touch.reset();
                        if (!RefreshLayout()) return false;
                        if (have_frame) {
                            if (!Draw()) return false;
                            if (!deferred) {
                                if (!SDL_RenderPresent(renderer)) return Fail("redraw enlarged touchscreen");
                                ++presents;
                            }
                        }
                        continue;
                    }
                    if (key==SDL_SCANCODE_F11) {
                        left_held=false;mouse_known=false;pending_touch.reset();
                        const bool fullscreen=(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN)!=0;
                        if (!SDL_SetWindowFullscreen(window,!fullscreen)) return Fail("toggle fullscreen");
                        continue;
                    }
                }
                const auto mask=KeyMask(key);
                if (down) buttons|=mask; else buttons&=~mask;
                if (down && !event.key.repeat) pressed_buttons|=mask;
                if (key==SDL_SCANCODE_I) circle_up=down;
                if (key==SDL_SCANCODE_K) circle_down=down;
                if (key==SDL_SCANCODE_J) circle_left=down;
                if (key==SDL_SCANCODE_L) circle_right=down;
                continue;
            }
            if (event.type==SDL_EVENT_MOUSE_BUTTON_DOWN || event.type==SDL_EVENT_MOUSE_BUTTON_UP ||
                event.type==SDL_EVENT_MOUSE_MOTION) {
                const auto id=event.type==SDL_EVENT_MOUSE_MOTION ? event.motion.windowID : event.button.windowID;
                if (id!=SDL_GetWindowID(window)) continue;
                if (!SDL_ConvertEventToRenderCoordinates(renderer,&event)) return Fail("map pointer to window pixels");
                if (event.type==SDL_EVENT_MOUSE_MOTION) {
                    mouse_x=event.motion.x;mouse_y=event.motion.y;mouse_known=true;
                    if (!MapTouch(layout,mouse_x,mouse_y)) {left_held=false;pending_touch.reset();}
                } else if (event.button.button==SDL_BUTTON_LEFT) {
                    mouse_x=event.button.x;mouse_y=event.button.y;mouse_known=true;
                    const auto touch=MapTouch(layout,mouse_x,mouse_y);
                    left_held=event.type==SDL_EVENT_MOUSE_BUTTON_DOWN && touch.has_value();
                    if (left_held) pending_touch=touch;
                    else if (!touch) pending_touch.reset();
                }
            }
        }
        if ((SDL_GetWindowFlags(window)&SDL_WINDOW_MINIMIZED)!=0) ReleaseInput();
        return true;
    }
};

DesktopWindow::DesktopWindow(const DesktopOptions& options) : impl_(std::make_unique<Impl>()) {
    if (options.width<160 || options.height<120 || options.width>16384 || options.height>16384)
        throw std::runtime_error("desktop dimensions must be between 160x120 and 16384x16384");
    if (!options.screenshot_path.empty() && std::filesystem::exists(options.screenshot_path))
        throw std::runtime_error("desktop screenshot file already exists");
    impl_->screenshot_requested=options.screenshot_path;
    SDL_SetMainReady();
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) throw std::runtime_error(std::string("SDL video initialization: ")+SDL_GetError());
    impl_->video_initialized=true;
    const auto flags=SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY|(options.hidden ? SDL_WINDOW_HIDDEN : 0);
    if (!SDL_CreateWindowAndRenderer("LEGO City Undercover - Tab: touchscreen | F11: fullscreen | Esc: quit",
                                     options.width,options.height,flags,&impl_->window,&impl_->renderer))
        throw std::runtime_error(std::string("SDL window initialization: ")+SDL_GetError());
    if (!SDL_SetWindowMinimumSize(impl_->window,160,120))
        throw std::runtime_error(std::string("SDL minimum window dimensions: ")+SDL_GetError());
    for (unsigned i=0;i<2;++i) {
        impl_->textures[i]=SDL_CreateTexture(impl_->renderer,SDL_PIXELFORMAT_RGBA32,
                                            SDL_TEXTUREACCESS_STREAMING,i ? 320 : 400,240);
        if (!impl_->textures[i] || !SDL_SetTextureScaleMode(impl_->textures[i],SDL_SCALEMODE_NEAREST) ||
            !SDL_SetTextureBlendMode(impl_->textures[i],SDL_BLENDMODE_NONE))
            throw std::runtime_error(std::string("SDL screen texture: ")+SDL_GetError());
    }
    if (!impl_->RefreshLayout()) throw std::runtime_error(impl_->failure);
}

DesktopWindow::~DesktopWindow()=default;

ctr::HostFrontendResult DesktopWindow::Pump(const ctr::GspGpuService& gpu,bool frame_ready,
                                           ctr::HostInputState& input) noexcept {
    try {
        if (!impl_->Poll()) {input={};return ctr::HostFrontendResult::Error;}
        if (impl_->quit) {input={};return ctr::HostFrontendResult::Quit;}
        if (frame_ready) {
            DualScreenFrame frame;
            if (!ReadDisplayFrame(gpu,frame,impl_->failure) || !Present(frame)) {
                input={};return ctr::HostFrontendResult::Error;
            }
        }
        input={};input.buttons=impl_->buttons|impl_->pressed_buttons;
        input.circle_x=static_cast<std::int16_t>(154*(int(impl_->circle_right)-int(impl_->circle_left)));
        input.circle_y=static_cast<std::int16_t>(154*(int(impl_->circle_up)-int(impl_->circle_down)));
        if (impl_->left_held && impl_->mouse_known) {
            if (const auto touch=MapTouch(impl_->layout,impl_->mouse_x,impl_->mouse_y)) {
                input.touching=true;input.touch_x=touch->x;input.touch_y=touch->y;
            }
        } else if (impl_->pending_touch) {
            input.touching=true;input.touch_x=impl_->pending_touch->x;input.touch_y=impl_->pending_touch->y;
        }
        return ctr::HostFrontendResult::Continue;
    } catch (const std::exception& ex) {
        impl_->failure=ex.what();input={};return ctr::HostFrontendResult::Error;
    }
}

void DesktopWindow::InputSampled() noexcept {
    // Only a committed guest HID sample consumes edges. Initial presentation,
    // window redraws and final screenshots never consume pending input.
    impl_->pressed_buttons=0;impl_->pending_touch.reset();
}

bool DesktopWindow::Present(const DualScreenFrame& frame) noexcept {
    try {
        if (!impl_->Upload(frame)) return false;
        impl_->last_frame=frame;impl_->have_frame=true;
        if (!impl_->Draw()) return false;
        if (impl_->deferred) return true;
        if (!impl_->screenshot_requested.empty() && impl_->screenshot_saved.empty() && frame.HasNonBlackPixels()) {
            if (!impl_->Capture(impl_->screenshot_requested)) return false;
            impl_->screenshot_saved=impl_->screenshot_requested;
        }
        if (!SDL_RenderPresent(impl_->renderer)) return impl_->Fail("present desktop window");
        ++impl_->presents;
        return true;
    } catch (const std::exception& ex) {impl_->failure=ex.what();return false;}
}

bool DesktopWindow::SaveScreenshot(const std::filesystem::path& new_path) noexcept {
    try {
        if (!impl_->have_frame) {impl_->failure="no LCD frame has been presented";return false;}
        if (!impl_->Draw()) return false;
        if (impl_->deferred) {impl_->failure="cannot capture a minimized or unavailable drawable";return false;}
        if (!impl_->Capture(new_path)) return false;
        if (!SDL_RenderPresent(impl_->renderer)) return impl_->Fail("present captured desktop window");
        ++impl_->presents;return true;
    } catch (const std::exception& ex) {impl_->failure=ex.what();return false;}
}

bool DesktopWindow::SetTitle(const std::string& title) noexcept {
    if (!SDL_SetWindowTitle(impl_->window,title.c_str())) return impl_->Fail("set window title");
    return true;
}
const char* DesktopWindow::error() const noexcept {return impl_->failure.empty() ? nullptr : impl_->failure.c_str();}
std::uint64_t DesktopWindow::present_count() const noexcept {return impl_->presents;}
const std::filesystem::path& DesktopWindow::screenshot_saved_path() const noexcept {return impl_->screenshot_saved;}
const ScreenLayout& DesktopWindow::layout() const noexcept {return impl_->layout;}
std::uint32_t DesktopWindow::window_id() const noexcept {return SDL_GetWindowID(impl_->window);}

} // namespace lego::frontend
