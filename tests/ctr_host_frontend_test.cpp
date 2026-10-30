#include "runtime/ctr_runner.h"
#include "services/hid_user_service.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>

using namespace lego::ctr;
namespace {
int failures{};
#define CHECK(x) do { if (!(x)) { std::cerr<<"FAIL "<<__LINE__<<": " #x "\n"; ++failures; } } while (0)
// Independently authored synthetic guest programs; no game input is embedded.
const a32::PackedOp loop_ops[]{
    {0xEAFFFFFE,a32::EncodeMetadata(a32::Opcode::Branch,a32::Condition::Al)}};
const a32::PackedOp input_ops[]{
    {0xEF000024,a32::EncodeMetadata(a32::Opcode::Svc,a32::Condition::Al)},
    {0xE59410C8,a32::EncodeMetadata(a32::Opcode::CoreMemory,a32::Condition::Al)},
    {0xE5851000,a32::EncodeMetadata(a32::Opcode::CoreMemory,a32::Condition::Al)},
    {0xEF000009,a32::EncodeMetadata(a32::Opcode::Svc,a32::Condition::Al)}};
const a32::Block blocks[]{{0x100000,loop_ops,1,false},{0x100100,input_ops,4,false}};
const a32::BlockShard shards[]{{0x100000,0x100FFC,blocks,2}};
const a32::Registry registry{shards,1,nullptr,0};

struct Frontend final : HostFrontend {
    unsigned calls{}, frames{}, polls{}, samples{};
    unsigned stop_after{};
    HostFrontendResult result{HostFrontendResult::Continue};
    HostInputState input{};
    HostFrontendResult Pump(const GspGpuService&,bool frame_ready,HostInputState& out) noexcept override {
        ++calls;
        if (frame_ready) ++frames; else ++polls;
        out=input;
        return stop_after && calls>=stop_after ? result : HostFrontendResult::Continue;
    }
    void InputSampled() noexcept override { ++samples; }
    const char* error() const noexcept override { return "synthetic frontend failure"; }
};
struct Fixture {
    Kernel kernel;
    GuestMemory memory;
    NativeRunner runner;
    explicit Fixture(CpuExecutionMode cpu=CpuExecutionMode::DiagnosticDual,
                     DisplayClockMode display=DisplayClockMode::ReferenceIdle)
        : runner(registry,memory,kernel,kDefaultRtcMsSince1900,{},PtmStepMode::Unconfigured,{},
                 GpuVramMode::Unconfigured,display,CfgProfile::Unconfigured,cpu) {
        CHECK(runner.InitializeMainThread());
    }
    void WaitForInput() {
        CHECK(memory.MapSharedServicePage(0x10000000,runner.hid_diagnostics().shared_memory(),MemoryPermission::Read));
        CHECK(memory.Map(0x08000000,4,MemoryPermission::Read|MemoryPermission::Write));
        Handle event{};
        CHECK(kernel.handles().Create(&event,runner.hid_diagnostics().event(0))==0);
        auto& s=runner.live_state();s.r[15]=0x100100;s.r[0]=event;
        s.r[2]=s.r[3]=0xFFFFFFFF;s.r[4]=0x10000000;s.r[5]=0x08000000;
    }
};
void DisabledAndNoDeadline() {
    Fixture headless,desktop;
    Frontend front;
    CHECK(desktop.runner.SetHostFrontend(&front));
    const auto a=headless.runner.Run(4000,100),b=desktop.runner.Run(4000,100);
    CHECK(a.reason==RunnerStopReason::BlockLimit && b.reason==a.reason);
    CHECK(a.exit.pc==b.exit.pc && a.dispatch_rounds==b.dispatch_rounds);
    CHECK(headless.runner.live_state().r==desktop.runner.live_state().r);
    CHECK(headless.kernel.now_ns()==desktop.kernel.now_ns());
    CHECK(headless.runner.issued_instructions()==desktop.runner.issued_instructions());
    CHECK(front.calls==1 && front.frames==1 && front.polls==0 && front.samples==0);
    CHECK(headless.runner.hid_diagnostics().host_input_samples()==0);
    CHECK(desktop.runner.hid_diagnostics().host_input_samples()==0);
    for (const auto* f:{&headless,&desktop}) {
        const auto& hid=f->runner.hid_diagnostics();
        CHECK(std::all_of(hid.shared_memory()->bytes().begin(),hid.shared_memory()->bytes().end(),[](auto b){return !b;}));
        for (unsigned i=0;i<5;++i) CHECK(!hid.event(i)->signaled());
    }
    CHECK(!desktop.runner.SetHostFrontend(nullptr));
    CHECK(!headless.runner.SetHostFrontend(&front));
}
void PollStopsAndResumedBudget() {
    for (const auto expected:{HostFrontendResult::Quit,HostFrontendResult::Error}) {
        Fixture f;Frontend front;front.stop_after=2;front.result=expected;
        CHECK(f.runner.SetHostFrontend(&front));
        RunnerResult stopped;
        unsigned runs=0;
        do { stopped=f.runner.Run(100,100);++runs; }
        while(stopped.reason==RunnerStopReason::BlockLimit && runs<50);
        CHECK(stopped.reason==(expected==HostFrontendResult::Quit?RunnerStopReason::HostQuit:RunnerStopReason::HostFrontendError));
        CHECK(stopped.exit.pc==0x100000 && stopped.exit.kind==a32::ExitKind::Wait);
        CHECK(f.runner.issued_instructions()[0]==4096 && f.runner.issued_instructions()[1]==0);
        CHECK(f.kernel.now_ns()==*CpuTickDeadline(4095));
        CHECK(front.calls==2 && front.polls==1 && front.frames==1 && front.samples==0);
        CHECK(f.runner.hid_diagnostics().host_input_samples()==0);
        if (expected==HostFrontendResult::Error)
            CHECK(std::strcmp(f.runner.frontend_error(),"synthetic frontend failure")==0);
        const auto now=f.kernel.now_ns();
        CHECK(f.runner.Run(100,100).reason==stopped.reason && f.kernel.now_ns()==now && front.calls==2);
    }
    Fixture initial;Frontend close;close.stop_after=1;close.result=HostFrontendResult::Quit;
    CHECK(initial.runner.SetHostFrontend(&close));
    CHECK(initial.runner.Run().reason==RunnerStopReason::HostQuit);
    CHECK(initial.runner.issued_instructions()[0]==0 && initial.kernel.now_ns()==0);
}
void RealHidWakeAndGuestRead() {
    Fixture f;Frontend front;front.input={1,319,239,true,0,0};
    f.WaitForInput();CHECK(f.runner.SetHostFrontend(&front));
    const auto result=f.runner.Run(100,100);
    CHECK(result.reason==RunnerStopReason::ProcessExited);
    CHECK(f.runner.display_periods()==1 && f.runner.hid_diagnostics().host_input_samples()==1);
    CHECK(front.frames==2 && front.polls==0 && front.samples==1);
    CHECK(f.runner.issued_instructions()[0]==4);
    CHECK(f.kernel.now_ns()>=16713681 && f.kernel.now_ns()<16713700);
    std::uint32_t observed{};
    CHECK(f.memory.Read32(0x08000000,&observed) && observed==0x00EF013F);
    CHECK(f.memory.Read32(0x1000001C,&observed) && observed==1);
    std::uint64_t stamp{};
    CHECK(f.memory.Read64(0x10000000,&stamp,nullptr) && stamp==4481136);
    CHECK(f.memory.Read64(0x100000A8,&stamp,nullptr) && stamp==4481136);
    CHECK(!f.runner.hid_diagnostics().event(0)->signaled());
    CHECK(f.runner.hid_diagnostics().event(1)->signaled());
    CHECK(!f.memory.Write32(0x100000C8,0));
}
void RejectedConfigurationAndInput() {
    Frontend front;
    Fixture no_clock(CpuExecutionMode::DiagnosticDual,DisplayClockMode::Disabled);
    CHECK(!no_clock.runner.SetHostFrontend(&front));
    Fixture strict(CpuExecutionMode::Strict);
    CHECK(!strict.runner.SetHostFrontend(&front));
    Fixture f;f.WaitForInput();front.input={0,320,0,true,0,0};
    CHECK(f.runner.SetHostFrontend(&front));
    CHECK(!f.runner.SetHostFrontend(&front));
    const auto stopped=f.runner.Run(100,100);
    CHECK(stopped.reason==RunnerStopReason::HostFrontendError && stopped.exit.pc==0x100104);
    CHECK(f.kernel.now_ns()==16713681 && f.runner.display_periods()==1);
    CHECK(f.runner.issued_instructions()[0]==1 && f.runner.hid_diagnostics().host_input_samples()==0);
    CHECK(f.runner.frontend_error()!=nullptr);
    CHECK(front.samples==0);
    CHECK(!f.runner.hid_diagnostics().event(0)->signaled());
    CHECK(!f.runner.hid_diagnostics().event(1)->signaled());
    std::uint32_t untouched=1;CHECK(f.memory.Read32(0x08000000,&untouched) && untouched==0);
}
} // namespace
int main() {
    DisabledAndNoDeadline();PollStopsAndResumedBudget();RealHidWakeAndGuestRead();RejectedConfigurationAndInput();
    if (failures) return EXIT_FAILURE;
    std::cout<<"PASS: optional host frontend, bounded polls, distinct host stops, deadline HID sampling and real guest touchscreen read\n";
}
