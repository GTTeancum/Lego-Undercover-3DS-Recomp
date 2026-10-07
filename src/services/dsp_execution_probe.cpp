#include "services/dsp_execution_probe.h"
#include "services/dsp_boot_handshake.h"
#include "services/dsp_ram.h"
#include "teakra/teakra.h"
#include "teakra/impl/register.h"
#include <algorithm>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <utility>

namespace lego::ctr {
struct DspExecutionProbe::Impl final : DspBootMailbox {
    std::shared_ptr<DspRam> ram=std::make_shared<DspRam>();
    std::vector<std::uint8_t>& memory=ram->bytes;
    std::vector<std::uint8_t>& source=ram->provenance;
    Teakra::Teakra dsp{{memory.data()}};
    DspBootHandshake handshake;
    const DspBootMode boot_mode;
    std::uint32_t boot_remaining{};
    DspProbeSummary state{};
    std::array<DspProbeReply,256> history{};
    bool live{},live_data_signaled{},live_semaphore_signaled{};
    bool capture_audio{};
    std::array<DspCapturedAudioFrame,kAudioCaptureCapacity> audio{};
    std::size_t audio_count{};
    std::uint64_t audio_total{};
    std::shared_ptr<DspAudioSink> audio_sink;
    std::uint16_t live_pending{};
    std::uint64_t live_notification_count{};
    void Emit(unsigned bit) {
        if(bit>=10 || live_notification_count==std::numeric_limits<std::uint64_t>::max())
            throw std::runtime_error("DSP live interrupt accounting overflow");
        live_pending|=static_cast<std::uint16_t>(1U<<bit);++live_notification_count;
    }
    void PipeEvent(bool from_data) {
        if(from_data)live_data_signaled=true;
        else {if(!(dsp.GetSemaphore()&0x8000))return;live_semaphore_signaled=true;}
        if(!live_data_signaled || !live_semaphore_signaled)return;
        live_data_signaled=live_semaphore_signaled=false;
        if(!dsp.RecvDataIsReady(2))throw std::runtime_error("DSP pipe notification lacks a mailbox word");
        const auto slot=dsp.RecvData(2);
        if(slot>=16)throw std::runtime_error("DSP pipe notification slot outside descriptor table");
        if(slot&1)return; // CPU-to-DSP direction is not a received-data event.
        if(slot==0)throw std::runtime_error("DSP debug-pipe draining is not implemented");
        Emit(2+slot/2);
    }

    Impl(const Dsp1Image& image,DspProbeReset reset,bool capture,bool reference_silence,bool reference_transmit,DspBootMode mode,std::shared_ptr<DspAudioSink> sink):handshake(image.receive_startup_replies),boot_mode(mode),capture_audio(capture),audio_sink(std::move(sink)) {
        dsp.SetReferenceTransmitProfile(reference_transmit);
        dsp.Reset(); // Pinned register/peripheral construction; raw zero isn't provenance.
        if(reset==DspProbeReset::ReferenceZeroData)
            std::fill(source.begin()+kDsp1BankBytes,source.end(),
                      static_cast<std::uint8_t>(DspByteSource::ReferenceDataReset));
        auto load=[&](const Dsp1Bank& bank,std::size_t base) {
            for(std::size_t i=0;i<kDsp1BankBytes;++i) if(bank.known_mask()[i]) {
                memory[base+i]=bank.storage()[i];
                source[base+i]=static_cast<std::uint8_t>(DspByteSource::Image);
            }
        };
        load(image.program,0);load(image.data,kDsp1BankBytes);
        state.known_bytes=static_cast<std::uint32_t>(std::count_if(source.begin(),source.end(),[](auto s){return s!=0;}));
        ram->known_bytes=state.known_bytes;
        dsp.SetMemoryAccessCallback([this](std::uint32_t address,bool write) {
            if(address>memory.size()-2) {
                Fault(DspProbeFault::AddressRange,"DSP backing access outside SRAM",address);
                throw std::runtime_error("DSP backing range");
            }
            if(write) {
                if(!ram->Mark(address,2,static_cast<std::uint8_t>(DspByteSource::FirmwareWrite))) {
                    Fault(DspProbeFault::Backend,"DSP SRAM epoch limit or sealed storage");
                    throw std::runtime_error("DSP SRAM write failed");
                }
                state.known_bytes=ram->known_bytes;
                ++state.written_words; // raw 16-bit store follows, with no allocation/throw.
            } else {
                if(!source[address] || !source[address+1]) {
                    Fault(DspProbeFault::UnknownSram,"DSP firmware attempted an unknown SRAM read",
                          source[address]?address+1:address);
                    throw std::runtime_error("unknown SRAM read");
                }
                ++state.read_words;
            }
        });
        auto external=[this](std::uint32_t address) {
            Fault(DspProbeFault::ExternalMemory,"DSP external-memory bus is not connected",address);
            throw std::runtime_error("DSP external-memory access");
        };
        dsp.SetAHBMCallback({
            [external](auto a)->std::uint8_t{external(a);return 0;},[external](auto a,auto){external(a);},
            [external](auto a)->std::uint16_t{external(a);return 0;},[external](auto a,auto){external(a);},
            [external](auto a)->std::uint32_t{external(a);return 0;},[external](auto a,auto){external(a);}});
        dsp.SetAudioCaptureCallback([this](auto samples,auto fifo_mask) {
            if ((!live && boot_mode!=DspBootMode::ReferenceSlice) || !capture_audio) {
                Fault(DspProbeFault::Audio,"DSP audio output requires explicit bounded capture");
                throw std::runtime_error("DSP audio output has no selected consumer");
            }
            if (audio_count == audio.size() && !audio_sink) {
                Fault(DspProbeFault::Audio,"DSP audio capture capacity exhausted; no samples silently dropped");
                throw std::runtime_error("DSP audio capture full");
            }
            const DspCapturedAudioFrame frame{samples,fifo_mask,state.attempted_steps};
            if(audio_total==std::numeric_limits<std::uint64_t>::max() ||
               (audio_sink && !audio_sink->Write(frame))) {
                Fault(DspProbeFault::Audio,"DSP audio sink write failed; partial external output may remain");
                throw std::runtime_error("DSP audio sink failed");
            }
            ++audio_total;
            if(audio_count<audio.size())audio[audio_count++]=frame;
            // Beyond the prefix, the sink owns every frame. Without a sink the
            // original finite-capture stop is unchanged; nothing is discarded.

        },reference_silence);
    }
    void Fault(DspProbeFault kind,const char* text) noexcept {
        if(state.state==DspProbeState::Fault)return;
        state.state=DspProbeState::Fault;state.fault=kind;ram->sealed=true;
        std::snprintf(state.error.data(),state.error.size(),"%s",text);
    }
    void Fault(DspProbeFault kind,const char* text,std::uint32_t address) noexcept {
        if(state.state==DspProbeState::Fault)return;
        Fault(kind,text);state.has_fault_address=true;state.fault_address=address;
    }
    DspMailboxRead TryReceive(std::uint8_t index,std::uint16_t& word) noexcept override {
        if(state.state==DspProbeState::Fault || index>=3)return DspMailboxRead::Failed;
        try {
            if(!dsp.RecvDataIsReady(index))return DspMailboxRead::Empty;
            if(state.reply_count==history.size()) {
                Fault(DspProbeFault::MailboxLimit,"DSP diagnostic mailbox history capacity exhausted");
                return DspMailboxRead::Failed;
            }
            word=dsp.RecvData(index);
            history[state.reply_count++]={index,word,state.completed_steps};
            return DspMailboxRead::Received;
        } catch(const std::exception& error) {Fault(DspProbeFault::Backend,error.what());}
          catch(...) {Fault(DspProbeFault::Backend,"unclassified DSP mailbox exception");}
        return DspMailboxRead::Failed;
    }
    bool Poll() noexcept {
        auto result=handshake.Poll(*this,256);
        if(result==DspBootPoll::ProtocolComplete) {
            state.state=DspProbeState::ProtocolComplete;
            state.has_pipe_base=true;state.pipe_base=*handshake.pipe_base_words();return false;
        }
        if(result==DspBootPoll::BackendFailed) {Fault(DspProbeFault::Backend,"DSP mailbox failed");return false;}
        return state.state!=DspProbeState::Fault;
    }
};
DspExecutionProbe::DspExecutionProbe(std::unique_ptr<Impl> p):impl_(std::move(p)){}
DspExecutionProbe::~DspExecutionProbe()=default;
std::unique_ptr<DspExecutionProbe> DspExecutionProbe::Create(const Dsp1Image& image,
                                                           DspProbeReset reset,const char*& error,bool capture_audio,bool reference_silence,bool reference_transmit,DspBootMode boot_mode,std::shared_ptr<DspAudioSink> sink) noexcept {
    error=nullptr;
    if(sink && !capture_audio) {error="DSP audio sink requires explicit capture";return {};}
    if(boot_mode!=DspBootMode::Immediate && boot_mode!=DspBootMode::ReferenceSlice) {
        error="invalid DSP boot polling mode";return {};
    }
    if(reference_silence && !capture_audio) { error="reference silence requires audio capture";return {}; }
    if(reset!=DspProbeReset::KnownOnly && reset!=DspProbeReset::ReferenceZeroData) {
        error="invalid DSP probe reset policy";return {};
    }
    if(image.special.required) {
        if(image.special.type!=Dsp1MemoryType::ProgramA && image.special.type!=Dsp1MemoryType::ProgramB &&
           image.special.type!=Dsp1MemoryType::Data) {
            error="invalid DSP special-segment memory type";return {};
        }
        const auto& bank=image.special.type==Dsp1MemoryType::Data?image.data:image.program;
        const auto at=std::uint64_t(image.special.target_words)*2;
        if(at>kDsp1BankBytes || !bank.IsKnown(static_cast<std::uint32_t>(at),image.special.bytes)) {
            error="DSP special segment must be explicitly resolved before execution";return {};
        }
    }
    try {
        return std::unique_ptr<DspExecutionProbe>(new DspExecutionProbe(std::make_unique<Impl>(image,reset,capture_audio,reference_silence,reference_transmit,boot_mode,std::move(sink))));
    }catch(const std::bad_alloc&){error="host allocation failed creating DSP execution probe";}
     catch(...){error="DSP interpreter construction failed";}
    return {};
}
DspProbeState DspExecutionProbe::Advance(std::uint32_t budget) noexcept {
    auto& p=*impl_;auto& s=p.state;
    if(!budget || s.state!=DspProbeState::Paused)return s.state;
    const auto limit=std::min(budget,kMaxStepsPerCall);
    try {
        for(std::uint32_t i=0;i<limit;++i) {
            if(!p.boot_remaining) {
                if(!p.Poll())break;
                // The reference checks mailbox readiness only between complete
                // executor slices. Returning the fourth word mid-slice can expose
                // an uninitialized firmware callback to the next ARM message.
                // Partial host budgets preserve this boundary; never add a guessed delay.
                p.boot_remaining=p.boot_mode==DspBootMode::ReferenceSlice?kReferenceBootSlice:1;
            }
            if(s.attempted_steps==std::numeric_limits<std::uint64_t>::max()) {
                p.Fault(DspProbeFault::Backend,"DSP step counter exhausted");break;
            }
            s.pc_before=p.dsp.GetRegisterState().pc;++s.attempted_steps;
            p.dsp.Run(1);++s.completed_steps;--p.boot_remaining;
            s.pc_after=p.dsp.GetRegisterState().pc;
        }
        if(s.state==DspProbeState::Paused && !p.boot_remaining)p.Poll();
    }catch(const std::exception& error){p.Fault(DspProbeFault::Backend,error.what());}
     catch(...){p.Fault(DspProbeFault::Backend,"unclassified DSP interpreter exception");}
    s.pc_after=p.dsp.GetRegisterState().pc;
    return s.state;
}
std::uint16_t DspExecutionProbe::TakeLiveInterrupts() noexcept {
    const auto bits=impl_->live_pending;impl_->live_pending=0;return bits;
}
std::uint64_t DspExecutionProbe::live_notifications() const noexcept {return impl_->live_notification_count;}
std::shared_ptr<DeviceMemory> DspExecutionProbe::data_backing() const noexcept {return impl_->ram;}
bool DspExecutionProbe::SetSemaphore(std::uint16_t bits) noexcept {
    if (!impl_->live || impl_->state.state != DspProbeState::ProtocolComplete) return false;
    try { impl_->dsp.SetSemaphore(bits); return true; }
    catch (const std::exception& e) { impl_->Fault(DspProbeFault::Backend, e.what()); }
    catch (...) { impl_->Fault(DspProbeFault::Backend, "DSP semaphore exception"); }
    return false;
}
bool DspExecutionProbe::CanSend(std::uint8_t index) const noexcept {
    return index<3 && impl_->state.state==DspProbeState::ProtocolComplete && impl_->dsp.SendDataIsEmpty(index);
}
bool DspExecutionProbe::Send(std::uint8_t index,std::uint16_t word) noexcept {
    if(!CanSend(index))return false;
    try{impl_->dsp.SendData(index,word);return true;}
    catch(const std::exception& e){impl_->Fault(DspProbeFault::Backend,e.what());}
    catch(...){impl_->Fault(DspProbeFault::Backend,"DSP send exception");}
    return false;
}
bool DspExecutionProbe::ContinueLive(std::uint32_t steps) noexcept {
    auto& p=*impl_;auto& s=p.state;
    if(s.state!=DspProbeState::ProtocolComplete || steps>kMaxStepsPerCall)return false;
    try {
        if(!p.live) {
            // Notifications are generated only by future firmware peripheral writes.
            // A pipe IRQ requires BOTH the actual reg2 word and semaphore bit0x8000.
            p.dsp.SetRecvDataHandler(0,[ptr=&p]{ptr->Emit(0);});
            p.dsp.SetRecvDataHandler(1,[ptr=&p]{ptr->Emit(1);});
            p.dsp.SetRecvDataHandler(2,[ptr=&p]{ptr->PipeEvent(true);});
            p.dsp.SetSemaphoreHandler([ptr=&p]{ptr->PipeEvent(false);});
            p.live=true;
        }
        for(std::uint32_t i=0;i<steps;++i) {
            if(s.attempted_steps==std::numeric_limits<std::uint64_t>::max()) {
                p.Fault(DspProbeFault::Backend,"DSP live step counter exhausted");break;
            }
            s.pc_before=p.dsp.GetRegisterState().pc;++s.attempted_steps;
            p.dsp.Run(1);++s.completed_steps;s.pc_after=p.dsp.GetRegisterState().pc;
        }
    } catch(const std::exception& e){p.Fault(DspProbeFault::Backend,e.what());}
      catch(...){p.Fault(DspProbeFault::Backend,"DSP live execution exception");}
    s.pc_after=p.dsp.GetRegisterState().pc;
    return s.state!=DspProbeState::Fault;
}
const DspProbeSummary& DspExecutionProbe::summary() const noexcept{
    impl_->state.known_bytes=impl_->ram->known_bytes;return impl_->state;
}
std::span<const std::uint8_t> DspExecutionProbe::memory() const noexcept{return impl_->memory;}
std::span<const std::uint8_t> DspExecutionProbe::provenance() const noexcept{return impl_->source;}
std::span<const DspProbeReply> DspExecutionProbe::replies() const noexcept{return std::span(impl_->history).first(impl_->state.reply_count);}
} // namespace lego::ctr

namespace lego::ctr {
std::span<const DspCapturedAudioFrame> DspExecutionProbe::captured_audio() const noexcept {
    return std::span(impl_->audio).first(impl_->audio_count);
}
}

namespace lego::ctr {
std::uint64_t DspExecutionProbe::emitted_audio_frames() const noexcept { return impl_->audio_total; }
}
