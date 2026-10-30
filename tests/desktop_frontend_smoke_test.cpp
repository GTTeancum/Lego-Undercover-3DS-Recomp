#include "frontend/desktop_window.h"
#include "services/gsp_gpu_service.h"

#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
using namespace lego::frontend;
int failures=0;
#define CHECK(x) do {if (!(x)) {std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}} while(0)
constexpr std::array<std::uint8_t,4> kTop{30,80,140,255};
constexpr std::array<std::array<std::uint8_t,4>,4> kBottom{{
    {220,120,30,255},{50,180,90,255},{170,60,160,255},{235,210,70,255}}};

DualScreenFrame Fixture(bool black=false) {
    DualScreenFrame frame{{400,240,std::vector<std::uint8_t>(400*240*4)},
                          {320,240,std::vector<std::uint8_t>(320*240*4)}};
    for (unsigned screen=0;screen<2;++screen) {
        auto& image=screen ? frame.bottom : frame.top;
        for (int y=0;y<240;++y)for (int x=0;x<image.width;++x) {
            const auto color=black ? std::array<std::uint8_t,4>{0,0,0,255} :
                screen ? kBottom[(y>=120 ? 2 : 0)+(x>=160 ? 1 : 0)] : kTop;
            const auto p=(y*image.width+x)*4;
            for (unsigned c=0;c<4;++c) image.rgba[p+c]=color[c];
        }
    }
    return frame;
}
std::array<std::uint8_t,4> SurfacePixel(const SDL_Surface& surface,int x,int y) {
    const auto* p=static_cast<const std::uint8_t*>(surface.pixels)+y*surface.pitch+x*4;
    return {p[0],p[1],p[2],p[3]};
}
void Mouse(std::uint32_t id,Uint32 type,float x,float y) {
    SDL_Event event{};event.type=type;
    if (type==SDL_EVENT_MOUSE_MOTION) {
        event.motion.windowID=id;event.motion.x=x;event.motion.y=y;
    } else {
        event.button.windowID=id;event.button.button=SDL_BUTTON_LEFT;
        event.button.x=x;event.button.y=y;event.button.down=type==SDL_EVENT_MOUSE_BUTTON_DOWN;
    }
    CHECK(SDL_PushEvent(&event));
}
void Key(std::uint32_t id,SDL_Scancode key,bool down) {
    SDL_Event event{};event.type=down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.windowID=id;event.key.scancode=key;event.key.down=down;
    CHECK(SDL_PushEvent(&event));
}
void WindowEvent(std::uint32_t id,Uint32 type) {
    SDL_Event event{};event.type=type;event.window.windowID=id;
    CHECK(SDL_PushEvent(&event));
}
std::vector<char> Bytes(const std::filesystem::path& path) {
    std::ifstream stream(path,std::ios::binary);
    return {std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
}
}

int main(int argc,char** argv) {
    std::filesystem::path owned_directory;
    try {
        std::filesystem::path screenshot;
        if (argc==3 && std::string_view(argv[1])=="--screenshot") screenshot=argv[2];
        else if (argc!=1) throw std::runtime_error("usage: desktop_frontend_smoke_test [--screenshot NEW_BMP]");
        else {
            const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
            owned_directory=std::filesystem::temp_directory_path()/("lego-desktop-smoke-"+std::to_string(stamp));
            if (!std::filesystem::create_directory(owned_directory)) throw std::runtime_error("cannot create unique smoke output directory");
            screenshot=owned_directory/"synthetic-pip.bmp";
        }
        {
            DesktopOptions options;options.width=800;options.height=600;options.screenshot_path=screenshot;
            DesktopWindow window(options);
            lego::ctr::GspGpuService gpu;
            CHECK(gpu.ConfigureVram(lego::ctr::GpuVramBank::ReferenceZero()));
            lego::ctr::HostInputState input;
            auto poll=[&] {return window.Pump(gpu,false,input);};
            CHECK(window.Pump(gpu,true,input)==lego::ctr::HostFrontendResult::Continue);
            CHECK(window.Present(Fixture(true)));CHECK(!std::filesystem::exists(screenshot));
            CHECK(window.Present(Fixture()));CHECK(std::filesystem::exists(screenshot));
            CHECK(window.screenshot_saved_path()==screenshot);
            const auto layout=window.layout();
            SDL_Surface* loaded=SDL_LoadBMP(screenshot.string().c_str());CHECK(loaded!=nullptr);
            SDL_Surface* rgba=loaded ? SDL_ConvertSurface(loaded,SDL_PIXELFORMAT_RGBA32) : nullptr;
            CHECK(rgba!=nullptr);
            if (rgba) {
                const auto& top=layout.top_destination;
                const auto& bottom=layout.bottom_destination;
                CHECK(SurfacePixel(*rgba,top.x+10,top.y+10)==kTop);
                for (int row=0;row<2;++row)for (int col=0;col<2;++col) {
                    const int x=bottom.x+bottom.width*(1+2*col)/4;
                    const int y=bottom.y+bottom.height*(1+2*row)/4;
                    CHECK(SurfacePixel(*rgba,x,y)==kBottom[row*2+col]);
                }
                CHECK(bottom.x>rgba->w/2 && bottom.y>rgba->h/2);
                CHECK(bottom.x+bottom.width<rgba->w && bottom.y+bottom.height<rgba->h);
                SDL_DestroySurface(rgba);
            }
            SDL_DestroySurface(loaded);
            // Refusing a duplicate screenshot must preserve its original bytes.
            const auto first=Bytes(screenshot);CHECK(!first.empty());
            CHECK(!window.SaveScreenshot(screenshot));CHECK(Bytes(screenshot)==first);

            const auto id=window.window_id();
            const auto& small=layout.bottom_destination;
            const float cx=small.x+small.width/2.0f,cy=small.y+small.height/2.0f;
            Mouse(id,SDL_EVENT_MOUSE_BUTTON_DOWN,cx,cy);Key(id,SDL_SCANCODE_Z,true);
            CHECK(poll()==lego::ctr::HostFrontendResult::Continue);
            CHECK(input.touching && input.touch_x==160 && input.touch_y==120);
            CHECK(input.buttons==1);
            Mouse(id,SDL_EVENT_MOUSE_MOTION,0,0);CHECK(poll()==lego::ctr::HostFrontendResult::Continue);
            CHECK(!input.touching && input.buttons==1);
            Mouse(id,SDL_EVENT_MOUSE_MOTION,cx,cy);CHECK(poll()==lego::ctr::HostFrontendResult::Continue);
            CHECK(!input.touching); // Re-entry requires a fresh press after leaving.
            Mouse(id,SDL_EVENT_MOUSE_BUTTON_DOWN,cx,cy);CHECK(poll()==lego::ctr::HostFrontendResult::Continue);CHECK(input.touching);
            WindowEvent(id,SDL_EVENT_WINDOW_FOCUS_LOST);CHECK(poll()==lego::ctr::HostFrontendResult::Continue);
            CHECK(!input.touching && input.buttons==0);
            Key(id,SDL_SCANCODE_I,true);Key(id,SDL_SCANCODE_L,true);
            CHECK(poll()==lego::ctr::HostFrontendResult::Continue);CHECK(input.circle_x==154 && input.circle_y==154);
            Key(id,SDL_SCANCODE_K,true);Key(id,SDL_SCANCODE_J,true);
            CHECK(poll()==lego::ctr::HostFrontendResult::Continue);CHECK(input.circle_x==0 && input.circle_y==0);
            Mouse(id,SDL_EVENT_MOUSE_BUTTON_DOWN,cx,cy);CHECK(poll()==lego::ctr::HostFrontendResult::Continue);CHECK(input.touching);
            Key(id,SDL_SCANCODE_TAB,true);CHECK(poll()==lego::ctr::HostFrontendResult::Continue);
            CHECK(!input.touching && window.layout().bottom_destination.width>small.width);
            const auto large=window.layout().bottom_destination;
            Mouse(id,SDL_EVENT_MOUSE_BUTTON_DOWN,large.x+large.width/2.0f,large.y+large.height/2.0f);
            CHECK(poll()==lego::ctr::HostFrontendResult::Continue);
            CHECK(input.touching && input.touch_x==160 && input.touch_y==120);
            Mouse(id,SDL_EVENT_MOUSE_BUTTON_UP,large.x+large.width/2.0f,large.y+large.height/2.0f);
            WindowEvent(id,SDL_EVENT_WINDOW_FOCUS_LOST);CHECK(poll()==lego::ctr::HostFrontendResult::Continue);
            // Both edges can arrive before one guest display/HID deadline.
            // The next sample must see a press; the following one sees release.
            Mouse(id,SDL_EVENT_MOUSE_BUTTON_DOWN,large.x+large.width/2.0f,large.y+large.height/2.0f);
            Mouse(id,SDL_EVENT_MOUSE_BUTTON_UP,large.x+large.width/2.0f,large.y+large.height/2.0f);
            Key(id,SDL_SCANCODE_Z,true);Key(id,SDL_SCANCODE_Z,false);
            CHECK(poll()==lego::ctr::HostFrontendResult::Continue);
            CHECK(input.touching && input.buttons==1);
            CHECK(window.Pump(gpu,true,input)==lego::ctr::HostFrontendResult::Continue);
            CHECK(input.touching && input.touch_x==160 && input.touch_y==120 && input.buttons==1);
            CHECK(window.Pump(gpu,true,input)==lego::ctr::HostFrontendResult::Continue);
            CHECK(input.touching && input.buttons==1); // Presentation alone is not an input sample.
            window.InputSampled();
            CHECK(window.Pump(gpu,true,input)==lego::ctr::HostFrontendResult::Continue);
            CHECK(!input.touching && input.buttons==0);
            SDL_Window* native=SDL_GetWindowFromID(id);CHECK(native!=nullptr);
            int minimum_width=0,minimum_height=0;
            CHECK(SDL_GetWindowMinimumSize(native,&minimum_width,&minimum_height));
            CHECK(minimum_width==160 && minimum_height==120);
            const bool minimize_requested=SDL_MinimizeWindow(native);
            CHECK(poll()==lego::ctr::HostFrontendResult::Continue);
            if (minimize_requested && (SDL_GetWindowFlags(native)&SDL_WINDOW_MINIMIZED)!=0) {
                const auto count=window.present_count();
                CHECK(window.Present(Fixture()));CHECK(window.present_count()==count);
                const auto minimized_path=std::filesystem::path(screenshot.string()+".minimized.bmp");
                CHECK(!window.SaveScreenshot(minimized_path));CHECK(!std::filesystem::exists(minimized_path));
                CHECK(!input.touching && input.buttons==0);
                CHECK(SDL_RestoreWindow(native));
                CHECK(poll()==lego::ctr::HostFrontendResult::Continue);
                CHECK((SDL_GetWindowFlags(native)&SDL_WINDOW_MINIMIZED)==0);
                CHECK(window.Present(Fixture()));CHECK(window.present_count()>count);
                std::cout<<"minimize_restore=PASS\n";
            } else {
                // Headless backends or an X server without a window manager
                // may not support minimizing. Do not claim that proof there.
                std::cout<<"minimize_restore=unavailable_on_backend\n";
            }
            Key(id,SDL_SCANCODE_ESCAPE,true);CHECK(poll()==lego::ctr::HostFrontendResult::Quit);
            CHECK(!input.touching && input.buttons==0 && input.circle_x==0 && input.circle_y==0);
            CHECK(window.present_count()>=3);
            std::cout<<"desktop_smoke="<<(failures ? "FAIL" : "PASS")
                     <<" fixture=synthetic_no_game screenshot="<<screenshot.string()
                     <<" rendered_frames="<<window.present_count()
                     <<" bottom_rect="<<small.x<<','<<small.y<<','<<small.width<<','<<small.height<<'\n';
        }
        if (!owned_directory.empty()) std::filesystem::remove_all(owned_directory);
        return failures ? 1 : 0;
    } catch (const std::exception& ex) {
        std::cerr<<"desktop_smoke=ERROR "<<ex.what()<<'\n';return 2;
    }
}
