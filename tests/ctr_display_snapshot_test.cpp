#include "frontend/display_snapshot.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

#ifdef LEGO_TEST_WRAP_SNAPSHOT_STDIO
namespace { unsigned writes{},flushes{},closes{},fail_write{},fail_flush{},fail_close{}; }
extern "C" {
std::size_t __real_fwrite(const void*,std::size_t,std::size_t,std::FILE*);
int __real_fflush(std::FILE*);
int __real_fclose(std::FILE*);
std::size_t __wrap_fwrite(const void* p,std::size_t size,std::size_t count,std::FILE* f) {
    if (++writes==fail_write) return 0;
    return __real_fwrite(p,size,count,f);
}
int __wrap_fflush(std::FILE* f) {
    const auto result=__real_fflush(f);
    return ++flushes==fail_flush ? EOF : result;
}
int __wrap_fclose(std::FILE* f) {
    const auto result=__real_fclose(f);
    return ++closes==fail_close ? EOF : result;
}
}
#endif

namespace {
using namespace lego::frontend;
namespace fs=std::filesystem;
int failures{};
#define CHECK(x) do {if (!(x)) {std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}} while(0)
struct TemporaryRoot {
    fs::path path;
    TemporaryRoot() {
        const auto seed=std::chrono::steady_clock::now().time_since_epoch().count();
        for (unsigned i=0;i<100;++i) {
            auto candidate=fs::temp_directory_path()/("lego-snapshot-test-"+std::to_string(seed)+"-"+std::to_string(i));
            if (fs::create_directory(candidate)) {path=std::move(candidate);return;}
        }
        throw std::runtime_error("cannot create test-owned temporary directory");
    }
    ~TemporaryRoot() {std::error_code ec;fs::remove_all(path,ec);}
};
ScreenImage Image(int width,unsigned salt) {
    ScreenImage image{width,240,std::vector<std::uint8_t>(static_cast<std::size_t>(width)*240*4)};
    for (int y=0;y<240;++y) for (int x=0;x<width;++x) {
        const auto p=(static_cast<std::size_t>(y)*width+x)*4;
        image.rgba[p]=static_cast<std::uint8_t>(x+salt);
        image.rgba[p+1]=static_cast<std::uint8_t>(y+salt);
        image.rgba[p+2]=static_cast<std::uint8_t>((x^y)+salt);
        image.rgba[p+3]=255;
    }
    return image;
}
std::vector<unsigned char> Read(const fs::path& path) {
    std::ifstream stream(path,std::ios::binary);
    return {std::istreambuf_iterator<char>(stream),{}};
}
void CheckPixels(const fs::path& path,int width,unsigned salt) {
    const auto bytes=Read(path);
    const auto header="P6\n"+std::to_string(width)+" 240\n255\n";
    CHECK(bytes.size()==header.size()+static_cast<std::size_t>(width)*240*3);
    if (bytes.size()!=header.size()+static_cast<std::size_t>(width)*240*3) return;
    CHECK(std::string(bytes.begin(),bytes.begin()+header.size())==header);
    for (int y=0;y<240;++y) for (int x=0;x<width;++x) {
        const auto p=header.size()+(static_cast<std::size_t>(y)*width+x)*3;
        CHECK(bytes[p]==static_cast<unsigned char>(x+salt));
        CHECK(bytes[p+1]==static_cast<unsigned char>(y+salt));
        CHECK(bytes[p+2]==static_cast<unsigned char>((x^y)+salt));
    }
}
void SuccessAndRefusal() {
    TemporaryRoot root;const DualScreenFrame frame{Image(400,3),Image(320,17)};const auto original=frame;
    std::string error="stale";const auto directory=root.path/"capture";
    CHECK(WriteDisplaySnapshot(frame,directory,error) && error.empty());
    CheckPixels(directory/"top.ppm",400,3);CheckPixels(directory/"bottom.ppm",320,17);
    CHECK(frame.top.rgba==original.top.rgba && frame.bottom.rgba==original.bottom.rgba);
    const auto top=Read(directory/"top.ppm"),bottom=Read(directory/"bottom.ppm");
    CHECK(!WriteDisplaySnapshot(frame,directory,error) && !error.empty());
    CHECK(Read(directory/"top.ppm")==top && Read(directory/"bottom.ppm")==bottom);
    fs::create_directory(root.path/"empty-existing");
    CHECK(!WriteDisplaySnapshot(frame,root.path/"empty-existing",error));
    CHECK(fs::is_empty(root.path/"empty-existing"));
    std::ofstream(root.path/"existing-file")<<"do not overwrite";
    const auto marker=Read(root.path/"existing-file");
    CHECK(!WriteDisplaySnapshot(frame,root.path/"existing-file",error));
    CHECK(Read(root.path/"existing-file")==marker);
    CHECK(!WriteDisplaySnapshot(frame,root.path/"missing-parent"/"capture",error));
    CHECK(!fs::exists(root.path/"missing-parent"));
    CHECK(!WriteDisplaySnapshot(frame,{},error));
#ifndef _WIN32
    fs::create_directory_symlink(directory,root.path/"alias");
    CHECK(!WriteDisplaySnapshot(frame,root.path/"alias",error));
    fs::create_symlink(root.path/"missing",root.path/"dangling");
    CHECK(!WriteDisplaySnapshot(frame,root.path/"dangling",error));
    CHECK(!fs::exists(root.path/"missing"));
#endif
    // All-black scanout is valid diagnostic evidence, never replaced with art.
    auto black=frame;
    for (auto* image:{&black.top,&black.bottom})
        for (std::size_t i=0;i<image->rgba.size();i+=4)
            image->rgba[i]=image->rgba[i+1]=image->rgba[i+2]=0;
    CHECK(WriteDisplaySnapshot(black,root.path/"black",error));
    const auto black_top=Read(root.path/"black/top.ppm");
    for (std::size_t i=15;i<black_top.size();++i) CHECK(black_top[i]==0);
}
void InvalidInputs() {
    TemporaryRoot root;const DualScreenFrame original{Image(400,3),Image(320,17)};
    for (int kind=0;kind<8;++kind) {
        auto frame=original;
        switch(kind) {
        case 0:frame.top.width=0;break;
        case 1:frame.top.width=320;break;
        case 2:frame.bottom.height=-1;break;
        case 3:frame.bottom.height=241;break;
        case 4:frame.top.rgba.pop_back();break;
        case 5:frame.bottom.rgba.push_back(0);break;
        case 6:frame.top.rgba[3]=0;break;
        case 7:frame.bottom.rgba.back()=254;break;
        }
        std::string error;const auto path=root.path/std::to_string(kind);
        CHECK(!WriteDisplaySnapshot(frame,path,error) && !error.empty());
        CHECK(!fs::exists(path));
    }
}
#ifdef LEGO_TEST_WRAP_SNAPSHOT_STDIO
void IoFailures() {
    TemporaryRoot root;const DualScreenFrame frame{Image(400,3),Image(320,17)};
    // Fail the first header, a row, the second image, either flush or close.
    for (int kind=0;kind<7;++kind) {
        writes=flushes=closes=fail_write=fail_flush=fail_close=0;
        if(kind==0)fail_write=1;
        if(kind==1)fail_write=2;
        if(kind==2)fail_write=242;
        if(kind==3)fail_flush=1;
        if(kind==4)fail_flush=2;
        if(kind==5)fail_close=1;
        if(kind==6)fail_close=2;
        const auto path=root.path/std::to_string(kind);std::string error;
        CHECK(!WriteDisplaySnapshot(frame,path,error) && !error.empty());
        CHECK(fs::exists(path/"top.ppm"));
        CHECK(closes==(kind==2 || kind==4 || kind==6 ? 2U : 1U));
        fail_write=fail_flush=fail_close=0;
        const auto old=Read(path/"top.ppm");
        CHECK(!WriteDisplaySnapshot(frame,path,error));
        CHECK(Read(path/"top.ppm")==old);
    }
}
#endif
}
int main() {
    SuccessAndRefusal();InvalidInputs();
#ifdef LEGO_TEST_WRAP_SNAPSHOT_STDIO
    IoFailures();
#endif
    if(failures)return 1;
    std::cout<<"PASS: exact raw LCD samples, RGB ordering, immutable input, invalid frames, exclusive outputs and retained I/O failures\n";
}
