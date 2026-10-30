#include "services/hid_user_service.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace lego::ctr;
namespace {
int failures{};
#define CHECK(x) do { if (!(x)) { std::cerr<<"FAIL "<<__LINE__<<": " #x "\n"; ++failures; } } while (0)
std::uint32_t Word(const HidUserService& hid, std::size_t offset) {
    const auto b=hid.shared_memory()->bytes();
    return std::uint32_t(b[offset])|std::uint32_t(b[offset+1])<<8|
           std::uint32_t(b[offset+2])<<16|std::uint32_t(b[offset+3])<<24;
}
std::uint64_t Tick(const HidUserService& hid,std::size_t offset) {
    return Word(hid,offset)|(std::uint64_t(Word(hid,offset+4))<<32);
}
void PressReleaseAndGuestView() {
    Kernel k; GuestMemory memory; HidUserService hid;
    CHECK(memory.MapSharedServicePage(0x10000000,hid.shared_memory(),MemoryPermission::Read));
    CHECK(!memory.Write32(0x10000000,0xFFFFFFFF));
    HostInputState input{0x401,319,239,true,154,-154};
    CHECK(hid.PublishHostInput(k,input,123));
    CHECK(k.now_ns()==0 && hid.host_input_samples()==1);
    CHECK(Word(hid,0x10)==0 && Word(hid,0xB8)==0);
    CHECK(Tick(hid,0)==123 && Tick(hid,8)==0 && Tick(hid,0xA8)==123);
    CHECK(Word(hid,0x1C)==0x90000401);
    CHECK(Word(hid,0x28)==0x90000401 && Word(hid,0x2C)==0x90000401 && Word(hid,0x30)==0);
    CHECK(Word(hid,0x34)==0xFF66009A);
    CHECK(Word(hid,0xC8)==0x00EF013F && Word(hid,0xCC)==1);
    std::uint32_t observed{};
    CHECK(memory.Read32(0x100000C8,&observed) && observed==0x00EF013F);
    CHECK(hid.event(0)->signaled() && hid.event(1)->signaled());
    for (unsigned i=2;i<5;++i) CHECK(!hid.event(i)->signaled());
    input={2,319,239,false,0,0}; // stale host coordinates must not become a valid touch.
    CHECK(hid.PublishHostInput(k,input,456));
    CHECK(Word(hid,0x10)==1 && Word(hid,0xB8)==1);
    CHECK(Word(hid,0x38)==2 && Word(hid,0x3C)==2 && Word(hid,0x40)==0x90000401);
    CHECK(Word(hid,0xD0)==0 && Word(hid,0xD4)==0 && Tick(hid,0)==123);
    CHECK(Word(hid,0x28)==0x90000401); // prior ring entry is preserved.
}
void RingAndUntouchedFields() {
    Kernel k; HidUserService hid;
    constexpr std::array<std::uint32_t,6> untouched{0x14,0x18,0x20,0xC0,0x108,0xFFC};
    const std::array<std::uint8_t,4> marker{0x11,0x22,0x33,0x44};
    for (auto offset:untouched) CHECK(hid.shared_memory()->Write(offset,marker));
    for (unsigned i=1;i<=9;++i) {
        HostInputState input; input.buttons=i&1;
        CHECK(hid.PublishHostInput(k,input,1000+i));
    }
    CHECK(hid.host_input_samples()==9 && Word(hid,0x10)==0 && Word(hid,0xB8)==0);
    CHECK(Tick(hid,0)==1009 && Tick(hid,8)==1001);
    CHECK(Tick(hid,0xA8)==1009 && Tick(hid,0xB0)==1001);
    CHECK(Word(hid,0x28)==1 && Word(hid,0x2C)==1 && Word(hid,0x30)==0);
    CHECK(Word(hid,0x98)==0 && Word(hid,0xA0)==1);
    for (auto offset:untouched) CHECK(Word(hid,offset)==0x44332211);
}
void WakeAndLifetime() {
    Kernel k; HidUserService hid; Handle handle{};
    CHECK(k.handles().Create(&handle,hid.event(0))==0);
    auto waiting=k.current_thread();
    CHECK(k.WaitSynchronization1(handle,-1).blocked);
    CHECK(k.CloseHandle(handle)==0); // wait retains the event object.
    CHECK(hid.PublishHostInput(k,{},77));
    CHECK(waiting->status==ThreadStatus::Ready && waiting->pending_wake);
    CHECK(!hid.event(0)->signaled() && hid.event(1)->signaled());
    CHECK(k.now_ns()==0);
}
void GuardsAreAtomic() {
    const std::array<HostInputState,6> bad{{
        {0x1000,0,0,false,0,0},{0,320,0,true,0,0},{0,0,240,true,0,0},
        {0,0,0,false,155,0},{0,0,0,false,0,-155},{0x80000000,0,0,false,0,0}}};
    for (const auto& input:bad) {
        Kernel k; HidUserService hid;
        CHECK(!hid.PublishHostInput(k,input,1));
        CHECK(hid.host_input_samples()==0 && k.now_ns()==0);
        CHECK(std::all_of(hid.shared_memory()->bytes().begin(),hid.shared_memory()->bytes().end(),[](auto b){return !b;}));
        for (unsigned i=0;i<5;++i) CHECK(!hid.event(i)->signaled());
    }
    Kernel k; HidUserService hid;
    CHECK(hid.PublishHostInput(k,{},10));
    const auto before=std::array<std::uint8_t,0x1000>([&]{
        std::array<std::uint8_t,0x1000> b{};std::copy(hid.shared_memory()->bytes().begin(),hid.shared_memory()->bytes().end(),b.begin());return b;}());
    const auto epoch=hid.shared_memory()->Epoch(0);
    CHECK(!hid.PublishHostInput(k,{},9));
    CHECK(!hid.PublishHostInput(k,{},std::numeric_limits<std::uint64_t>::max()));
    CHECK(hid.host_input_samples()==1 && hid.shared_memory()->Epoch(0)==epoch);
    CHECK(std::equal(before.begin(),before.end(),hid.shared_memory()->bytes().begin()));
}
void CircleDirections() {
    Kernel k; HidUserService hid;
    const struct { std::int16_t x,y;std::uint32_t directions; } cases[]{
        {40,0,0},{41,0,0x10000000},{0,41,0x40000000},{-41,0,0x20000000},
        {0,-41,0x80000000},{80,80,0x50000000},{-80,-80,0xA0000000},
        {40,23,0x10000000},{40,24,0x50000000}};
    for (const auto& c:cases) {
        HostInputState input;input.circle_x=c.x;input.circle_y=c.y;
        CHECK(hid.PublishHostInput(k,input,1));
        CHECK(Word(hid,0x1C)==c.directions);
    }
}
} // namespace
int main() {
    PressReleaseAndGuestView();RingAndUntouchedFields();WakeAndLifetime();GuardsAreAtomic();CircleDirections();
    if (failures) return EXIT_FAILURE;
    std::cout<<"PASS: explicit HID pad/touch rings, guest-visible bytes, deltas, timestamps, real event wake and atomic guards\n";
}
