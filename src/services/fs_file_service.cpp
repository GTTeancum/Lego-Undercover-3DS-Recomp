#include "services/fs_file_service.h"
#include <algorithm>
#include <cerrno>
#include <limits>
#include <string>
#include <system_error>
#include <vector>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace lego::ctr {
std::uint64_t SharedArchiveFile::size() const {
    if (!savedata_) return size_; // Preserve fixed extdata's captured extent.
    if (!is_open()) throw std::logic_error("GetSize on a closed SaveData backend");
#if defined(__unix__) || defined(__APPLE__)
    struct stat st{};
    if (::fstat(fd_,&st)!=0)
        throw std::system_error(errno,std::generic_category(),"SaveData GetSize fstat");
    if (!S_ISREG(st.st_mode) || st.st_size<0 || st.st_nlink>1 ||
        static_cast<std::uint64_t>(st.st_size)>kMaxSaveDataFileBytes)
        throw std::runtime_error("SaveData extent/type/link count exceeds host policy");
    // Other endpoints for this inode can grow it; an instance-local snapshot
    // would falsely report the size observed at OpenFile.
    return static_cast<std::uint64_t>(st.st_size);
#else
    throw std::logic_error("contained SaveData size unavailable on this host");
#endif
}

void SharedArchiveFile::Close() {
    if (fd_ < 0) return;
    // Retire ownership before close; never retry an ambiguous close error or let
    // the destructor close a descriptor the host may have already recycled.
    const int fd = fd_;
    fd_ = -1;
#if defined(__unix__) || defined(__APPLE__)
    if (::close(fd) != 0)
        throw std::system_error(errno, std::generic_category(), "file Close");
#else
    (void)fd;
    throw std::logic_error("contained file close unavailable on this host");
#endif
}

std::uint32_t SharedArchiveFile::Read(std::uint64_t offset, std::span<std::uint8_t> data) const {
    if (!savedata_ || data.size()>kMaxFsFileTransfer)
        throw std::logic_error("unsupported SaveData Read backend/size");
    if (!is_open()) throw std::logic_error("Read on a closed SaveData backend");
#if defined(__unix__) || defined(__APPLE__)
    // DiskFile::Read/GetSize at the retained reference use the current file and
    // return the actual count, including a short or zero-byte EOF read:
    // https://github.com/azahar-emu/azahar/blob/86a9f9236ae42bb5a2b995dbc933d599d8ea07ac/src/core/file_sys/disk_archive.cpp
    // Reuse the regular-file, link-count and live-extent host checks. Keep the
    // explicit host offset bound; it is not a discovered console restriction.
    const auto extent=size();
    if (offset>kMaxSaveDataFileBytes ||
        offset>static_cast<std::uint64_t>(std::numeric_limits<off_t>::max()))
        throw std::logic_error("SaveData Read offset exceeds the supported host extent");
    const auto count=static_cast<std::uint32_t>(
        offset>=extent ? 0 : std::min<std::uint64_t>(data.size(),extent-offset));
    std::uint32_t read=0;
    while (read<count) {
        // No shared seek position changes, file writes, or guest-visible bytes.
        const auto n=::pread(fd_,data.data()+read,count-read,static_cast<off_t>(offset+read));
        if (n<0) {
            if (errno==EINTR) continue;
            throw std::system_error(errno,std::generic_category(),"SaveData Read pread");
        }
        if (n==0) break; // A real short read if the host file shrank meanwhile.
        read+=static_cast<std::uint32_t>(n);
    }
    return read;
#else
    (void)offset;
    throw std::logic_error("contained SaveData reads unavailable on this host");
#endif
}

Result SharedArchiveFile::Write(std::uint64_t offset, std::span<const std::uint8_t> data,
                                bool flush, std::uint32_t* written) {
    if (!written || data.size() > kMaxFsFileTransfer)
        throw std::logic_error("unsupported file write output/size");
    *written = 0;
    if (!is_open()) throw std::logic_error("Write on a closed file backend");
    // Match fixed extdata: beyond end is an error; at end is a zero-byte success.
    // Subtraction after the comparison avoids offset+length overflow.
    if (!savedata_) {
        if (offset > size_) return kResultFsWriteBeyondEnd;
        if (offset == size_) return kResultSuccess;
    }
#if defined(__unix__) || defined(__APPLE__)
    struct stat st{};
    if (::fstat(fd_, &st) != 0)
        throw std::system_error(errno, std::generic_category(), "file Write fstat");
    // Extdata retains its fixed-size policy. SaveData follows DiskFile's growing
    // extent, bounded here by explicit host policy rather than an invented quota.
    // Neither policy writes a newly hard-linked object.
    if (!S_ISREG(st.st_mode) || st.st_size < 0 ||
        (!savedata_ && static_cast<std::uint64_t>(st.st_size) != size_) || st.st_nlink > 1)
        throw std::runtime_error("file extent/type/link count changed outside guest");
    if (savedata_ && (static_cast<std::uint64_t>(st.st_size)>kMaxSaveDataFileBytes ||
        offset>kMaxSaveDataFileBytes || data.size()>kMaxSaveDataFileBytes-offset))
        throw std::logic_error("SaveData Write exceeds the supported host extent");
    const auto count = static_cast<std::uint32_t>(
        savedata_ ? data.size() : std::min<std::uint64_t>(data.size(), size_ - offset));
    const auto extent = savedata_ ? offset+count : size_;
    if (extent > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max()))
        throw std::logic_error("file offset cannot be represented by this host");
    while (*written < count) {
        const auto n = ::pwrite(fd_, data.data() + *written, count - *written,
                                static_cast<off_t>(offset + *written));
        if (n < 0) {
            if (errno == EINTR) continue;
            throw std::system_error(errno, std::generic_category(), "file Write pwrite");
        }
        if (n == 0) throw std::runtime_error("file Write made no progress");
        *written += static_cast<std::uint32_t>(n);
    }
    // pwrite has no stdio buffer. Explicitly request fsync on the same descriptor
    // for flush requests: stronger host policy than pinned Azahar's fflush.
    // A sync failure stops with the actual written count, never claimed rollback.
    if (flush) {
        int result;
        do { result = ::fsync(fd_); } while (result != 0 && errno == EINTR);
        if (result != 0)
            throw std::system_error(errno, std::generic_category(), "file Write fsync");
    }
    return kResultSuccess;
#else
    (void)flush;
    throw std::logic_error("contained file writes unavailable on this host");
#endif
}

bool FsFileService::CanHandle(const IpcCommandBuffer& command) const noexcept {
    if (command[0] == IpcMakeHeader(0x0804, 0, 0) ||
        command[0] == IpcMakeHeader(0x0808, 0, 0)) return true;
    if (file_->is_savedata() && file_->is_open() &&
        command[0]==IpcMakeHeader(0x0802,3,2))
        return command[3]<=kMaxFsFileTransfer && command[4]==((command[3]<<4U)|0xCU);
    return file_->is_open() && command[0] == IpcMakeHeader(0x0803, 4, 2) &&
           command[3] <= kMaxFsFileTransfer &&
           command[5] == ((command[3] << 4U) | 0xAU);
}

Result FsFileService::Handle(IpcRouter& router, Kernel&, GuestMemory& memory,
                             ThreadObject& thread, IpcCommandBuffer& command) {
    if (!CanHandle(command)) return kResultNotFound;
    const auto cb = std::uint64_t(thread.tls_address) + kIpcCommandBufferOffset;
    if (file_->is_savedata()) {
        if (cb+sizeof(command)>0x100000000ULL ||
            !memory.IsReadable(static_cast<std::uint32_t>(cb),sizeof(command)) ||
            !memory.IsWritable(static_cast<std::uint32_t>(cb),sizeof(command)))
            return kResultInvalidPointer;
        if (!memory.IsCachelessPrivateRange(static_cast<std::uint32_t>(cb),sizeof(command))) {
            router.RequestHostStop("SaveData file reply requires private backing");
            return kResultSuccess;
        }
        try {
            // Prepare before Read publishes bytes, Write changes files, or
            // Close releases ownership.
            // Ordinary reply writes then need no callback or epoch allocation.
            if (!memory.PrepareDeviceWrite(static_cast<std::uint32_t>(cb),sizeof(command))) {
                router.RequestHostStop("SaveData file reply preparation failed");
                return kResultSuccess;
            }
        } catch (const std::exception& error) {
            router.RequestHostStop(error.what());
            return kResultSuccess;
        }
    }
    if (IpcCommandId(command[0]) == 0x0804) {
        std::uint64_t size;
        try {
            size=file_->size();
        } catch (const std::exception& error) {
            router.RequestHostStop(error.what());
            return kResultSuccess;
        }
        command.fill(0);
        command[0] = IpcMakeHeader(0x0804, 3, 0);
        command[1] = kResultSuccess;
        command[2] = static_cast<std::uint32_t>(size);
        command[3] = static_cast<std::uint32_t>(size >> 32);
        return kResultSuccess;
    }
    if (IpcCommandId(command[0]) == 0x0808) {
        try {
            file_->Close();
        } catch (const std::exception& error) {
            router.RequestHostStop(error.what());
            return kResultSuccess;
        }
        // File IPC Close closes the backend, NOT the kernel client handle.
        // Repeated Close returns success as in the pinned HLE (not HW-verified).
        command.fill(0);
        command[0] = IpcMakeHeader(0x0808, 1, 0);
        command[1] = kResultSuccess;
        return kResultSuccess;
    }
    if (IpcCommandId(command[0])==0x0802) {
        const auto length=command[3],descriptor=command[4],address=command[5];
        const auto offset=std::uint64_t(command[1])|(std::uint64_t(command[2])<<32);
        const auto end=std::uint64_t(address)+length;
        // Check the entire declared destination even if EOF will return fewer
        // bytes. Both it and the already-prepared TLS reply must be private.
        if (end>0x100000000ULL || (length!=0 && !memory.IsWritable(address,length)))
            return kResultInvalidPointer;
        if (length!=0 && (!memory.IsCachelessPrivateRange(address,length) ||
            memory.SpansAlias(address,length,static_cast<std::uint32_t>(cb),sizeof(command)))) {
            router.RequestHostStop("SaveData Read destination requires private backing separate from its reply");
            return kResultSuccess;
        }
        try {
            if (!memory.PrepareDeviceWrite(address,length))
                throw std::runtime_error("SaveData Read destination preparation failed");
            std::vector<std::uint8_t> data(length);
            const auto read=file_->Read(offset,data);
            // File::Read returns result/count and the unchanged mapped buffer;
            // only its actual read count is copied, preserving every tail byte.
            // https://github.com/azahar-emu/azahar/blob/86a9f9236ae42bb5a2b995dbc933d599d8ea07ac/src/core/hle/service/fs/file.cpp
            const IpcCommandBuffer reply{IpcMakeHeader(0x0802,2,2),kResultSuccess,read,descriptor,address};
            if (!memory.CommitDeviceWrite(address,std::span<const std::uint8_t>(data).first(read)))
                throw std::runtime_error("SaveData Read destination commit failed");
            command=reply; // No allocations or callbacks remain after commit.
        } catch (const std::exception& error) {
            router.RequestHostStop(std::string("SaveData Read stopped without a reply: ")+error.what());
        }
        return kResultSuccess;
    }
    const auto length = command[3], flags = command[4];
    const auto descriptor = command[5], address = command[6];
    const auto offset = std::uint64_t(command[1]) | (std::uint64_t(command[2]) << 32);
    const auto end = std::uint64_t(address) + length;
    // Validate the WHOLE declared input even when EOF will clip the write.
    if (end > 0x100000000ULL || (length != 0 && !memory.IsReadable(address, length)))
        return kResultInvalidPointer;
    if (length != 0 && address < cb + sizeof(IpcCommandBuffer) && cb < end) {
        router.RequestHostStop("file Write source aliases IPC reply (unsupported mapping)");
        return kResultSuccess;
    }
    if (file_->is_savedata() && length!=0 &&
        (!memory.IsCachelessPrivateRange(address,length) ||
         memory.SpansAlias(address,length,static_cast<std::uint32_t>(cb),sizeof(command)))) {
        router.RequestHostStop("SaveData Write source requires private backing separate from its reply");
        return kResultSuccess;
    }
    std::uint32_t written = 0;
    Result result;
    try {
        std::vector<std::uint8_t> data(length);
        for (std::uint32_t i = 0; i < length; ++i) {
            if (!memory.Read8(address + i, &data[i]))
                throw std::runtime_error("file Write input changed after preflight");
        }
        // Pinned File::Write uses low byte for flush, next byte for timestamps.
        // DiskFile ignores the timestamp flag; bit 16 in actual 0x10001 is not it.
        // No guest timestamp/clock changes or guessed flag semantics are added.
        result = file_->Write(offset, data, (flags & 0xFFU) != 0, &written);
    } catch (const std::exception& error) {
        router.RequestHostStop("file Write stopped after " + std::to_string(written) +
                               " bytes; host changes retained: " + error.what());
        return kResultSuccess; // Router preserves original CPU and IPC request.
    }
    command.fill(0);
    command[0] = IpcMakeHeader(0x0803, 2, 2);
    command[1] = result;
    command[2] = written;
    command[3] = descriptor;
    command[4] = address;
    return kResultSuccess;
}
} // namespace lego::ctr
