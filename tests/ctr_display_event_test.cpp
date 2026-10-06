#include "services/gsp_gpu_service.h"
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace lego::ctr;
namespace {
int failures=0;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}}while(0)
struct Fixture {
 Kernel k;GuestMemory m;IpcRouter ipc;
 std::shared_ptr<GspGpuService> endpoint=std::make_shared<GspGpuService>();
 std::vector<Handle> sessions,events;
 Fixture(){CHECK(m.EnsureTlsMappings(k));CHECK(ipc.RegisterService("gsp::Gpu",endpoint)==0);}
 void Add(bool registered=true){
  Handle s{},ev{};CHECK(ipc.ConnectToService(k,"gsp::Gpu",&s)==0);CHECK(k.CreateEvent(&ev,0)==0);
  sessions.push_back(s);events.push_back(ev);
  if(registered){IpcCommandBuffer q{0x00130042,0,0,ev};const auto cb=k.current_thread()->tls_address+kIpcCommandBufferOffset;
   for(unsigned i=0;i<q.size();++i)CHECK(m.Write32(cb+4*i,q[i]));
   const auto result=ipc.SendSyncRequest(k,m,s);CHECK(result&&*result==0);}
 }
 std::shared_ptr<EventObject> Event(unsigned i=0){return std::dynamic_pointer_cast<EventObject>(k.handles().Get(events[i]));}
 auto Bytes(){const auto b=endpoint->shared_memory()->bytes();return std::vector<std::uint8_t>(b.begin(),b.end());}
 void Byte(unsigned p,std::uint8_t v){CHECK(endpoint->shared_memory()->Write(p,{&v,1}));}
 void Word(unsigned p,std::uint32_t v){std::array<std::uint8_t,4>b{};for(unsigned i=0;i<4;++i)b[i]=v>>(8*i);CHECK(endpoint->shared_memory()->Write(p,b));}
 std::uint32_t Word(unsigned p){const auto b=Bytes();return std::uint32_t(b[p])|(std::uint32_t(b[p+1])<<8)|(std::uint32_t(b[p+2])<<16)|(std::uint32_t(b[p+3])<<24);}
 void Deliver(){DisplayPeriodPlan p;const char* err=nullptr;CHECK(endpoint->PrepareDisplayPeriod(p,err));CHECK(!err);CHECK(endpoint->CommitDisplayPeriod(k,p));}
};
void RegisteredBroadcastAndLatching(){
 Fixture f;for(unsigned i=0;i<4;++i)f.Add(i!=2);
 // No AcquireRight: display interrupts still reach all registered slots.
 f.Byte(0,51);f.Byte(0x200,0x81);f.Byte(0x201,0xA1);
 const std::array<std::uint32_t,7> info{1,0x1F346500,0x1F38CA00,720,0x341,1,0xBAD};
 for(unsigned i=0;i<7;++i)f.Word(0x220+4*i,info[i]);
 f.Byte(0x241,1);const std::array<std::uint32_t,7> bottom{0,0x14000010,0,12,1,0,0};
 for(unsigned i=0;i<7;++i)f.Word(0x244+4*i,bottom[i]);
 const auto handles=f.k.handles().OpenHandleCount();const auto time=f.k.now_ns();
 const auto queue_epoch=f.endpoint->shared_memory()->Epoch(0x800);
 f.Deliver();auto b=f.Bytes();CHECK(b[0]==51&&b[1]==2&&b[12+51]==2&&b[12]==3);
 CHECK(b[0x200]==0x81&&b[0x201]==0xA0&&b[0x241]==0);
 for(unsigned i:{0U,1U,3U})CHECK(b[0x40*i+1]==2&&f.Event(i)->signaled());
 CHECK(b[0x81]==0&&!f.Event(2)->signaled());
 CHECK(f.endpoint->register_word(0x40046C)==0x18346500U); // top left2
 CHECK(f.endpoint->register_word(0x400498)==0x1838CA00U); // top right2
 CHECK(f.endpoint->register_word(0x400470)==0x341U&&f.endpoint->register_word(0x400478)==1U);
 CHECK(f.endpoint->register_word(0x400568)==0x20000010U); // bottom left1
 CHECK(f.endpoint->register_word(0x400590)==12U);
 CHECK(f.k.now_ns()==time&&f.k.handles().OpenHandleCount()==handles);
 CHECK(f.endpoint->shared_memory()->Epoch(0x800)==queue_epoch);
 // Cleared dirty bit: changes to stored info alone do not latch next period.
 f.Word(0x224,0x1F000100);f.Deliver();CHECK(f.endpoint->register_word(0x40046C)==0x18346500U);
 CHECK(f.Bytes()[1]==4);
}
void IgnoreThresholdAndWrap(){
 for(unsigned count:{0U,31U,32U,51U,52U}){
  Fixture f;f.Add();f.Byte(1,count);f.Word(4,0xFFFFFFFFU);f.Word(8,0xFFFFFFFFU);
  f.Deliver();const auto b=f.Bytes();
  const auto added=count<32?std::min(2U,32-count):0U;
  CHECK(b[1]==count+added&&b[2]==0);
  CHECK(f.Word(4)==(count>=32?0U:0xFFFFFFFFU));
  CHECK(f.Word(8)==(count>=31?0U:0xFFFFFFFFU));
  CHECK(f.Event()->signaled()==(added!=0));
 }
 Fixture f;f.Add();f.Byte(3,0x81);f.Word(4,17);f.Word(8,19);auto before=f.Bytes();
 f.Deliver();CHECK(f.Bytes()==before&&!f.Event()->signaled());
}
void AtomicAndStalePlans(){
 Fixture f;f.Add();f.Add();const auto before=f.Bytes();DisplayPeriodPlan p;const char* error{};
 CHECK(f.endpoint->PrepareDisplayPeriod(p,error));f.Byte(0x41,53);const auto bad=f.Bytes();
 CHECK(!f.endpoint->CommitDisplayPeriod(f.k,p));CHECK(f.Bytes()==bad&&!f.Event()->signaled());
 DisplayPeriodPlan untouched;untouched.count=7;
 CHECK(!f.endpoint->PrepareDisplayPeriod(untouched,error));CHECK(error&&untouched.count==7);
 f.Byte(0x41,0);f.Byte(0x200+0x80+1,1);f.Word(0x284+4,0xDEAD0000);const auto invalidfb=f.Bytes();
 CHECK(!f.endpoint->PrepareDisplayPeriod(p,error));CHECK(f.Bytes()==invalidfb&&!f.Event()->signaled());
 f.Byte(0x281,0);f.Byte(2,1);CHECK(!f.endpoint->PrepareDisplayPeriod(p,error));f.Byte(2,0);
 CHECK(f.endpoint->PrepareDisplayPeriod(p,error));CHECK(f.endpoint->CommitDisplayPeriod(f.k,p));
 const auto after=f.Bytes();CHECK(!f.endpoint->CommitDisplayPeriod(f.k,p));CHECK(f.Bytes()==after);
 Fixture other;CHECK(!other.endpoint->CommitDisplayPeriod(other.k,p));
 // A retired session receives no further display IRQs; service retains no identity.
 f.k.CloseHandle(f.sessions[1]);const auto count=f.Bytes()[0x41];f.Deliver();CHECK(f.Bytes()[0x41]==count);
 (void)before;
}
void RealWaitAndRetainedEvent(){
 Fixture f;f.Add();auto event=f.Event();const auto wait=f.k.WaitSynchronization1(f.events[0],-1);CHECK(wait.blocked);
 f.k.CloseHandle(f.events[0]);const auto handles=f.k.handles().OpenHandleCount();f.Deliver();
 CHECK(f.k.current_thread()->status==ThreadStatus::Ready&&f.k.current_thread()->pending_wake);
 CHECK(f.k.current_thread()->wait_result==0&&event->signaled()); // second notification remains signaled
 CHECK(f.k.handles().OpenHandleCount()==handles&&f.k.now_ns()==0);
}
}
int main(){RegisteredBroadcastAndLatching();IgnoreThresholdAndWrap();AtomicAndStalePlans();RealWaitAndRetainedEvent();
 if(failures)return EXIT_FAILURE;std::cout<<"PASS: timed-event relay broadcast, latching, threshold, atomicity and real wakes\n";}
