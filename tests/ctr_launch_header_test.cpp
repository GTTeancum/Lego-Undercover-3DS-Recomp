#include "host/launch_header.h"
#include "runtime/ctr_kernel.h"
#include <array>
#include <iostream>
#include <cstdlib>
using namespace lego::ctr;
namespace {
int failures=0;
#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}}while(0)
void LayoutAndIdentity() {
    std::array<std::uint8_t,0x800> synthetic{};
    constexpr std::uint64_t id=0x00040000000AD500ULL;
    for(unsigned i=0;i<8;++i)synthetic[0x200+i]=static_cast<std::uint8_t>(id>>(8*i));
    synthetic[0x20E]=4;synthetic[0x20F]=48;synthetic[0x210]=0x9E;
    const auto p=lego::host::ReadLaunchPolicy(synthetic);
    CHECK(p && p->program_id==id && p->maximum_cpu==30 && p->multi &&
          p->ideal_processor==0 && p->affinity_bits==1 && p->priority==48 && p->category==0);
    // Plausible synthetic metadata is NOT an authenticated original header.
    CHECK(!lego::host::VerifiedLegoLaunchPolicy(synthetic));
    CHECK(!lego::host::ReadLaunchPolicy(std::span(synthetic).first(0x400)));
    CHECK(!lego::host::VerifiedLegoLaunchPolicy(std::span(synthetic).first(0x7FF)));
    for(unsigned mode=0;mode<2;++mode)for(unsigned max=0;max<128;++max) {
        synthetic[0x210]=static_cast<std::uint8_t>(max|(mode<<7));synthetic[0x211]=0xAB;
        const auto v=lego::host::ReadLaunchPolicy(synthetic);
        CHECK(v && v->maximum_cpu==max && v->multi==bool(mode));
    }
}
void LaunchState() {
    Kernel k;CHECK(k.app_cpu_time_maximum()==80 && k.app_cpu_time_current()==0);
    CHECK(!k.ConfigureCpuExecution(CpuExecutionMode::Strict,30));
    CHECK(!k.ConfigureCpuExecution(CpuExecutionMode::DiagnosticDual,101));
    CHECK(k.app_cpu_time_maximum()==80 && k.cpu_execution_mode()==CpuExecutionMode::Strict);
    const auto handles=k.handles().OpenHandleCount();
    CHECK(k.ConfigureCpuExecution(CpuExecutionMode::DiagnosticDual,30));
    CHECK(k.app_cpu_time_maximum()==30 && k.app_cpu_time_current()==0 && k.now_ns()==0);
    CHECK(k.handles().OpenHandleCount()==handles && k.threads().size()==1);
    CHECK(k.UpdateAppCpuTimeLimit(30)==0 && k.app_cpu_time_current()==30);
    CHECK(k.UpdateAppCpuTimeLimit(31)==0 && k.app_cpu_time_current()==30);
    CHECK(!k.ConfigureCpuExecution(CpuExecutionMode::DiagnosticDual,80));
    CHECK(k.app_cpu_time_maximum()==30 && k.app_cpu_time_current()==30);
    CHECK(k.ConfigureCpuExecution(CpuExecutionMode::DiagnosticDual)); // Runner may confirm, not replace.
    Handle res=0;CHECK(k.GetResourceLimit(&res,kCurrentProcessPseudoHandle)==0);
    const auto object=std::dynamic_pointer_cast<ResourceLimitObject>(k.handles().Get(res));
    CHECK(object && object->Limit(ResourceLimitType::CpuTime)==30 && object->Current(ResourceLimitType::CpuTime)==30);
    Kernel late;late.AdvanceTime(1);
    CHECK(!late.ConfigureCpuExecution(CpuExecutionMode::DiagnosticDual,30) && late.app_cpu_time_maximum()==80);
    Kernel active;Handle child=0;CHECK(active.CreateThread(&child,0x100000,0,0x8001000,48,0)==0);
    CHECK(!active.ConfigureCpuExecution(CpuExecutionMode::DiagnosticDual,30) && active.app_cpu_time_maximum()==80);
    Kernel old;CHECK(old.ConfigureCpuExecution(CpuExecutionMode::DiagnosticDual));
    CHECK(old.app_cpu_time_maximum()==80); // No silent change to existing CLI behavior.
}
}
int main(){LayoutAndIdentity();LaunchState();if(failures)return EXIT_FAILURE;
 std::cout<<"PASS: launch field layout, identity rejection, actual resource ceiling and immutable live policy\n";}
