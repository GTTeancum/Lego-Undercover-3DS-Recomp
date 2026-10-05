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

Result SharedArchiveFile::Write(std::uint64_t offset, std::span<const std::uint8_t> data,
                                bool flush, std::uint32_t* written) {
    if (!written || data.size() > kMaxFsFileTransfer)
        throw std::logic_error("unsupported file write output/size");
    *written = 0;
    if (!is_open()) throw std::logic_error("Write on a closed file backend");
    // Match fixed extdata: beyond end is an error; at end is a zero-byte success.
    // Subtraction after the comparison avoids offset+length overflow.
    if (offset > size_) return kResultFsWriteBeyondEnd;
    if (offset == size_) return kResultSuccess;
#if defined(__unix__) || defined(__APPLE__)
    struct stat st{};
    if (::fstat(fd_, &st) != 0)
        throw std::system_error(errno, std::generic_category(), "file Write fstat");
    // Do not silently regrow a host-truncated file or write a newly hard-linked
    // object. This is host containment policy, not a guest filesystem Result.
    if (!S_ISREG(st.st_mode) || st.st_size < 0 ||
        static_cast<std::uint64_t>(st.st_size) != size_ || st.st_nlink > 1)
        throw std::runtime_error("file extent/type/link count changed outside guest");
    const auto count = static_cast<std::uint32_t>(
        std::min<std::uint64_t>(data.size(), size_ - offset));
    if (size_ > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max()))
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
    return file_->is_open() && command[0] == IpcMakeHeader(0x0803, 4, 2) &&
           command[3] <= kMaxFsFileTransfer &&
           command[5] == ((command[3] << 4U) | 0xAU);
}

Result FsFileService::Handle(IpcRouter& router, Kernel&, GuestMemory& memory,
                             ThreadObject& thread, IpcCommandBuffer& command) {
    if (!CanHandle(command)) return kResultNotFound;
    if (IpcCommandId(command[0]) == 0x0804) {
        const auto size = file_->size();
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
    const auto length = command[3], flags = command[4];
    const auto descriptor = command[5], address = command[6];
    const auto offset = std::uint64_t(command[1]) | (std::uint64_t(command[2]) << 32);
    const auto end = std::uint64_t(address) + length;
    // Validate the WHOLE declared input even when EOF will clip the write.
    if (end > 0x100000000ULL || (length != 0 && !memory.IsReadable(address, length)))
        return kResultInvalidPointer;
    const auto cb = std::uint64_t(thread.tls_address) + kIpcCommandBufferOffset;
    if (length != 0 && address < cb + sizeof(IpcCommandBuffer) && cb < end) {
        router.RequestHostStop("file Write source aliases IPC reply (unsupported mapping)");
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
