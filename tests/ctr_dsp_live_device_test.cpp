#include "dsp_live_fixture.h"
#include "services/dsp_ram.h"
#include <cstdlib>
#include <iostream>
#include <new>
#include <limits>
static bool fail_next=false;
void* operator new(std::size_t n){if(fail_next){fail_next=false;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept{std::free(p);}void operator delete(void* p,std::size_t) noexcept{std::free(p);}
namespace {
using namespace lego::ctr;using namespace dsp_live_fixture;
int failures=0;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}}while(0)
std::uint16_t Word(std::shared_ptr<DeviceMemory> m,unsigned at){std::array<std::uint8_t,2>b{};CHECK(m->Read(at,b));return b[0]|(b[1]<<8);}
void Put(std::shared_ptr<DeviceMemory> m,unsigned at,std::uint16_t w){const std::array<std::uint8_t,2>b{static_cast<std::uint8_t>(w),static_cast<std::uint8_t>(w>>8)};CHECK(m->Write(at,b));}
void MemoryAndLifetime(){
    auto p=Boot(Image());auto d=Device(p);GuestMemory m;auto ram=p->data_backing();
    CHECK(d->Attach(m));CHECK(!d->Attach(m));CHECK(!m.IsMapped(0x1FF00000));CHECK(m.IsMapped(DspLiveDevice::DataAddress,0x40000));
    CHECK(!m.IsReadable(DspLiveDevice::DataAddress+0x200,2));CHECK(m.IsWritable(DspLiveDevice::DataAddress+0x200,2));
    std::uint32_t unchanged=0x12345678;CHECK(!m.Read32(DspLiveDevice::DataAddress+0x200,&unchanged)&&unchanged==0x12345678);
    CHECK(!m.Write32(DspLiveDevice::DataAddress+0x3FFFE,1));CHECK(m.LoadBytes(DspLiveDevice::DataAddress+0x200,{}));
    const std::array<std::uint8_t,1> b{7};CHECK(!m.LoadBytes(DspLiveDevice::DataAddress+0x200,b));CHECK(!m.ZeroBytes(DspLiveDevice::DataAddress+0x200,1));
    CHECK(!m.PrepareDeviceWrite(DspLiveDevice::DataAddress+0x200,2));CHECK(!m.CommitDeviceWrite(DspLiveDevice::DataAddress+0x200,b));
    CHECK(m.MapDeviceMemory(0x20000000,ram,MemoryPermission::Read|MemoryPermission::Write));
    CHECK(m.SpansAlias(DspLiveDevice::DataAddress+0x400,2,0x20000400,2));CHECK(!m.SpansAlias(DspLiveDevice::DataAddress+0x400,2,0x20000408,2));
    std::uint64_t value=0,token=0;std::uint32_t fault=0;
    CHECK(m.LoadExclusive(DspLiveDevice::DataAddress+0x400,2,&value,&token,&fault));
    CHECK(m.Write16(0x20000400,0xABCD));CHECK(Word(ram,0x400)==0xABCD);
    CHECK(m.StoreExclusive(DspLiveDevice::DataAddress+0x400,2,1,token,&fault)==a32::ExclusiveStoreResult::ReservationLost);
    CHECK(m.Write16(DspLiveDevice::DataAddress+0x200,0x4567));CHECK(p->provenance()[0x40200]==4);
    CHECK(m.LoadExclusive(DspLiveDevice::DataAddress+0x200,2,&value,&token,&fault));
    const auto before=p->summary().completed_steps;CHECK(d->RunScheduled(*d->next_deadline_ns()));
    CHECK(d->slices()==1&&p->summary().completed_steps==before+DspLiveDevice::SliceSteps);
    CHECK(Word(ram,0x200)==0x7B&&p->provenance()[0x40200]==3);
    CHECK(m.StoreExclusive(DspLiveDevice::DataAddress+0x200,2,1,token,&fault)==a32::ExclusiveStoreResult::ReservationLost);
    CHECK(m.UnmapDeviceMemory(DspLiveDevice::DataAddress,*ram));CHECK(!m.UnmapDeviceMemory(DspLiveDevice::DataAddress,*ram));
    CHECK(!m.IsMapped(DspLiveDevice::DataAddress)&&m.IsMapped(0x20000000));
    d.reset();p.reset();ram.reset();std::uint16_t word=0;CHECK(m.Read16(0x20000200,&word)&&word==0x7B);
}
void PipeTransactions(){
    {auto p=Boot(Image());auto d=Device(p);auto ram=p->data_backing();
     for(unsigned s=0;s<16;++s){DspPipeDescriptor desc;CHECK(d->InspectPipe(s,desc)==DspPipeResult::Complete&&desc.slot==s&&desc.used==0);}
     const std::array<std::uint8_t,4>b{1,2,3,4};CHECK(d->WritePipe(2,b)==DspPipeResult::Complete);
     CHECK(Word(ram,0x84+5*10+6)==4&&Word(ram,0x84+5*10+4)==0);std::array<std::uint8_t,4>copy{};
     CHECK(ram->Read(0x450,copy)&&copy==b);const auto state=std::vector(p->memory().begin(),p->memory().end());
     CHECK(d->WritePipe(2,b)==DspPipeResult::WouldBlock);CHECK(std::equal(state.begin(),state.end(),p->memory().begin()));}
    {auto p=Boot(Image());auto d=Device(p);auto ram=p->data_backing();
     const auto at=0x84+3*10;Put(ram,at+4,4);Put(ram,at+6,6);
     const std::array<std::uint8_t,6>b{10,20,30,40,50,60};CHECK(d->WritePipe(1,b)==DspPipeResult::Complete);
     CHECK(Word(ram,at+6)==0x8004&&Word(ram,at+4)==4);std::array<std::uint8_t,8>copy{};CHECK(ram->Read(0x430,copy));
     CHECK(copy[6]==10&&copy[7]==20&&copy[0]==30&&copy[3]==60);DspPipeDescriptor desc;
     CHECK(d->InspectPipe(3,desc)==DspPipeResult::Complete&&desc.used==8);}
    {auto p=Boot(Image());auto d=Device(p);auto ram=p->data_backing();const auto at=0x84+6*10;
     Put(ram,at+4,6);Put(ram,at+6,0x8002);std::array<std::uint8_t,8>payload{};CHECK(ram->Read(0x460,payload));
     std::array<std::uint8_t,4>output{};CHECK(d->ReadPipe(3,output)==DspPipeResult::Complete);
     CHECK((output==std::array<std::uint8_t,4>{payload[6],payload[7],payload[0],payload[1]}));
     CHECK(Word(ram,at+4)==0x8002&&Word(ram,at+6)==0x8002);}
    {auto p=Boot(Image());auto d=Device(p);auto ram=p->data_backing();std::array<std::uint8_t,4>output{9,9,9,9};
     CHECK(d->ReadPipe(1,output)==DspPipeResult::WouldBlock&&output[0]==9);
     CHECK(d->ReadPipe(8,output)==DspPipeResult::Invalid);Put(ram,0x84+3*10+2,0);
     const auto before=std::vector(p->memory().begin(),p->memory().end());CHECK(d->WritePipe(1,output)==DspPipeResult::Invalid);
     CHECK(std::equal(before.begin(),before.end(),p->memory().begin()));}
}
void InterruptFromFirmware(){
    auto p=Boot(Image(false,true));auto d=Device(p);GuestMemory m;CHECK(d->Attach(m));
    CHECK(d->RunScheduled(*d->next_deadline_ns()));
    CHECK(p->live_notifications()==1&&p->TakeLiveInterrupts()==1&&p->TakeLiveInterrupts()==0);
}
void PipeInterruptPairing(){
    for(bool semaphore_first:{false,true})for(unsigned slot:{4U,5U,16U,0U}) {
        auto image=Image();auto program=Program();program.resize(20);
        auto emit=[&](std::initializer_list<std::uint16_t> words){for(auto w:words){program.push_back(w&255);program.push_back(w>>8);}};
        const auto semaphore=[&]{emit({0xD4B8,0x0300,0xD4BC,0x80CC});};
        const auto mailbox=[&]{emit({static_cast<std::uint16_t>(0x2100|slot),0xD4BC,0x80C8});};
        if(semaphore_first){semaphore();mailbox();}else{mailbox();semaphore();}emit({0x57F0});
        image.program=Dsp1Bank();CHECK(image.program.Stage(0,program));
        const std::array<std::uint8_t,2> bit{0,0x80};CHECK(image.data.Stage(0x600,bit));
        auto p=Boot(image);auto d=Device(p);GuestMemory m;CHECK(d->Attach(m));
        const bool ran=d->RunScheduled(*d->next_deadline_ns());
        if(slot==4){CHECK(ran&&p->live_notifications()==1&&p->TakeLiveInterrupts()==(1U<<4));}
        else if(slot==5){CHECK(ran&&p->live_notifications()==0&&p->TakeLiveInterrupts()==0);}
        else CHECK(!ran&&p->summary().state==DspProbeState::Fault);
    }
}
void InvalidAndFaults(){
    const char* error=nullptr;CHECK(!DspLiveDevice::Prepare({},0,error)&&error);
    for(unsigned bad:{0U,1U,2U,3U}){auto p=Boot(Image());auto ram=p->data_backing();
        if(bad==0)Put(ram,0x84+2,0);if(bad==1)Put(ram,0x84+4,0x7FFF);
        if(bad==2)Put(ram,0x84,0x42);if(bad==3)Put(ram,0x84+10,0x200);
        CHECK(!DspLiveDevice::Prepare(p,0,error)&&error);}
    {auto p=Boot(Image());auto d=Device(p);GuestMemory m;
     fail_next=true;bool failed=false;try{d->Attach(m);}catch(const std::bad_alloc&){failed=true;}fail_next=false;
     CHECK(failed&&!m.IsMapped(DspLiveDevice::DataAddress)&&!d->attached());CHECK(d->Attach(m));}
    {auto p=Boot(Image());fail_next=true;auto d=DspLiveDevice::Prepare(p,0,error);fail_next=false;CHECK(!d&&error);}
    for(bool notify:{false}){auto p=Boot(Image(!notify,notify));auto d=Device(p);GuestMemory m;CHECK(d->Attach(m));
        CHECK(!d->RunScheduled(*d->next_deadline_ns()));CHECK(d->error()&&p->summary().state==DspProbeState::Fault);
        const auto before=p->summary();CHECK(!d->RunScheduled(*d->next_deadline_ns())&&p->summary()==before);
        CHECK(!m.IsReadable(DspLiveDevice::DataAddress+0x84,1)&&!m.IsWritable(DspLiveDevice::DataAddress+0x84,1));}
}
}
int main(){try{MemoryAndLifetime();PipeTransactions();InterruptFromFirmware();PipeInterruptPairing();InvalidAndFaults();}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}
 if(failures)return 1;std::cout<<"PASS: persistent DSP storage, provenance, alias reservations, bounded live steps and pipe wrap/guards\n";}
