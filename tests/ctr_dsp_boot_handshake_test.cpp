#include "services/dsp_boot_handshake.h"
#include <array>
#include <deque>
#include <cstdlib>
#include <iostream>
#include <limits>
namespace {
using namespace lego::ctr;
int failures=0;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": " #x "\n";++failures;}}while(0)
// Explicit protocol fixture, NOT DSP instructions or a firmware emulator.
struct Mailbox final : DspBootMailbox {
    std::array<std::deque<std::uint16_t>,3> words;
    unsigned calls{};bool error{};std::array<unsigned,3> reads{};
    DspMailboxRead TryReceive(std::uint8_t index,std::uint16_t& word) noexcept override {
        ++calls;if (error || index>=3) return DspMailboxRead::Failed;
        if (words[index].empty()) return DspMailboxRead::Empty;
        ++reads[index];word=words[index].front();words[index].pop_front();return DspMailboxRead::Received;
    }
};
void RealWordsRequiredAndPipeIsSeparate() {
    Mailbox m;DspBootHandshake h(true);
    CHECK(h.Poll(m,0)==DspBootPoll::ReadBudget && m.calls==0 && !h.pipe_base_words());
    m.words[2]={1,0xCDEF};m.words[1]={1};
    for (int i=0;i<4;++i) CHECK(h.Poll(m,100)==DspBootPoll::NeedsExecution && h.expected_register()==0);
    CHECK(m.reads[1]==0 && m.reads[2]==0 && !h.pipe_base_words());
    m.words[0]={0,0x8000,1};
    CHECK(h.Poll(m,1)==DspBootPoll::ReadBudget && h.discarded_words()==1);
    CHECK(h.Poll(m,1)==DspBootPoll::ReadBudget && h.discarded_words()==2);
    CHECK(h.Poll(m,3)==DspBootPoll::ReadBudget && h.expected_register()==2 && !h.pipe_base_words());
    CHECK(m.reads[2]==1 && h.received_words()==5);
    CHECK(h.Poll(m,1)==DspBootPoll::ProtocolComplete && h.pipe_base_words()==0xCDEF);
    CHECK(h.received_words()==6 && !h.expected_register());const auto calls=m.calls;
    CHECK(h.Poll(m,100)==DspBootPoll::ProtocolComplete && h.Poll(m,0)==DspBootPoll::ProtocolComplete && m.calls==calls);
}
void NoReadyBitAndAllPipeWords() {
    for (unsigned word=0;word<=65535;++word) {
        Mailbox m;m.words[2].push_back(static_cast<std::uint16_t>(word));DspBootHandshake h(false);
        CHECK(h.expected_register()==2 && h.Poll(m,1)==DspBootPoll::ProtocolComplete);
        CHECK(h.pipe_base_words()==word && h.received_words()==1 && h.discarded_words()==0);
    }
}
void BudgetsFailuresAndContinuation() {
    struct Infinite final : DspBootMailbox {
        unsigned calls{};
        DspMailboxRead TryReceive(std::uint8_t,std::uint16_t& word) noexcept override {++calls;word=0;return DspMailboxRead::Received;}
    } m;
    DspBootHandshake h(true);
    CHECK(h.Poll(m,std::numeric_limits<std::uint32_t>::max())==DspBootPoll::ReadBudget);
    CHECK(m.calls==256 && h.received_words()==256 && h.discarded_words()==256 && h.expected_register()==0);
    Mailbox fail;fail.words[0]={1};
    CHECK(h.Poll(fail,1)==DspBootPoll::ReadBudget && h.expected_register()==1);
    fail.error=true;CHECK(h.Poll(fail,2)==DspBootPoll::BackendFailed && h.failed() && !h.pipe_base_words());
    const auto calls=fail.calls;fail.error=false;fail.words[1]={1};
    CHECK(h.Poll(fail,100)==DspBootPoll::BackendFailed && fail.calls==calls);
    Mailbox a,b;a.words[0]=b.words[0]={12,1};a.words[1]=b.words[1]={0,0xEEEE,1};a.words[2]=b.words[2]={0,1,0x3456};
    DspBootHandshake x(true),y(true);CHECK(x.Poll(a,100)==DspBootPoll::ProtocolComplete);
    for (int i=0;i<7;++i) CHECK(y.Poll(b,1)==DspBootPoll::ReadBudget);
    CHECK(y.Poll(b,1)==DspBootPoll::ProtocolComplete);
    CHECK(x.received_words()==y.received_words() && x.discarded_words()==y.discarded_words() && x.pipe_base_words()==y.pipe_base_words());
}
}
int main(){RealWordsRequiredAndPipeIsSeparate();NoReadyBitAndAllPipeWords();BudgetsFailuresAndContinuation();if(failures)return EXIT_FAILURE;
std::cout<<"PASS: bounded ordered real-word startup protocol, no ready/pipe fabrication; synthetic mailbox fixtures only\n";}
