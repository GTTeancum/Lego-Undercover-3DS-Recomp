#include "services/dsp_boot_handshake.h"
#include <algorithm>
#include <limits>
namespace lego::ctr {
std::optional<std::uint8_t> DspBootHandshake::expected_register() const noexcept {
    if (failed_ || phase_==4) return std::nullopt;
    return static_cast<std::uint8_t>(phase_<3?phase_:2);
}
DspBootPoll DspBootHandshake::Poll(DspBootMailbox& backend,std::uint32_t read_budget) noexcept {
    if (failed_) return DspBootPoll::BackendFailed;
    if (phase_==4) return DspBootPoll::ProtocolComplete;
    const auto limit=std::min(read_budget,kMaxReadsPerPoll);
    for (std::uint32_t n=0;n<limit;++n) {
        if (received_==std::numeric_limits<std::uint64_t>::max()) {
            failed_=true;return DspBootPoll::BackendFailed;
        }
        std::uint16_t word{};
        const auto status=backend.TryReceive(*expected_register(),word);
        if (status==DspMailboxRead::Empty) return DspBootPoll::NeedsExecution;
        if (status!=DspMailboxRead::Received) {failed_=true;return DspBootPoll::BackendFailed;}
        ++received_;
        if (phase_<3) {
            if (word==1) ++phase_;
            else ++discarded_; // pin discards non-one replies, it does not accept them
        } else {
            // The pipe address is the NEXT register-2 word, not its ready value.
            // This captures the actual word; pipe layout validation is backend work.
            pipe_base_=word;phase_=4;return DspBootPoll::ProtocolComplete;
        }
    }
    return DspBootPoll::ReadBudget;
}
} // namespace lego::ctr
