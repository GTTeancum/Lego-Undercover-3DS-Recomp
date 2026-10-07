#include "services/dsp_live_device.h"
#include "runtime/ctr_clock.h"
#include "runtime/ctr_cpu_schedule.h"
#include <algorithm>
#include <array>
#include <limits>

namespace lego::ctr {
namespace {
constexpr std::uint32_t BankBytes=0x40000, TableBytes=160;
std::uint16_t U16(const std::array<std::uint8_t,10>& b,unsigned p){return b[p]|(std::uint16_t(b[p+1])<<8);}
bool Overlap(std::uint32_t a,std::uint32_t n,std::uint32_t b,std::uint32_t m){return a<std::uint64_t(b)+m && b<std::uint64_t(a)+n;}
std::uint16_t AdvancePointer(std::uint16_t pointer,std::uint32_t n,std::uint16_t capacity){
    auto at=(pointer&0x7FFFU)+n;auto wrap=pointer&0x8000U;
    if(at>=capacity){at-=capacity;wrap^=0x8000U;}
    return static_cast<std::uint16_t>(wrap|at);
}
}
std::unique_ptr<DspLiveDevice> DspLiveDevice::Prepare(std::shared_ptr<DspExecutionProbe> probe,
                                                    std::uint64_t now,const char*& error) noexcept {
    error=nullptr;
    if(!probe || probe->summary().state!=DspProbeState::ProtocolComplete || !probe->summary().has_pipe_base){
        error="DSP live device requires completed firmware handshake";return {};
    }
    try {
        auto device=std::unique_ptr<DspLiveDevice>(new DspLiveDevice(std::move(probe)));
        device->table_=std::uint32_t(device->probe_->summary().pipe_base)*2;
        if(!device->ValidateTable()) {error="DSP firmware pipe table is invalid or unknown";return {};}
        const auto tick=SystemTicksFromNanoseconds(now);
        if(tick>std::numeric_limits<std::uint64_t>::max()-SliceSteps) {error="DSP clock overflow";return {};}
        // Pinned LLE: first event +16384 ARM ticks, subsequent events +32768.
        // Boot is synchronous and uncharged, as in the reference loader.
        device->next_tick_=tick+SliceSteps;
        if(!device->next_deadline_ns()) {error="DSP event deadline overflow";return {};}
        if(!device->probe_->ContinueLive(0)){error="DSP live activation failed";return {};}
        return device;
    }catch(const std::bad_alloc&){error="DSP live device allocation failed";return {};}
}
DspPipeResult DspLiveDevice::InspectPipe(std::uint8_t slot,DspPipeDescriptor& out) const noexcept {
    if(error_ || probe_->summary().state==DspProbeState::Fault)return DspPipeResult::Fault;
    if(slot>=16 || std::uint64_t(table_)+TableBytes>BankBytes)return DspPipeResult::Invalid;
    std::array<std::uint8_t,10> b{};const auto memory=probe_->data_backing();
    if(!memory->Read(table_+slot*10,b))return DspPipeResult::Invalid;
    DspPipeDescriptor d{U16(b,0),U16(b,2),U16(b,4),U16(b,6),b[8],b[9],0};
    const auto read=d.read_pointer&0x7FFFU,write=d.write_pointer&0x7FFFU;
    if(d.slot!=slot || !d.capacity || d.capacity>0x7FFF || read>=d.capacity || write>=d.capacity ||
       std::uint64_t(d.address_words)*2+d.capacity>BankBytes ||
       Overlap(d.address_words*2,d.capacity,table_,TableBytes))return DspPipeResult::Invalid;
    const bool wrapped=(d.read_pointer^d.write_pointer)&0x8000U;
    if((!wrapped && write<read)||(wrapped && write>read))return DspPipeResult::Invalid;
    d.used=wrapped?d.capacity+write-read:write-read;out=d;return DspPipeResult::Complete;
}
bool DspLiveDevice::ValidateTable() const noexcept {
    std::array<DspPipeDescriptor,16> descriptors{};
    for(unsigned i=0;i<descriptors.size();++i){
        if(InspectPipe(static_cast<std::uint8_t>(i),descriptors[i])!=DspPipeResult::Complete)return false;
        for(unsigned j=0;j<i;++j)if(Overlap(descriptors[i].address_words*2,descriptors[i].capacity,
                                          descriptors[j].address_words*2,descriptors[j].capacity))return false;
    }
    return true;
}
bool DspLiveDevice::Attach(GuestMemory& memory) {
    if(attached_ || error_)return false;
    if(!memory.MapDeviceMemory(DataAddress,probe_->data_backing(),MemoryPermission::Read|MemoryPermission::Write))
        return false;
    attached_=true;return true;
}
std::optional<std::uint64_t> DspLiveDevice::next_deadline_ns() const noexcept {
    return CpuTickDeadline(next_tick_,0);
}
bool DspLiveDevice::RunScheduled(std::uint64_t now) noexcept {
    if(error_)return false;
    const auto deadline=next_deadline_ns();
    if(!attached_ || !deadline || now<*deadline)return Fail("DSP schedule state/deadline invariant");
    if(next_tick_>std::numeric_limits<std::uint64_t>::max()-2*SliceSteps ||
       slices_==std::numeric_limits<std::uint64_t>::max())return Fail("DSP schedule counter overflow");
    if(!probe_->ContinueLive(SliceSteps))return Fail(probe_->summary().error.data());
    next_tick_+=2*SliceSteps;++slices_;
    if(!next_deadline_ns())return Fail("DSP next deadline overflow after execution");
    return true;
}
bool DspLiveDevice::WritePointer(std::uint8_t slot,std::uint16_t value) noexcept {
    const std::array<std::uint8_t,2> b{static_cast<std::uint8_t>(value),static_cast<std::uint8_t>(value>>8)};
    return probe_->data_backing()->Write(table_+slot*10+(slot%2?6:4),b);
}
DspPipeResult DspLiveDevice::WritePipe(std::uint8_t pipe,std::span<const std::uint8_t> input) noexcept {
    if(pipe>=8)return DspPipeResult::Invalid;
    DspPipeDescriptor d{};auto result=InspectPipe(pipe*2+1,d);if(result!=DspPipeResult::Complete)return result;
    if(input.size()>d.capacity)return DspPipeResult::Invalid;
    if(input.empty())return DspPipeResult::Complete;
    if(input.size()>d.capacity-d.used || !probe_->CanSend(2))return DspPipeResult::WouldBlock;
    if(!ValidateTable())return DspPipeResult::Invalid;
    const auto memory=probe_->data_backing();const auto base=d.address_words*2;
    if(!memory->CanWrite(base,d.capacity) || !memory->CanWrite(table_+d.slot*10+6,2))return DspPipeResult::Fault;
    // Fixed staging handles input overlapping SRAM. All bounds are preflighted.
    std::array<std::uint8_t,0x8000> copy{};std::copy(input.begin(),input.end(),copy.begin());
    const auto at=d.write_pointer&0x7FFFU,n=static_cast<std::uint32_t>(input.size());
    const auto first=std::min<std::uint32_t>(n,d.capacity-at);
    if(!memory->Write(base+at,std::span(copy).first(first)) ||
       (first<n && !memory->Write(base,std::span(copy).subspan(first,n-first))) ||
       !WritePointer(d.slot,AdvancePointer(d.write_pointer,n,d.capacity)) || !probe_->Send(2,d.slot)){
        Fail("DSP pipe write failed after possible partial device commit");return DspPipeResult::Fault;
    }
    return DspPipeResult::Complete;
}
DspPipeResult DspLiveDevice::ReadPipe(std::uint8_t pipe,std::span<std::uint8_t> output, bool wait_for_notification) noexcept {
    if(pipe>=8)return DspPipeResult::Invalid;
    DspPipeDescriptor d{};auto result=InspectPipe(pipe*2,d);if(result!=DspPipeResult::Complete)return result;
    if(output.size()>d.capacity)return DspPipeResult::Invalid;
    if(output.empty())return DspPipeResult::Complete;
    if(output.size()>d.used || (!wait_for_notification && !probe_->CanSend(2)))return DspPipeResult::WouldBlock;
    if(!ValidateTable())return DspPipeResult::Invalid;
    const auto memory=probe_->data_backing();const auto base=d.address_words*2;
    const auto at=d.read_pointer&0x7FFFU,n=static_cast<std::uint32_t>(output.size());
    const auto first=std::min<std::uint32_t>(n,d.capacity-at);
    std::array<std::uint8_t,0x8000> copy{};
    if(!memory->Read(base+at,std::span(copy).first(first)) ||
       (first<n && !memory->Read(base,std::span(copy).subspan(first,n-first))))return DspPipeResult::Invalid;
    if(!memory->CanWrite(table_+d.slot*10+4,2))return DspPipeResult::Fault;
    if(!WritePointer(d.slot,AdvancePointer(d.read_pointer,n,d.capacity))){
        Fail("DSP pipe read pointer commit failed");return DspPipeResult::Fault;
    }
    // Pinned LLE updates its owned read pointer BEFORE waiting for an empty
    // command mailbox. A wait runs real full DSP slices, not forced mailbox clears
    // or fabricated acknowledgements. ARM time/scheduled deadlines are unchanged,
    // matching the reference's synchronous service wait convention.
    constexpr unsigned MaxNotificationSlices=4; // Explicit host work/containment bound.
    unsigned waits=0;
    while(!probe_->CanSend(2)) {
        if(!wait_for_notification || waits==MaxNotificationSlices){
            Fail("DSP pipe notification wait limit after read-pointer commit");return DspPipeResult::Fault;
        }
        if(notification_wait_slices_==std::numeric_limits<std::uint64_t>::max() ||
           !probe_->ContinueLive(SliceSteps)){
            Fail("DSP execution fault during pipe notification wait; partial read retained");return DspPipeResult::Fault;
        }
        ++waits;++notification_wait_slices_;
    }
    if(!probe_->Send(2,d.slot)){
        Fail("DSP pipe notification failed after read-pointer commit");return DspPipeResult::Fault;
    }
    std::copy_n(copy.begin(),n,output.begin());return DspPipeResult::Complete;
}
} // namespace lego::ctr
