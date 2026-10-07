#pragma once
#include "services/dsp_execution_probe.h"
#include <cstdio>
#include <filesystem>
#include <memory>
#include <stdexcept>

namespace lego::host {
// Diagnostic records, NOT a WAV (no measured sample rate/channel wiring asserted).
// Header: "DSPAUD1\0", u32 version=1, u32 record-size=16, 16 reserved zero bytes.
// Record: s16 channel0, s16 channel1, u8 FIFO provenance mask, three zero bytes,
//         u64 interpreter attempt counter. Every multi-byte field is little-endian.
// Exclusive create never replaces a preexisting file. A failed write/close is
// terminal; a partial file is retained as evidence, never retried or called complete.
class DspAudioFile final : public ctr::DspAudioSink {
public:
    static constexpr std::uint64_t kMaxFrames=4U*1024U*1024U; // 64 MiB host disk guard.
    static std::shared_ptr<DspAudioFile> Open(const std::filesystem::path& path,
                                            std::uint64_t max_frames=kMaxFrames) {
        if(path.empty() || max_frames==0 || max_frames>kMaxFrames)
            throw std::invalid_argument("invalid DSP audio file path or frame bound");
        auto result=std::shared_ptr<DspAudioFile>(new DspAudioFile(max_frames));
#ifdef _WIN32
        result->file_=_wfopen(path.c_str(),L"wbx");
#else
        result->file_=std::fopen(path.c_str(),"wbx");
#endif
        if(!result->file_)throw std::runtime_error("cannot create NEW DSP audio file; existing paths are never overwritten");
        std::array<std::uint8_t,32> header{'D','S','P','A','U','D','1',0,1,0,0,0,16,0,0,0};
        if(!result->Bytes(header))throw std::runtime_error("DSP audio file header write failed; partial file retained");
        return result;
    }
    ~DspAudioFile() override { (void)Close(); }
    DspAudioFile(const DspAudioFile&)=delete;
    DspAudioFile& operator=(const DspAudioFile&)=delete;
    bool Write(const ctr::DspCapturedAudioFrame& frame) noexcept override {
        if(!file_ || failed_)return false;
        if(frames_==max_frames_ || frame.fifo_mask>3 ||
           (frames_ && frame.during_run_call<last_call_)) {failed_=true;return false;}
        std::array<std::uint8_t,16> record{};
        for(unsigned ch=0;ch<2;++ch) {
            auto value=static_cast<std::uint16_t>(frame.samples[ch]);
            record[ch*2]=static_cast<std::uint8_t>(value);record[ch*2+1]=static_cast<std::uint8_t>(value>>8);
        }
        record[4]=frame.fifo_mask;
        for(unsigned i=0;i<8;++i)record[8+i]=static_cast<std::uint8_t>(frame.during_run_call>>(8*i));
        if(!Bytes(record))return false;
        ++frames_;last_call_=frame.during_run_call;return true;
    }
    bool Close() noexcept {
        if(file_) {auto* f=file_;file_=nullptr;if(std::fclose(f)!=0)failed_=true;}
        return !failed_;
    }
    [[nodiscard]] std::uint64_t frames() const noexcept {return frames_;}
    [[nodiscard]] bool failed() const noexcept {return failed_;}
private:
    explicit DspAudioFile(std::uint64_t bound):max_frames_(bound){}
    bool Bytes(std::span<const std::uint8_t> bytes) noexcept {
        if(!file_ || failed_)return false;
        if(std::fwrite(bytes.data(),1,bytes.size(),file_)!=bytes.size() || std::fflush(file_)!=0) {
            failed_=true;return false;
        }
        // Flush stdio errors each record. This is not a power-loss/fsync guarantee.
        return true;
    }
    std::FILE* file_{};
    std::uint64_t frames_{},last_call_{},max_frames_{};
    bool failed_{};
};
}
