#include "frontend/display_frame.h"
#include "frontend/screen_layout.h"
#include "services/gsp_gpu_service.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace {
using namespace lego::frontend;
int failures=0;
#define CHECK(x) do {if (!(x)) {std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}} while(0)

std::array<std::uint8_t,4> Pixel(const ScreenImage& image,int x,int y) {
    const auto p=(static_cast<std::size_t>(y)*image.width+x)*4;
    return {image.rgba[p],image.rgba[p+1],image.rgba[p+2],image.rgba[p+3]};
}
void LayoutAndTouch() {
    const auto value=ComputeLayout(1280,720,false);CHECK(value.has_value());
    if (!value) return;
    const auto& layout=*value;
    CHECK((layout.top_destination==Rect{40,0,1200,720}));
    CHECK((layout.bottom_destination==Rect{908,438,358,268}));
    CHECK((MapTouch(layout,908,438)==TouchPosition{0,0}));
    CHECK((MapTouch(layout,1087,572)==TouchPosition{160,120}));
    CHECK((MapTouch(layout,1265.99,705.99)==TouchPosition{319,239}));
    CHECK(!MapTouch(layout,907.99,438));CHECK(!MapTouch(layout,908,437.99));
    CHECK(!MapTouch(layout,1266,572));CHECK(!MapTouch(layout,1087,706));
    CHECK(!MapTouch(layout,std::numeric_limits<double>::quiet_NaN(),500));
    CHECK(!MapTouch(layout,1000,std::numeric_limits<double>::infinity()));
    const auto expanded=ComputeLayout(1280,720,true);CHECK(expanded.has_value());
    if (expanded) {
        CHECK((expanded->bottom_destination==Rect{179,14,922,692}));
        CHECK((MapTouch(*expanded,640,360)==TouchPosition{160,120}));
    }
    for (auto dimensions:std::array<std::array<int,2>,6>{{{160,120},{720,1280},{3440,1440},{3840,2160},{4,4},{16384,16384}}}) {
        for (bool large:{false,true}) {
            const auto resized=ComputeLayout(dimensions[0],dimensions[1],large);
            CHECK(resized.has_value());if (!resized)continue;
            const auto& r=resized->bottom_destination;
            CHECK(r.x>=0 && r.y>=0 && r.width>0 && r.height>0);
            CHECK(r.x+r.width<=dimensions[0] && r.y+r.height<=dimensions[1]);
            CHECK((MapTouch(*resized,r.x,r.y)==TouchPosition{0,0}));
            CHECK(!MapTouch(*resized,r.x+r.width,r.y));
        }
    }
    const auto cropped=ComputeLayout(1280,720,false,{20,30,100,80});CHECK(cropped.has_value());
    if (cropped) {
        const auto& r=cropped->bottom_destination;
        CHECK((MapTouch(*cropped,r.x,r.y)==TouchPosition{20,30}));
        CHECK((MapTouch(*cropped,r.x+r.width/2.0,r.y+r.height/2.0)==TouchPosition{70,70}));
        CHECK((MapTouch(*cropped,r.x+r.width-.001,r.y+r.height-.001)==TouchPosition{119,109}));
    }
    CHECK(!ComputeLayout(0,720,false));CHECK(!ComputeLayout(1280,-1,false));
    CHECK(!ComputeLayout(1280,720,false,{0,0,321,240}));
    CHECK(!ComputeLayout(1280,720,false,{300,0,21,240}));
    CHECK(!ComputeLayout(1280,720,false,{0,-1,320,240}));
    CHECK(!ComputeLayout(1280,720,false,{0,0,0,1}));
}

void ActualBytesAndRotation() {
    std::vector<std::uint8_t> vram(lego::ctr::kGpuVramBytes,0);
    // Four independently positioned BGR markers: raw first/last columns of
    // first/last rows. Landscape orientation must put them in distinct corners.
    const std::uint32_t offset=1024,stride=736;
    const std::array<std::array<std::uint8_t,3>,4> colors{{{3,2,1},{6,5,4},{9,8,7},{12,11,10}}};
    const std::array<std::size_t,4> locations{offset+717,offset+399*stride+717,offset,offset+399*stride};
    for (unsigned i=0;i<4;++i)for(unsigned c=0;c<3;++c)vram[locations[i]+c]=colors[i][c];
    ScanoutDescriptor descriptor{(400U<<16)|240U,1,stride,1,{0x18000000U,0x18000400U},0};
    ScreenImage image;std::string error;
    CHECK(DecodeRgb8Scanout(vram,descriptor,400,image,error));CHECK(error.empty());
    CHECK(image.width==400 && image.height==240 && image.rgba.size()==400*240*4);
    CHECK((Pixel(image,0,0)==std::array<std::uint8_t,4>{1,2,3,255}));
    CHECK((Pixel(image,399,0)==std::array<std::uint8_t,4>{4,5,6,255}));
    CHECK((Pixel(image,0,239)==std::array<std::uint8_t,4>{7,8,9,255}));
    CHECK((Pixel(image,399,239)==std::array<std::uint8_t,4>{10,11,12,255}));
    CHECK((Pixel(image,200,120)==std::array<std::uint8_t,4>{0,0,0,255}));
    descriptor.active_buffer=0;
    CHECK(DecodeRgb8Scanout(vram,descriptor,400,image,error));
    CHECK((Pixel(image,0,0)==std::array<std::uint8_t,4>{0,0,0,255}));
    descriptor.active_buffer=1;
    // A subsequent device write must appear; presentation never caches game pixels.
    vram[locations[0]+2]=99;
    CHECK(DecodeRgb8Scanout(vram,descriptor,400,image,error));
    CHECK((Pixel(image,0,0)==std::array<std::uint8_t,4>{99,2,3,255}));
    DualScreenFrame frame{image,{}};CHECK(frame.HasNonBlackPixels());
    descriptor.lcd_color_fill=0x01000000U;
    CHECK(DecodeRgb8Scanout({},descriptor,400,image,error));
    CHECK((Pixel(image,0,0)==std::array<std::uint8_t,4>{0,0,0,255}));
    frame.top=image;CHECK(!frame.HasNonBlackPixels());
}

void BoundsAndFailureAtomicity() {
    std::vector<std::uint8_t> vram(320*720);
    const ScanoutDescriptor good{(320U<<16)|240U,1,720,0,{0x18000000U,0},0};
    ScreenImage sentinel{1,1,{77,88,99,255}};std::string error;
    auto image=sentinel;
    CHECK(DecodeRgb8Scanout(vram,good,320,image,error));
    CHECK(image.width==320 && image.height==240);
    std::vector<ScanoutDescriptor> invalid;
    auto d=good;d.active_buffer=2;invalid.push_back(d);
    for (auto format:{0U,2U,3U,4U,5U,6U,7U}) {d=good;d.format_word=format;invalid.push_back(d);}
    for (auto address:{0U,0x17FFFFFFU,0x18000001U,0x20000000U,0xFFFFFFFFU}) {
        d=good;d.addresses[0]=address;invalid.push_back(d);
    }
    for (auto stride:{0U,719U,721U,0xFFFFFFFFU}) {d=good;d.stride=stride;invalid.push_back(d);}
    d=good;d.size_word=(321U<<16)|240U;invalid.push_back(d);
    d=good;d.size_word=(320U<<16)|239U;invalid.push_back(d);
    d=good;d.lcd_color_fill=0x010000FFU;invalid.push_back(d);
    for (const auto& bad:invalid) {
        image=sentinel;CHECK(!DecodeRgb8Scanout(vram,bad,320,image,error));CHECK(!error.empty());
        CHECK(image.width==sentinel.width && image.height==sentinel.height && image.rgba==sentinel.rgba);
    }
    image=sentinel;CHECK(!DecodeRgb8Scanout({},good,320,image,error));CHECK(image.rgba==sentinel.rgba);
    image=sentinel;CHECK(!DecodeRgb8Scanout(vram,good,321,image,error));CHECK(image.rgba==sentinel.rgba);
}

void LiveGspRead() {
    lego::ctr::GspGpuService gpu;
    DualScreenFrame image;std::string error;
    CHECK(!ReadDisplayFrame(gpu,image,error));
    auto bank=lego::ctr::GpuVramBank::ReferenceZero();CHECK(gpu.ConfigureVram(bank));
    CHECK(ReadDisplayFrame(gpu,image,error));CHECK(!image.HasNonBlackPixels());
    // Default top buffer is 0x181E6000. Its first byte is the bottom-left blue.
    const std::array<std::uint8_t,3> bgr{12,34,56};CHECK(bank->Write(0x1E6000,bgr));
    CHECK(ReadDisplayFrame(gpu,image,error));CHECK(image.HasNonBlackPixels());
    CHECK((Pixel(image.top,0,239)==std::array<std::uint8_t,4>{56,34,12,255}));
    CHECK((Pixel(image.bottom,0,239)==std::array<std::uint8_t,4>{0,0,0,255}));
}
}

int main() {
    LayoutAndTouch();ActualBytesAndRotation();BoundsAndFailureAtomicity();LiveGspRead();
    std::cout<<"frontend display/layout checks: "<<(failures ? "FAIL" : "PASS")<<'\n';
    return failures ? 1 : 0;
}
