#include "runtime/ctr_runner.h"
#include "host/sha256.h"
#include "host/launch_header.h"
#include "host/dsp_special_input.h"
#include "host/dsp_audio_file.h"
#include "services/dsp_discovery_service.h"
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
    case R::UnsupportedDisplayEvent: return "UnsupportedDisplayEvent";
    case R::UnsupportedCpuExecution: return "UnsupportedCpuExecution";
    case R::UnsupportedDspEvent: return "UnsupportedDspEvent";
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
            std::cout<<"LEGOChaseNative code.bin [--block-limit N] [--host-event-limit N] [--rtc-ms-since-1900 N] [--shared-extdata-root DIR] [--ptm-step-mode empty] [--romfs FILE] [--gpu-vram-mode reference-zero] [--display-clock-mode reference-idle] [--cfg-profile reference-stereo] [--cfg-sound-mode mono|stereo|surround] [--cpu-mode diagnostic-dual] [--exheader FILE] [--dsp-special-profile empty-config | --dsp-special-block FILE] [--dsp-executor guarded-teakra|live-teakra] [--dsp-reset-profile reference-zero-data] [--dsp-probe-steps N] [--dsp-audio-mode capture|capture-reference-silence] [--dsp-audio-file NEW_FILE] [--dsp-transmit-profile reference-stereo] [--dsp-boot-mode reference-slice]\n"
                       "Headless reconstruction diagnostic; not a playable release.\n";
            return argc<2 ? 2 : 0;
        }
        std::filesystem::path shared_extdata_root, romfs_path, exheader_path;
        ctr::DspSpecialConfig dsp_config;
        ctr::DspProbeOptions dsp_probe;
        bool dsp_executor_selected=false,dsp_reset_selected=false,dsp_steps_selected=false,dsp_audio_selected=false;
        std::filesystem::path dsp_special_path, dsp_audio_path;
        bool dsp_audio_file_selected=false;
        bool dsp_special_selected=false;
        auto vram_mode=ctr::GpuVramMode::Unconfigured;
        auto display_mode=ctr::DisplayClockMode::Disabled;
        auto cfg_profile=ctr::CfgProfile::Unconfigured;
        auto cfg_sound_mode=ctr::CfgSoundMode::Unconfigured;
        std::string_view cfg_sound_name;
        auto cpu_mode=ctr::CpuExecutionMode::Strict;
        auto ptm_step_mode = ctr::PtmStepMode::Unconfigured;
        std::uint32_t block_limit=1000000,event_limit=4096;
        std::uint64_t rtc_epoch_ms=ctr::kDefaultRtcMsSince1900;
        for (int i=2; i<argc; i+=2) {
            if (i+1==argc) throw std::runtime_error("missing option value");
            const std::string_view option(argv[i]);
            if(option=="--cpu-mode") {
                if(std::string_view(argv[i+1])!="diagnostic-dual")
                    throw std::runtime_error("CPU mode must be diagnostic-dual (or omit the option)");
                cpu_mode=ctr::CpuExecutionMode::DiagnosticDual;
            }
            else if (option=="--dsp-executor") {
                if(dsp_executor_selected || (std::string_view(argv[i+1])!="guarded-teakra" && std::string_view(argv[i+1])!="live-teakra"))
                    throw std::runtime_error("select DSP executor guarded-teakra or live-teakra once, or omit it");
                dsp_executor_selected=true;dsp_probe.enabled=true;
                dsp_probe.live=std::string_view(argv[i+1])=="live-teakra";
            }
            else if (option=="--dsp-boot-mode") {
                if(dsp_probe.boot_mode!=ctr::DspBootMode::Immediate || std::string_view(argv[i+1])!="reference-slice")
                    throw std::runtime_error("invalid or repeated DSP boot polling mode");
                dsp_probe.boot_mode=ctr::DspBootMode::ReferenceSlice;
            }
            else if (option=="--dsp-transmit-profile") {
                if(dsp_probe.reference_transmit || std::string_view(argv[i+1])!="reference-stereo")
                    throw std::runtime_error("select DSP transmit profile reference-stereo once, or omit it");
                dsp_probe.reference_transmit=true;
            }
            else if (option=="--dsp-audio-file") {
                if(dsp_audio_file_selected || !argv[i+1][0])
                    throw std::runtime_error("select one nonempty NEW DSP audio file path");
                dsp_audio_file_selected=true;dsp_audio_path=argv[i+1];
            }
            else if (option=="--dsp-audio-mode") {
                if(dsp_audio_selected || (std::string_view(argv[i+1])!="capture" && std::string_view(argv[i+1])!="capture-reference-silence"))
                    throw std::runtime_error("select DSP audio mode capture or capture-reference-silence once, or omit it");
                dsp_audio_selected=true;dsp_probe.capture_audio=true;
                dsp_probe.reference_audio_silence=std::string_view(argv[i+1])=="capture-reference-silence";
            }
            else if (option=="--dsp-reset-profile") {
                if(dsp_reset_selected || std::string_view(argv[i+1])!="reference-zero-data")
                    throw std::runtime_error("select DSP reset profile reference-zero-data once, or omit it");
                dsp_reset_selected=true;dsp_probe.reset=ctr::DspProbeReset::ReferenceZeroData;
            }
            else if (option=="--dsp-probe-steps") {
                if(dsp_steps_selected)throw std::runtime_error("select DSP probe step limit once");
                dsp_steps_selected=true;dsp_probe.steps=PositiveNumber(argv[i+1]);
                if(dsp_probe.steps>ctr::DspExecutionProbe::kMaxStepsPerCall)
                    throw std::runtime_error("DSP probe step limit exceeds host cap 100000");
            }
            else if (option=="--dsp-special-profile" || option=="--dsp-special-block") {
                if (dsp_special_selected) throw std::runtime_error("select DSP special configuration only once");
                dsp_special_selected=true;
                if (option=="--dsp-special-profile") {
                    if (std::string_view(argv[i+1])!="empty-config")
                        throw std::runtime_error("DSP special profile must be empty-config (or omit the option)");
                    dsp_config=ctr::DspSpecialConfig::EmptySystemConfig();
                } else {
                    if (!argv[i+1][0]) throw std::runtime_error("empty DSP special block path");
                    dsp_special_path=argv[i+1];
                }
            }
            else if(option=="--exheader") {
                if(!argv[i+1][0])throw std::runtime_error("empty ExHeader path");
                exheader_path=argv[i+1];
            }
            else if (option=="--cfg-profile") {
                if (std::string_view(argv[i+1])!="reference-stereo")
                    throw std::runtime_error("CFG profile must be reference-stereo (or omit the option)");
                cfg_profile=ctr::CfgProfile::ReferenceStereo;
            }
            else if (option=="--cfg-sound-mode") {
                if (cfg_sound_mode != ctr::CfgSoundMode::Unconfigured)
                    throw std::runtime_error("select CFG sound mode only once");
                cfg_sound_name=argv[i+1];
                if (cfg_sound_name=="mono") cfg_sound_mode=ctr::CfgSoundMode::Mono;
                else if (cfg_sound_name=="stereo") cfg_sound_mode=ctr::CfgSoundMode::Stereo;
                else if (cfg_sound_name=="surround") cfg_sound_mode=ctr::CfgSoundMode::Surround;
                else throw std::runtime_error("CFG sound mode must be mono, stereo or surround (or omit it)");
            }
            else if (option=="--display-clock-mode") {
                if (std::string_view(argv[i+1])!="reference-idle")
                    throw std::runtime_error("unsupported display clock mode (expected reference-idle)");
                display_mode=ctr::DisplayClockMode::ReferenceIdle;
            }
            else if (option=="--gpu-vram-mode") {
                if (std::string_view(argv[i+1])!="reference-zero")
                    throw std::runtime_error("GPU VRAM mode must be reference-zero (or omit the option)");
                vram_mode=ctr::GpuVramMode::ReferenceZero;
            }
            else if (option=="--romfs") {
                if (!argv[i+1][0]) throw std::runtime_error("empty RomFS path");
                romfs_path=argv[i+1];
            }
            else if (option=="--block-limit") block_limit=PositiveNumber(argv[i+1]);
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
        if(dsp_audio_file_selected && (!dsp_audio_selected || !dsp_probe.live ||
           !dsp_probe.reference_transmit || cpu_mode!=ctr::CpuExecutionMode::DiagnosticDual))
            throw std::runtime_error("DSP audio file requires explicit live capture, reference transmitter and diagnostic-dual CPU");
        if(dsp_audio_selected && !dsp_probe.live)
            throw std::runtime_error("DSP audio capture requires --dsp-executor live-teakra");
        if(dsp_probe.live && cpu_mode!=ctr::CpuExecutionMode::DiagnosticDual)
            throw std::runtime_error("live-teakra requires --cpu-mode diagnostic-dual");
        if((dsp_reset_selected || dsp_steps_selected) && !dsp_probe.enabled)
            throw std::runtime_error("DSP reset/step options require --dsp-executor guarded-teakra or live-teakra");
        if (!dsp_special_path.empty()) dsp_config=lego::host::ReadDspSpecialBlock(dsp_special_path);
        std::optional<lego::host::LaunchPolicy> launch;
        if(!exheader_path.empty()) {
            if(cpu_mode!=ctr::CpuExecutionMode::DiagnosticDual)
                throw std::runtime_error("--exheader requires --cpu-mode diagnostic-dual");
            std::ifstream header(exheader_path,std::ios::binary|std::ios::ate);
            if(!header || header.tellg()!=2048)throw std::runtime_error("ExHeader missing or wrong size");
            std::array<std::uint8_t,2048> bytes{};header.seekg(0);
            if(!header.read(reinterpret_cast<char*>(bytes.data()),bytes.size()))
                throw std::runtime_error("cannot read complete ExHeader");
            launch=lego::host::VerifiedLegoLaunchPolicy(bytes);
            if(!launch)throw std::runtime_error("ExHeader identity or launch policy mismatch");
            std::cout<<"exheader_sha256="<<lego::host::kLegoExHeaderSha256
                     <<" launch_mode=multi launch_maximum="<<launch->maximum_cpu
                     <<" recovered_title_header=true hardware_cycles=false\n";
        }
        if(dsp_probe.boot_mode!=ctr::DspBootMode::Immediate && (!dsp_probe.enabled || !dsp_probe.live))
            throw std::runtime_error("DSP reference-slice boot requires live-teakra");
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
        if(launch && !kernel.ConfigureCpuExecution(cpu_mode,launch->maximum_cpu))
            throw std::runtime_error("launch CPU resource configuration failed");
        std::shared_ptr<const ctr::RomfsImage> romfs;
        if (!romfs_path.empty()) {
            romfs=ctr::RomfsImage::OpenVerified(romfs_path,ctr::kLegoRawRomfsBytes,
                                               ctr::kLegoRomfsViewOffset,ctr::kLegoRawRomfsSha256);
            std::cout << "romfs_sha256=" << romfs->sha256() << " raw_bytes=" << ctr::kLegoRawRomfsBytes
                      << " view_offset=" << ctr::kLegoRomfsViewOffset << " view_bytes=" << romfs->size() << '\n';
        }
        std::shared_ptr<lego::host::DspAudioFile> audio_file;
        if(dsp_audio_file_selected) {
            audio_file=lego::host::DspAudioFile::Open(dsp_audio_path);
            dsp_probe.audio_sink=audio_file;
        }
        ctr::NativeRunner runner(registry,memory,kernel,rtc_epoch_ms,shared_extdata_root,ptm_step_mode,romfs,vram_mode,display_mode,cfg_profile,cpu_mode,dsp_config,dsp_probe,cfg_sound_mode);
        if (!shared_extdata_root.empty())
            std::cout << "shared_extdata_root=" << shared_extdata_root.generic_string() << '\n';
        if (ptm_step_mode == ctr::PtmStepMode::EmptyHistory)
            std::cout << "ptm_step_source=explicit_empty_history total_steps="<<0<<" sensor_input=none\n";
        if (vram_mode==ctr::GpuVramMode::ReferenceZero)
            std::cout << "gpu_vram_source=pinned_hle_zero_initialization bytes=6291456 cpu_mapping=none\n";
        if (cfg_sound_mode!=ctr::CfgSoundMode::Unconfigured)
            std::cout<<"cfg_sound_mode="<<cfg_sound_name<<" value="<<unsigned(cfg_sound_mode)
                     <<" source=explicit_host_preference recovered_console_setting=false playback=unchanged\n";
        if (cfg_profile==ctr::CfgProfile::ReferenceStereo)
            std::cout << "cfg_profile=pinned_hle_stereo_default block=00050005 bytes=32 recovered_calibration=false\n";
        if (dsp_config.profile()==ctr::DspSpecialProfile::EmptySystemConfig)
            std::cout<<"dsp_special_profile=explicit_empty_system_config block=00070000 read=missing fallback=documented_zero recovered_calibration=false\n";
        else if (dsp_config.profile()==ctr::DspSpecialProfile::SuppliedBlock) {
            std::span<const std::uint8_t> block;(void)dsp_config.Read(block);
            std::cout<<"dsp_special_profile=host_supplied_block bytes=532 sha256="<<lego::host::Sha256(block)<<" authentication=not_asserted\n";
        }
        if (!runner.InitializeMainThread()) throw std::runtime_error("main thread setup failed");
        std::cout << "rtc_epoch_ms_since_1900=" << rtc_epoch_ms
                  << " guest_time_source=kernel_ns\n";
        if (display_mode==ctr::DisplayClockMode::ReferenceIdle)
            std::cout<<"display_clock="<<(cpu_mode==ctr::CpuExecutionMode::DiagnosticDual?"diagnostic_cpu_periodic":"reference_idle")
                     <<" frame_ticks="<<ctr::kDisplayPeriodTicks
                     <<" clock_hz="<<ctr::kArm11TicksPerSecond<<" presentation=none\n";
        if(cpu_mode==ctr::CpuExecutionMode::DiagnosticDual)
            std::cout<<"cpu_model=diagnostic_dual issue=one_recorded_A32_per_core_per_tick order=core0_then_core1 hardware_cycles=false\n";
        if(dsp_probe.enabled)
            std::cout<<"dsp_executor="<<(dsp_probe.live?"live_teakra host_probe_only=false data_reset=":"guarded_teakra host_probe_only=true data_reset=")
                     <<(dsp_probe.reset==ctr::DspProbeReset::ReferenceZeroData?"explicit_reference_zero":"known_image_only")
                     <<" program_gaps=unknown step_limit="<<dsp_probe.steps<<'\n';
        const auto result=runner.Run(block_limit,event_limit);
        if(audio_file) {
            const bool closed=audio_file->Close();
            std::cout<<"dsp_audio_file="<<dsp_audio_path.generic_string()<<" records="<<audio_file->frames()
                     <<" close_ok="<<closed<<" format=DSPAUD1 sample_rate=unasserted playback=none\n";
            if(!closed)throw std::runtime_error("DSP audio file write/close failed; partial output retained");
        }
        if(dsp_probe.boot_mode==ctr::DspBootMode::ReferenceSlice) std::cout << "dsp_boot_mode=reference-slice mailbox_poll_calls=16384 hardware_timing=unverified\n";
        if(dsp_probe.reference_transmit) std::cout << "dsp_transmit_profile=reference-stereo irq=bounded_control_fifo_empty hardware_format=unverified\n";
        if(dsp_probe.capture_audio && runner.dsp_diagnostics().execution_probe())
            std::cout << "dsp_audio_mode=capture frames=" << runner.dsp_diagnostics().execution_probe()->emitted_audio_frames()
                      << " underflow=" << (dsp_probe.reference_audio_silence?"explicit_reference_silence":"stop")
                      << " playback=none capacity=" << ctr::DspExecutionProbe::kAudioCaptureCapacity
                      << " sink=" << (audio_file?"file":"memory")
                      << " retained_prefix=" << runner.dsp_diagnostics().execution_probe()->captured_audio().size() << '\n';
        if(const auto* device=runner.dsp_diagnostics().live_device())
            std::cout<<"dsp_live_loaded=1 data_base=0x1ff40000 scheduled_slices="<<device->slices()
                     <<" notification_wait_slices="<<device->notification_wait_slices()
                     <<" next_deadline_ns="<<device->next_deadline_ns().value_or(0)<<'\n';
        if(runner.dsp_error())std::cout<<"dsp_live_error="<<runner.dsp_error()<<'\n';
        if(const auto* probe=runner.dsp_diagnostics().execution_probe()) {
            const auto& p=probe->summary();
            std::cout<<"dsp_probe_completed_steps="<<p.completed_steps<<" attempted_steps="<<p.attempted_steps
                     <<" read_words="<<p.read_words<<" written_words="<<p.written_words
                     <<" known_bytes="<<p.known_bytes<<" replies="<<p.reply_count
                     <<" pc_before=0x"<<std::hex<<p.pc_before<<" pc_after=0x"<<p.pc_after<<std::dec<<'\n';
            if(p.has_fault_address)std::cout<<"dsp_probe_fault_byte=0x"<<std::hex<<p.fault_address<<std::dec<<'\n';
            for(const auto& reply:probe->replies())
                std::cout<<"dsp_probe_reply="<<unsigned(reply.index)<<":"<<reply.word<<" after_steps="<<reply.after_steps<<'\n';
        }
        if(cpu_mode==ctr::CpuExecutionMode::DiagnosticDual) {
            std::cout<<"cpu_ticks="<<runner.diagnostic_ticks()<<" core0_instructions="<<runner.issued_instructions()[0]
                     <<" core1_instructions="<<runner.issued_instructions()[1]<<" quota_transitions="<<runner.quota_transitions()<<'\n';
            if(runner.cpu_error())std::cout<<"cpu_model_error="<<runner.cpu_error()<<'\n';
        }
        if (display_mode==ctr::DisplayClockMode::ReferenceIdle) {
            std::cout<<"display_periods="<<runner.display_periods()<<" guest_now_ns="<<kernel.now_ns()<<'\n';
            if (runner.display_error())std::cout<<"display_error="<<runner.display_error()<<'\n';
        }
        const auto& state=runner.live_state();
        std::cout << "app_cpu_time_current=" << kernel.app_cpu_time_current()
                  << " maximum=" << kernel.app_cpu_time_maximum()
                  << " core0_only=" << kernel.app_cpu_core0_only()
                  << " core1_enforcement=" << (cpu_mode==ctr::CpuExecutionMode::DiagnosticDual?"diagnostic_windows":"unimplemented") << '\n';
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
