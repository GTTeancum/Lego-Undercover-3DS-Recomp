#include "services/gsp_gpu_service.h"
#include <algorithm>
#include <limits>

namespace lego::ctr {
namespace {
std::uint32_t Word(std::span<const std::uint8_t> b,std::uint32_t p) noexcept {
    return std::uint32_t(b[p]) | (std::uint32_t(b[p+1])<<8) |
           (std::uint32_t(b[p+2])<<16) | (std::uint32_t(b[p+3])<<24);
}
void Store(ServiceSharedMemoryObject& page,std::uint32_t offset,std::uint32_t value) noexcept {
    std::array<std::uint8_t,4> b{};
    for(unsigned i=0;i<4;++i)b[i]=static_cast<std::uint8_t>(value>>(8*i));
    (void)page.Write(offset,b);
}
bool Physical(std::uint32_t va,std::uint32_t& pa) noexcept {
    // Same bounded selectors used by this runtime, not arbitrary mappings.
    if (!va) {pa=0;return true;}
    if(va>=0x1F000000U && va<0x1F600000U) {pa=va-0x1F000000U+0x18000000U;return true;}
    if(va>=0x14000000U && va<0x1C000000U) {pa=va-0x14000000U+0x20000000U;return true;}
    return false;
}
}
bool GspGpuService::PrepareDisplayPeriod(DisplayPeriodPlan& out,const char*& error) const noexcept {
    error=nullptr;
    const auto fail=[&](const char* message){error=message;return false;};
    if(shared_->display_generation==std::numeric_limits<std::uint64_t>::max())
        return fail("display event generation exhausted");
    DisplayPeriodPlan plan;
    plan.state=shared_;plan.generation=shared_->display_generation;
    std::copy(shared_->memory->bytes().begin(),shared_->memory->bytes().end(),plan.before_page.begin());
    plan.before_registers=shared_->register_words;plan.before_cached=shared_->display_cached;
    auto staged=plan.before_page;
    // Pinned VBlankCallback sends PDC0 to ALL registered sessions, then PDC1.
    // Unlike command IRQs these are not restricted to the GPU-rights owner.
    for(std::uint32_t screen=0;screen<2;++screen) {
        for(std::uint32_t slot=0;slot<kGspRelaySlots;++slot) {
            const auto session=shared_->slots[slot].lock();
            if(!session || !session->registered || !session->event)continue;
            auto& step=plan.steps[plan.count++];step.event=session->event;
            const auto base=slot*0x40U;step.relay_base=base;step.interrupt=static_cast<std::uint8_t>(screen+2);
            if(staged[base]>=52 || staged[base+1]>52 || staged[base+2])
                return fail("display event has invalid or failed relay ring");
            if(!(staged[base+3]&1U)) {
                if(staged[base+1]>=32) {
                    step.missed=true;step.missed_offset=base+4+4*screen;
                    step.missed_value=Word(staged,step.missed_offset)+1U;
                    for(unsigned i=0;i<4;++i)staged[step.missed_offset+i]=static_cast<std::uint8_t>(step.missed_value>>(8*i));
                } else {
                    step.queue=true;step.slot_offset=base+12+(staged[base]+staged[base+1])%52;
                    step.count=++staged[base+1];staged[step.slot_offset]=step.interrupt;
                }
            }
            step.fb_base=0x200U+(2*slot+screen)*0x40U;
            if(staged[step.fb_base+1]&1U) {
                step.update_fb=true;step.dirty_byte=staged[step.fb_base+1]&~1U;
                const auto info=step.fb_base+4+(staged[step.fb_base]&1U)*0x1CU;
                for(unsigned n=0;n<7;++n)step.info[n]=Word(staged,info+n*4);
                if(!Physical(step.info[1],step.physical_left)||!Physical(step.info[2],step.physical_right))
                    return fail("display framebuffer uses unsupported address selector");
                staged[step.fb_base+1]=step.dirty_byte;
            }
        }
    }
    out=std::move(plan);return true;
}
bool GspGpuService::CommitDisplayPeriod(Kernel& kernel,const DisplayPeriodPlan& plan) noexcept {
    if(plan.state.get()!=shared_.get() || plan.generation!=shared_->display_generation ||
       plan.count>plan.steps.size() || plan.before_registers!=shared_->register_words ||
       plan.before_cached!=shared_->display_cached ||
       !std::equal(plan.before_page.begin(),plan.before_page.end(),shared_->memory->bytes().begin()))return false;
    for(std::size_t n=0;n<plan.count;++n) {
        const auto& step=plan.steps[n];
        if(step.queue) {
            (void)shared_->memory->Write(step.relay_base+1,{&step.count,1});
            (void)shared_->memory->Write(step.slot_offset,{&step.interrupt,1});
            kernel.SignalEventObject(*step.event);
        } else if(step.missed)Store(*shared_->memory,step.missed_offset,step.missed_value);
        // Reference signals the relay event before latching dirty framebuffer info.
        // CPU execution resumes only after this synchronous device event finishes.
        if(step.update_fb) {
            const auto screen=step.interrupt-2U;
            shared_->display_cached[screen]=step.info;
            (void)shared_->memory->Write(step.fb_base+1,{&step.dirty_byte,1});
            const auto reg=0x100U+screen*0x40U;
            const auto second=step.info[0]!=0;
            shared_->register_words[reg+0x1A+second]=step.physical_left;
            shared_->register_words[reg+0x25+second]=step.physical_right;
            shared_->register_words[reg+0x24]=step.info[3];
            shared_->register_words[reg+0x1C]=step.info[4];
            shared_->register_words[reg+0x1E]=step.info[5];
        }
    }
    ++shared_->display_generation;return true;
}
} // namespace lego::ctr
