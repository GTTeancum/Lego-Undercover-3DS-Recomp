#include "services/gsp_gpu_service.h"
#include "runtime/ctr_svc_bridge.h"
#include <array>
#include <bit>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace lego::ctr;
namespace {
int failures{};
#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}} while(0)
constexpr auto kRw=MemoryPermission::Read|MemoryPermission::Write;
constexpr std::uint32_t kPage=0x10000000,kList=0x14000000,kVertices=0x14002000;
constexpr std::uint32_t kFramebuffer=0x1000,kTransfer=0x4000;
constexpr std::uint32_t kTexture=0x6000,kDepth=0x8000,kTextureVa=0x14003000;
using Packet=std::array<std::uint32_t,8>;

// A complete, authored two-MOV shader and four-corner strip. No game commands,
// shaders, vertices or output images are used in this integration fixture.
struct Fixture {
    Kernel kernel;GuestMemory memory;IpcRouter ipc;SvcBridge bridge{kernel,&ipc};
    std::shared_ptr<GspGpuService> gpu=std::make_shared<GspGpuService>();
    std::shared_ptr<GpuVramBank> bank=GpuVramBank::ReferenceZero();
    std::shared_ptr<EventObject> event;Handle session{},event_handle{};
    a32::GuestState cpu{};std::vector<std::uint32_t> list;
    Fixture() {
        CHECK(memory.EnsureTlsMappings(kernel));CHECK(memory.Map(kList,0x4000,kRw));
        CHECK(gpu->ConfigureVram(bank));CHECK(ipc.RegisterService("gsp::Gpu",gpu)==0);
        CHECK(ipc.ConnectToService(kernel,"gsp::Gpu",&session)==0);
        CHECK(kernel.CreateEvent(&event_handle,0)==0);
        event=std::dynamic_pointer_cast<EventObject>(kernel.handles().Get(event_handle));
        Put({0x00130042,1,0,event_handle});CHECK(Call().kind==a32::ExitKind::Fallthrough);
        Put({0x00160042,0,0,kCurrentProcessPseudoHandle});CHECK(Call().kind==a32::ExitKind::Fallthrough);
        CHECK(memory.MapSharedServicePage(kPage,gpu->shared_memory(),kRw));
        std::vector<std::uint8_t> initial(kGpuVramBytes,0xA5);
        for(unsigned pixel=0;pixel<64;++pixel) {
            initial[kFramebuffer+4*pixel]=0x55;
            initial[kFramebuffer+4*pixel+1]=0x66;
            initial[kFramebuffer+4*pixel+2]=0x77;
            initial[kFramebuffer+4*pixel+3]=0x88;
        }
        CHECK(bank->Write(0,initial));
        Configure();Vertices();
    }
    unsigned cb()const{return kernel.current_thread()->tls_address+kIpcCommandBufferOffset;}
    void Put(const IpcCommandBuffer& command={0xC0000}) {
        for(unsigned i=0;i<command.size();++i)CHECK(memory.Write32(cb()+4*i,command[i]));
        for(unsigned i=0;i<16;++i)cpu.r[i]=0xAC000000U+i;
        cpu.r[0]=session;cpu.r[15]=0x25947C;cpu.cpsr=0x60000010;cpu.fpscr=0x23000010;
    }
    a32::ExecutionResult Call(){return bridge.Handle({a32::ExitKind::Svc,cpu.r[15],a32::FallbackReason::None,kSvcSendSyncRequest},cpu,&memory);}
    void Word(unsigned offset,std::uint32_t value){CHECK(memory.Write32(kPage+offset,value));}
    void Set(unsigned id,std::uint32_t value,unsigned mask=15){list.push_back(value);list.push_back(id|(mask<<16));}
    void Configure() {
        for(auto [id,value]:std::vector<std::pair<unsigned,std::uint32_t>>{
            {0x40,0},{0x41,0x410000},{0x43,0x410000},{0x47,0},
            {0x4D,0xBF0000},{0x4E,0},{0x4F,2},{0x50,0x03020100},{0x51,0x0B0A0908},
            {0x65,0},{0x68,0},{0x6D,1},{0x80,0},{0xE0,0},{0x100,0x100},
            {0x101,0x76760000},{0x104,0},{0x105,0},{0x107,0x700},
            {0x113,15},{0x115,0},{0x116,3},{0x117,0},
            {0x11D,(kGpuVramPhysicalBase+kFramebuffer)/8},{0x11E,0x01007008},
            {0x1C6,1},{0x25E,0x100}})Set(id,value);
        for(unsigned stage=0;stage<6;++stage) {
            const unsigned base=stage<4?0xC0+8*stage:0xF0+8*(stage-4);
            Set(base,stage?0x000F000F:0);for(unsigned word=1;word<5;++word)Set(base+word,0);
        }
        Set(0x200,(kVertices-0x14000000U+0x20000000U)/8);Set(0x201,0xFF);Set(0x202,0x10000000);
        for(unsigned id=0x203;id<=0x226;++id)Set(id,0);
        Set(0x203,0);Set(0x204,0x10);Set(0x205,0x20200000);
        Set(0x228,4);Set(0x229,0);Set(0x22A,0);Set(0x244,0);
        Set(0x2B9,0xA0000001);Set(0x2BA,0);Set(0x2BB,0x10);Set(0x2BC,0);Set(0x2BD,3);
        Set(0x2D5,0);Set(0x2D6,0x0000036F); // xyzw identity, every output component.
        Set(0x2CB,0);Set(0x2CC,0x4C000000);Set(0x2CC,0x4C201000);Set(0x2CC,0x88000000);
    }
    void Vertices() {
        const std::array<std::array<float,2>,4> corners{{{1,1},{1,-1},{-1,1},{-1,-1}}};
        for(unsigned v=0;v<8;++v) {
            const std::array<float,8> values{corners[v%4][0],corners[v%4][1],0,1,
                                           v<4?1.0F:0.0F,v<4?0.0F:1.0F,0,1};
            for(unsigned component=0;component<8;++component)
                CHECK(memory.Write32(kVertices+32*v+4*component,std::bit_cast<std::uint32_t>(values[component])));
        }
    }
    void TexturedDepth(bool private_texture=false) {
        // The existing authored MOV output1 now supplies ordinary texture UV.
        // Unmapped primary-color components retain the established value1.
        Set(0x51,0x1F1F0D0C);
        for(unsigned vertex=0;vertex<4;++vertex) {
            CHECK(memory.Write32(kVertices+32*vertex+8,std::bit_cast<std::uint32_t>(-0.5f)));
            CHECK(memory.Write32(kVertices+32*vertex+16,std::bit_cast<std::uint32_t>(0.5f)));
            CHECK(memory.Write32(kVertices+32*vertex+20,std::bit_cast<std::uint32_t>(0.5f)));
        }
        Set(0x80,1);Set(0x82,0x00100010);Set(0x83,6);Set(0x84,0);Set(0x8E,0);
        Set(0x85,(private_texture?kTextureVa-0x14000000U+0x20000000U:kGpuVramPhysicalBase+kTexture)/8);
        Set(0xC0,0x00030003); // Replace RGB and alpha with texture0.
        std::vector<std::uint8_t> texture(1024);
        for(unsigned i=0;i<texture.size();i+=4) {
            texture[i]=0x80;texture[i+1]=0x30;texture[i+2]=0x20;texture[i+3]=0x10;
        }
        if(private_texture)CHECK(memory.LoadBytes(kTextureVa,texture));
        else CHECK(bank->Write(kTexture,texture));
        Set(0x107,0x1741); // RGB writes, Less depth test and depth writes.
        Set(0x114,2);Set(0x115,2);Set(0x116,3);Set(0x11C,(kGpuVramPhysicalBase+kDepth)/8);
        std::vector<std::uint8_t> depth(256,0xFF);
        for(unsigned i=3;i<depth.size();i+=4)depth[i]=0x5A;
        CHECK(bank->Write(kDepth,depth));
    }
    void Draw(unsigned mask=15){Set(0x22E,0,mask);}
    void Restart(){Set(0x25F,0,0);}
    void Irq(){Set(0x10,0x12345678);}
    void PacketAt(unsigned index,const Packet& packet){for(unsigned word=0;word<8;++word)Word(0x820+32*index+4*word,packet[word]);}
    void Submit(unsigned packet=0,unsigned count=1) {
        CHECK(list.size()*4<0x2000);
        for(unsigned i=0;i<list.size();++i)CHECK(memory.Write32(kList+4*i,list[i]));
        PacketAt(packet,{1,kList,static_cast<std::uint32_t>(4*list.size()),0,0,0,0,0});Word(0x800,count<<8);
    }
    std::vector<std::uint8_t> Page()const{auto b=gpu->shared_memory()->bytes();return {b.begin(),b.end()};}
    std::vector<std::uint8_t> Vram()const{auto b=bank->bytes();return {b.begin(),b.end()};}
    PicaGpuRegisters Regs()const{PicaGpuRegisters out;for(unsigned i=0;i<out.size();++i)out[i]=*gpu->register_word(0x400000+4*i);return out;}
    void Success(){Put();CHECK(Call().kind==a32::ExitKind::Fallthrough&&!ipc.unsupported_request());if(ipc.unsupported_request())std::cerr<<ipc.last_host_error()<<'\n';CHECK(cpu.r[0]==0&&cpu.r[15]==0x259480);}
    void Stop(const char* reason) {
        Put();const auto before_vram=Vram(),before_page=Page();const auto before_regs=Regs();
        const auto before_uploads=gpu->pica_uploads();const auto before_cpu=cpu;
        const auto before_event=event->signaled(),before_empty=before_uploads.primitive_empty;
        CHECK(Call().kind==a32::ExitKind::Svc&&ipc.unsupported_request());
        CHECK(ipc.last_host_error().find(reason)!=std::string::npos);
        CHECK(Vram()==before_vram&&Page()==before_page&&Regs()==before_regs&&gpu->pica_uploads()==before_uploads);
        CHECK(cpu.r==before_cpu.r&&cpu.cpsr==before_cpu.cpsr&&cpu.fpscr==before_cpu.fpscr);
        CHECK(event->signaled()==before_event&&gpu->pica_uploads().primitive_empty==before_empty&&kernel.now_ns()==0);
        CHECK(gpu->last_pica_result().draws==0);
    }
    void Pixels(const std::array<std::uint8_t,4>& expected)const {
        const auto bytes=bank->bytes();bool equal=true;
        for(unsigned pixel=0;pixel<64;++pixel)
            for(unsigned c=0;c<4;++c)equal&=bytes[kFramebuffer+4*pixel+c]==expected[c];
        CHECK(equal);
    }
    void DepthPixels()const {
        const auto bytes=bank->bytes();bool equal=true;
        for(unsigned pixel=0;pixel<64;++pixel) {
            const auto offset=kDepth+4*pixel;
            equal&=bytes[offset]==0xFF && bytes[offset+1]==0xFF && bytes[offset+2]==0x7F && bytes[offset+3]==0x5A;
        }
        CHECK(equal); // Z=1/2 truncates to0x7fffff, preserving stencil0x5a.
    }
    std::vector<std::uint8_t> PrivateBytes(unsigned address,unsigned count) {
        std::vector<std::uint8_t> out(count);
        for(unsigned i=0;i<count;++i)CHECK(memory.Read8(address+i,&out[i]));
        return out;
    }
};

void RealPixelsThenIrq() {
    Fixture f;f.Draw(0);f.Irq();f.Submit();const auto before=f.Vram();f.Success();
    f.Pixels({0x55,0,0,255});const auto after=f.bank->bytes();bool outside=true;
    for(unsigned i=0;i<after.size();++i)if(i<kFramebuffer||i>=kFramebuffer+256)outside&=after[i]==before[i];
    CHECK(outside);const auto& result=f.gpu->last_pica_result();
    CHECK(result.draws==1&&result.vertices==4&&result.shader_instructions==12&&result.triangles==2);
    CHECK(result.pixels_covered==64&&result.pixels_written==64&&result.irqs==1);
    CHECK(!f.gpu->pica_uploads().primitive_empty&&f.Page()[1]==1&&f.Page()[12]==5&&f.event->signaled());
    CHECK(f.kernel.now_ns()==0);CHECK(f.kernel.WaitSynchronization1(f.event_handle,0).result==0);
    const auto done=f.Vram();f.Put();CHECK(f.Call().kind==a32::ExitKind::Fallthrough&&f.Vram()==done&&!f.event->signaled());
}

void WholeQueueRollback() {
    Fixture register_failure;register_failure.Draw();register_failure.Set(0x23C,1);register_failure.Submit();register_failure.Stop("chaining");
    Fixture packet_failure;packet_failure.Draw();packet_failure.Irq();packet_failure.Submit(0,2);
    packet_failure.PacketAt(1,{0x7F,0,0,0,0,0,0,0});packet_failure.Stop("queue packet");
    Fixture irq_failure;irq_failure.Draw();irq_failure.Irq();irq_failure.Submit();irq_failure.Word(0,0x3400);irq_failure.Stop("relay");
    Fixture continuation;continuation.Draw();continuation.Draw();continuation.Submit();continuation.Stop("primitive restart");
    Fixture bound;for(unsigned i=0;i<17;++i){bound.Restart();bound.Draw();}bound.Submit();bound.Stop("whole-queue host bound");
    Fixture indexed;indexed.Set(0x22F,1);indexed.Submit();indexed.Stop("non-indexed");
}

void OrderedDrawsAndTransfers() {
    Fixture two;two.Draw();two.Restart();two.Set(0x22A,4);two.Draw();two.Irq();two.Submit();two.Success();
    two.Pixels({0x55,0,255,0});CHECK(two.gpu->last_pica_result().draws==2&&two.gpu->last_pica_result().pixels_written==128);
    Fixture split;split.Draw();split.Submit();split.Success();split.list.clear();split.Restart();split.Set(0x22A,4);split.Draw();split.Submit();split.Success();split.Pixels({0x55,0,255,0});
    Fixture transfer;transfer.Draw();transfer.Irq();transfer.Submit(0,2);
    transfer.PacketAt(1,{3,kGpuVramVirtualBase+kFramebuffer,kGpuVramVirtualBase+kTransfer,0x80008,0x80008,0x01001004,0,0});
    transfer.Success();bool pixels=true;for(unsigned p=0;p<32;++p){const auto at=kTransfer+3*p;const auto bytes=transfer.bank->bytes();pixels&=bytes[at]==0&&bytes[at+1]==0&&bytes[at+2]==255;}
    CHECK(pixels&&transfer.Page()[1]==2&&transfer.Page()[12]==5&&transfer.Page()[13]==4);
}

void FillForwardingAndPrivateSourceGuard() {
    const Packet fill{2,kGpuVramVirtualBase+kFramebuffer,0x11223344,kGpuVramVirtualBase+kFramebuffer+256,0,0,0,0x201};
    Fixture before;before.Draw();before.Irq();before.Submit(1,2);before.PacketAt(0,fill);before.Success();before.Pixels({0x44,0,0,255});
    CHECK(before.Page()[1]==2&&before.Page()[12]==0&&before.Page()[13]==5);
    Fixture after;after.Draw();after.Irq();after.Submit(0,2);after.PacketAt(1,fill);after.Success();after.Pixels({0x44,0x33,0x22,0x11});
    Fixture stale;stale.Draw();stale.Submit(1,2);
    stale.PacketAt(0,{3,kGpuVramVirtualBase+kTransfer,kVertices,0x80008,0x80008,0x4400,0,0});
    stale.Stop("earlier staged FCRAM transfer");
}

void TextureSnapshotAndDepthWrites() {
    Fixture f;f.TexturedDepth();f.Draw();f.Irq();f.Submit(1,2);
    // Before the draw, a staged fill replaces the texture with RGBA(128,96,64,255).
    // Reading live VRAM instead of the ordered snapshot would give a different color.
    f.PacketAt(0,{2,kGpuVramVirtualBase+kTexture,0x806040FF,
                  kGpuVramVirtualBase+kTexture+1024,0,0,0,0x201});
    const auto before=f.Vram();f.Success();f.Pixels({0x55,0x40,0x60,0x80});f.DepthPixels();
    CHECK(f.gpu->last_pica_result().pixels_written==64 && f.gpu->last_pica_result().draws==1);
    CHECK(f.Page()[1]==2 && f.Page()[12]==0 && f.Page()[13]==5 && f.event->signaled());
    const auto after=f.Vram();bool outside=true,texture=true;
    constexpr std::array<std::uint8_t,4> filled{0xFF,0x40,0x60,0x80};
    for(unsigned i=0;i<after.size();++i) {
        const bool changed=(i>=kFramebuffer&&i<kFramebuffer+256) ||
                           (i>=kDepth&&i<kDepth+256) || (i>=kTexture&&i<kTexture+1024);
        if(!changed)outside&=after[i]==before[i];
        if(i>=kTexture&&i<kTexture+1024)texture&=after[i]==filled[(i-kTexture)%4];
    }
    CHECK(outside && texture);
}

void TextureFcramDependenciesAndRollback() {
    Fixture overlap;overlap.TexturedDepth(true);overlap.Draw();overlap.Irq();overlap.Submit(1,2);
    overlap.PacketAt(0,{3,kGpuVramVirtualBase+kTransfer,kTextureVa+768,0x80008,0x80008,0x4400,0,0});
    const auto before_texture=overlap.PrivateBytes(kTextureVa,1024);
    overlap.Stop("PICA texture source depends on an earlier staged FCRAM transfer");
    CHECK(overlap.PrivateBytes(kTextureVa,1024)==before_texture);

    Fixture disjoint;disjoint.TexturedDepth(true);disjoint.Draw();disjoint.Irq();disjoint.Submit(1,2);
    disjoint.PacketAt(0,{3,kGpuVramVirtualBase+kTransfer,kTextureVa+1024,0x80008,0x80008,0x4400,0,0});
    const auto source=disjoint.PrivateBytes(kTextureVa,1024);
    disjoint.Success();disjoint.Pixels({0x55,74,75,75});disjoint.DepthPixels();
    CHECK(disjoint.PrivateBytes(kTextureVa,1024)==source);
    CHECK(disjoint.PrivateBytes(kTextureVa+1024,128)==std::vector<std::uint8_t>(128,0xA5));
    CHECK(disjoint.Page()[1]==2 && disjoint.Page()[12]==4 && disjoint.Page()[13]==5);

    Fixture packet;packet.TexturedDepth();packet.Draw();packet.Irq();packet.Submit(0,2);
    packet.PacketAt(1,{0x7F,0,0,0,0,0,0,0});packet.Stop("queue packet");
    Fixture relay;relay.TexturedDepth();relay.Draw();relay.Irq();relay.Submit();
    relay.Word(0,0x3400);relay.Stop("relay");
}
} // namespace

int main(){RealPixelsThenIrq();WholeQueueRollback();OrderedDrawsAndTransfers();FillForwardingAndPrivateSourceGuard();
    TextureSnapshotAndDepthWrites();TextureFcramDependenciesAndRollback();
    if(failures)return EXIT_FAILURE;
    std::cout<<"PASS: native draw/texture/depth bytes, ordered device staging, restart, IRQ and complete-queue rollback\n";
}
