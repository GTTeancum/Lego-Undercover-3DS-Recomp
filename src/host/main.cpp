#include "runtime/ctr_runner.h"
#include "host/sha256.h"
#include "lego_aot_generated.h"
#include <charconv>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
namespace ctr = lego::ctr;
namespace a32 = oot3d::recomp::a32;
constexpr std::string_view kCodeHash =
    "5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f";

const char* StopName(ctr::RunnerStopReason reason) {
    using R = ctr::RunnerStopReason;
    switch (reason) {
    case R::ProcessExited: return "ProcessExited";
    case R::WaitingNoRunnableThread: return "WaitingNoRunnableThread";
    case R::UnsupportedSvc: return "UnsupportedSvc";
    case R::UnsupportedIpc: return "UnsupportedIpc";
    case R::BlockLimit: return "BlockLimit";
    case R::MissingBlock: return "MissingBlock";
    case R::MemoryFault: return "MemoryFault";
    case R::Unsupported: return "Unsupported";
    case R::Fallback: return "Fallback";
    case R::HostEventLimit: return "HostEventLimit";
    case R::OtherExit: return "OtherExit";
    }
    return "Unknown";
}
std::uint32_t PositiveNumber(std::string_view arg) {
    std::uint32_t result=0;
    const auto parsed=std::from_chars(arg.data(),arg.data()+arg.size(),result);
    if (parsed.ec!=std::errc{} || parsed.ptr!=arg.data()+arg.size() || result==0)
        throw std::runtime_error("limits must be positive decimal integers");
    return result;
}
void ValidateRegistry(const a32::Registry& registry, std::span<const std::uint8_t> code) {
    if (registry.shard_count!=599 || !registry.shards)
        throw std::runtime_error("wrong AOT registry page count");
    std::uint64_t slots=0,blocks=0;
    for (std::uint32_t i=0; i<registry.shard_count; ++i) {
        const auto& shard=registry.shards[i];
        const auto base=ctr::kTextBase+i*ctr::kPageSize;
        if (shard.first_pc!=base || shard.last_pc!=base+ctr::kPageSize-4 ||
            (shard.block_count && !shard.blocks))
            throw std::runtime_error("invalid AOT shard bounds");
        std::uint64_t previous_end=base;
        for (std::uint32_t b=0; b<shard.block_count; ++b) {
            const auto& block=shard.blocks[b];
            const std::uint64_t end=std::uint64_t(block.pc)+std::uint64_t(block.op_count)*4;
            if (!block.ops || !block.op_count || (block.pc&3) || block.pc<previous_end ||
                end>std::uint64_t(base)+ctr::kPageSize)
                throw std::runtime_error("overlapping or out-of-page AOT block");
            previous_end=end;
            for (std::uint32_t op=0; op<block.op_count; ++op) {
                const auto offset=block.pc-ctr::kTextBase+op*4;
                const std::uint32_t raw=std::uint32_t(code[offset]) |
                    std::uint32_t(code[offset+1])<<8 | std::uint32_t(code[offset+2])<<16 |
                    std::uint32_t(code[offset+3])<<24;
                if (raw!=block.ops[op].raw)
                    throw std::runtime_error("AOT instruction differs from verified code.bin");
            }
            slots+=block.op_count;
            ++blocks;
        }
    }
    std::cout<<"registry_pages="<<registry.shard_count<<" blocks="<<blocks
             <<" instruction_words_verified="<<slots<<'\n';
}
}  // namespace

int main(int argc,char** argv) {
    try {
        if (argc<2 || std::string_view(argv[1])=="--help") {
            std::cout<<"LEGOChaseNative code.bin [--block-limit N] [--host-event-limit N] [--rtc-ms-since-1900 N] [--shared-extdata-root DIR] [--ptm-step-mode empty]\n"
                       "Headless reconstruction diagnostic; not a playable release.\n";
            return argc<2 ? 2 : 0;
        }
        std::filesystem::path shared_extdata_root;
        auto ptm_step_mode = ctr::PtmStepMode::Unconfigured;
        std::uint32_t block_limit=1000000,event_limit=4096;
        std::uint64_t rtc_epoch_ms=ctr::kDefaultRtcMsSince1900;
        for (int i=2; i<argc; i+=2) {
            if (i+1==argc) throw std::runtime_error("missing option value");
            const std::string_view option(argv[i]);
            if (option=="--block-limit") block_limit=PositiveNumber(argv[i+1]);
            else if (option=="--host-event-limit") event_limit=PositiveNumber(argv[i+1]);
            else if (option=="--shared-extdata-root") {
                if (!argv[i+1][0]) throw std::runtime_error("empty shared-extdata root");
                shared_extdata_root=argv[i+1];
            }
            else if (option=="--ptm-step-mode") {
                if (std::string_view(argv[i+1]) != "empty")
                    throw std::runtime_error("PTM step mode must be empty (explicit new profile)");
                ptm_step_mode = ctr::PtmStepMode::EmptyHistory;
            }
            else if (option=="--rtc-ms-since-1900") {
                const std::string_view value(argv[i+1]);
                const auto parsed=std::from_chars(value.data(),value.data()+value.size(),rtc_epoch_ms);
                if (parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size() ||
                    !ctr::ValidRtcEpoch(rtc_epoch_ms))
                    throw std::runtime_error("invalid RTC epoch (decimal milliseconds since 1900; minimum 2000-01-01)");
            }
            else throw std::runtime_error("unknown option");
        }
        std::ifstream input(argv[1],std::ios::binary|std::ios::ate);
        if (!input || input.tellg()!=std::streamoff(ctr::kPreparedCodeBytes))
            throw std::runtime_error("code.bin missing or wrong size");
        std::vector<std::uint8_t> code(ctr::kPreparedCodeBytes);
        input.seekg(0);
        if (!input.read(reinterpret_cast<char*>(code.data()),code.size()))
            throw std::runtime_error("could not read complete code.bin");
        const auto digest=lego::host::Sha256(code);
        if (digest!=kCodeHash) throw std::runtime_error("wrong code.bin SHA-256: "+digest);
        std::cout<<"code_sha256="<<digest<<'\n';
        const auto& registry=oot3d::recomp::GetA32GeneratedRegistry();
        ValidateRegistry(registry,code);
        ctr::GuestMemory memory;
        if (!memory.LoadLegoCodeImage(code)) throw std::runtime_error("image mapping failed");
        ctr::Kernel kernel;
        ctr::NativeRunner runner(registry,memory,kernel,rtc_epoch_ms,shared_extdata_root,ptm_step_mode);
        if (!shared_extdata_root.empty())
            std::cout << "shared_extdata_root=" << shared_extdata_root.generic_string() << '\n';
        if (ptm_step_mode == ctr::PtmStepMode::EmptyHistory)
            std::cout << "ptm_step_source=explicit_empty_history total_steps=0 sensor_input=none\n";
        if (!runner.InitializeMainThread()) throw std::runtime_error("main thread setup failed");
        std::cout << "rtc_epoch_ms_since_1900=" << rtc_epoch_ms
                  << " guest_time_source=kernel_ns\n";
        const auto result=runner.Run(block_limit,event_limit);
        const auto& state=runner.live_state();
        std::uint32_t clock_counter=0, clock_fault=0;
        std::uint64_t clock_date=0, clock_tick=0;
        if (memory.Read32(ctr::kSharedPageBase,&clock_counter)) {
            const auto record=ctr::kSharedPageBase+ctr::kSharedClockSnapshot0Offset+
                              (clock_counter&1U)*ctr::kSharedClockSnapshotBytes;
            if (memory.Read64(record,&clock_date,&clock_fault) &&
                memory.Read64(record+8,&clock_tick,&clock_fault))
                std::cout << "shared_clock_counter=" << clock_counter
                          << " snapshot_ms_since_1900=" << clock_date
                          << " snapshot_tick=" << clock_tick
                          << " guest_now_ns=" << kernel.now_ns() << '\n';
        }
        std::cout<<"stop="<<StopName(result.reason)<<" pc=0x"<<std::hex
                 <<std::setw(8)<<std::setfill('0')<<result.exit.pc
                 <<" detail=0x"<<std::setw(8)<<result.exit.detail
                 <<" thread="<<std::dec<<result.thread_id
                 <<" dispatch_rounds="<<result.dispatch_rounds<<'\n';
        for (int i=0; i<16; ++i)
            std::cout<<'r'<<std::dec<<i<<"=0x"<<std::hex<<std::setw(8)
                     <<state.r[i]<<(i%4==3 ? '\n' : ' ');
        std::cout<<"cpsr=0x"<<std::setw(8)<<state.cpsr<<" fpscr=0x"
                 <<std::setw(8)<<state.fpscr<<" tls=0x"<<state.thread_pointer<<'\n';
        std::cout << "last_ipc_session=" << runner.ipc().last_session_name()
                  << " requested_service=" << runner.ipc().last_lookup_name()
                  << " request_header=0x" << std::setw(8)
                  << runner.ipc().last_request()[0] << '\n';
        if (!runner.ipc().last_host_error().empty())
            std::cout << "host_ipc_error=" << runner.ipc().last_host_error() << '\n';
        const auto cb=kernel.current_thread()->tls_address+ctr::kIpcCommandBufferOffset;
        std::cout<<"ipc_words=";
        for (unsigned i=0;i<8;++i) {
            std::uint32_t word=0;
            if (memory.Read32(cb+i*4,&word)) std::cout<<' '<<std::setw(8)<<word;
            else std::cout<<" unreadable";
        }
        std::cout<<'\n';
        std::cout<<"ipc_static_buffer0=";
        for (unsigned i=0;i<2;++i) {
            std::uint32_t word=0;
            if (memory.Read32(cb+0x100+i*4,&word)) std::cout<<' '<<std::setw(8)<<word;
            else std::cout<<" unreadable";
        }
        std::cout<<'\n';
        if (result.reason==ctr::RunnerStopReason::ProcessExited) return 0;
        if (result.reason==ctr::RunnerStopReason::BlockLimit ||
            result.reason==ctr::RunnerStopReason::HostEventLimit) return 4;
        return 3; // Explicit diagnostic stop, never a claimed successful boot.
    } catch (const std::exception& error) {
        std::cerr<<"ERROR: "<<error.what()<<'\n';
        return 2;
    }
}
