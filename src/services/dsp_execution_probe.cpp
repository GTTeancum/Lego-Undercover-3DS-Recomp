#include "services/dsp_execution_probe.h"
#include "services/dsp_boot_handshake.h"
#include "teakra/teakra.h"
#include "teakra/impl/register.h"
#include <algorithm>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <utility>

namespace lego::ctr {
struct DspExecutionProbe::Impl final : DspBootMailbox {
    std::vector<std::uint8_t> memory = std::vector<std::uint8_t>(0x80000,0);
    std::vector<std::uint8_t> source = std::vector<std::uint8_t>(0x80000,0);
    Teakra::Teakra dsp{{memory.data()}};
    DspBootHandshake handshake;
    DspProbeSummary state{};
    std::array<DspProbeReply,256> history{};

    Impl(const Dsp1Image& image,DspProbeReset reset):handshake(image.receive_startup_replies) {
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
        dsp.SetMemoryAccessCallback([this](std::uint32_t address,bool write) {
            if(address>memory.size()-2) {
                Fault(DspProbeFault::AddressRange,"DSP backing access outside SRAM",address);
                throw std::runtime_error("DSP backing range");
            }
            if(write) {
                for(unsigned i=0;i<2;++i) {
                    if(!source[address+i])++state.known_bytes;
                    source[address+i]=static_cast<std::uint8_t>(DspByteSource::FirmwareWrite);
                }
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
        dsp.SetAudioCallback([this](auto) {
            Fault(DspProbeFault::Audio,"DSP audio output requires a live device contract");
            throw std::runtime_error("DSP audio output before live integration");
        });
    }
    void Fault(DspProbeFault kind,const char* text) noexcept {
        if(state.state==DspProbeState::Fault)return;
        state.state=DspProbeState::Fault;state.fault=kind;
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
                                                           DspProbeReset reset,const char*& error) noexcept {
    error=nullptr;
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
        return std::unique_ptr<DspExecutionProbe>(new DspExecutionProbe(std::make_unique<Impl>(image,reset)));
    }catch(const std::bad_alloc&){error="host allocation failed creating DSP execution probe";}
     catch(...){error="DSP interpreter construction failed";}
    return {};
}
DspProbeState DspExecutionProbe::Advance(std::uint32_t budget) noexcept {
    auto& p=*impl_;auto& s=p.state;
    if(!budget || s.state!=DspProbeState::Paused)return s.state;
    const auto limit=std::min(budget,kMaxStepsPerCall);
    try {
        for(std::uint32_t i=0;i<limit && p.Poll();++i) {
            if(s.attempted_steps==std::numeric_limits<std::uint64_t>::max()) {
                p.Fault(DspProbeFault::Backend,"DSP step counter exhausted");break;
            }
            s.pc_before=p.dsp.GetRegisterState().pc;++s.attempted_steps;
            p.dsp.Run(1);++s.completed_steps;
            s.pc_after=p.dsp.GetRegisterState().pc;
        }
        if(s.state==DspProbeState::Paused)p.Poll();
    }catch(const std::exception& error){p.Fault(DspProbeFault::Backend,error.what());}
     catch(...){p.Fault(DspProbeFault::Backend,"unclassified DSP interpreter exception");}
    s.pc_after=p.dsp.GetRegisterState().pc;
    return s.state;
}
const DspProbeSummary& DspExecutionProbe::summary() const noexcept{return impl_->state;}
std::span<const std::uint8_t> DspExecutionProbe::memory() const noexcept{return impl_->memory;}
std::span<const std::uint8_t> DspExecutionProbe::provenance() const noexcept{return impl_->source;}
std::span<const DspProbeReply> DspExecutionProbe::replies() const noexcept{return std::span(impl_->history).first(impl_->state.reply_count);}
} // namespace lego::ctr
