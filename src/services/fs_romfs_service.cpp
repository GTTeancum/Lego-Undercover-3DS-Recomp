#include "services/fs_romfs_service.h"
#include "host/sha256_stream.h"
#include <algorithm>
#include <cerrno>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <vector>
#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace lego::ctr {
namespace {
[[noreturn]] void IoError(const char* operation) {
    throw std::system_error(errno,std::generic_category(),operation);
}
#if defined(__unix__) || defined(__APPLE__)
std::pair<std::int64_t,std::int64_t> Modified(const struct stat& st) {
#if defined(__APPLE__)
    return {st.st_mtimespec.tv_sec,st.st_mtimespec.tv_nsec};
#else
    return {st.st_mtim.tv_sec,st.st_mtim.tv_nsec};
#endif
}
#endif
}
RomfsImage::~RomfsImage() {
#if defined(__unix__) || defined(__APPLE__)
    if (fd_>=0) ::close(fd_);
#endif
}
void RomfsImage::CheckUnchanged() const {
#if defined(__unix__) || defined(__APPLE__)
    struct stat st{};
    if (::fstat(fd_,&st)!=0) IoError("RomFS fstat");
    if (!S_ISREG(st.st_mode) || st.st_size<0 || std::uint64_t(st.st_size)!=raw_bytes_ ||
        Modified(st)!=std::pair(modified_seconds_,modified_nanoseconds_))
        throw std::runtime_error("RomFS changed outside guest after verification");
#else
    throw std::logic_error("safe RomFS backend unavailable on this host");
#endif
}
void RomfsImage::ReadRaw(std::uint64_t offset,std::span<std::uint8_t> output) const {
    if (offset>raw_bytes_ || output.size()>raw_bytes_-offset)
        throw std::logic_error("RomFS raw read outside verified file");
#if defined(__unix__) || defined(__APPLE__)
    std::size_t done=0;
    while (done<output.size()) {
        const auto n=::pread(fd_,output.data()+done,output.size()-done,static_cast<off_t>(offset+done));
        if (n<0) { if(errno==EINTR)continue;IoError("RomFS pread"); }
        if (n==0) throw std::runtime_error("unexpected EOF in verified RomFS");
        done+=static_cast<std::size_t>(n);
    }
#else
    throw std::logic_error("safe RomFS backend unavailable on this host");
#endif
}
std::shared_ptr<const RomfsImage> RomfsImage::OpenVerified(const std::filesystem::path& path,
    std::uint64_t raw_bytes,std::uint64_t view_offset,const std::string& expected_hash) {
    if (raw_bytes==0 || view_offset>=raw_bytes || expected_hash.size()!=64 ||
        !std::all_of(expected_hash.begin(),expected_hash.end(),[](char c){
            return (c>='0' && c<='9') || (c>='a' && c<='f'); }))
        throw std::invalid_argument("invalid RomFS identity");
    auto image=std::shared_ptr<RomfsImage>(new RomfsImage);
#if defined(__unix__) || defined(__APPLE__)
    if (raw_bytes>static_cast<std::uint64_t>(std::numeric_limits<off_t>::max()))
        throw std::invalid_argument("RomFS too large for host offsets");
    struct stat before{},after{};
    if (::lstat(path.c_str(),&before)!=0) IoError("RomFS path");
    if (!S_ISREG(before.st_mode)) throw std::invalid_argument("RomFS must be a regular non-symlink file");
    image->fd_=::open(path.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK);
    if (image->fd_<0) IoError("RomFS open");
    if (::fstat(image->fd_,&after)!=0) IoError("RomFS opened file");
    if (!S_ISREG(after.st_mode) || before.st_dev!=after.st_dev || before.st_ino!=after.st_ino ||
        after.st_size<0 || std::uint64_t(after.st_size)!=raw_bytes)
        throw std::invalid_argument("RomFS size/type/identity mismatch");
    image->raw_bytes_=raw_bytes; image->view_offset_=view_offset;
    const auto [sec,nsec]=Modified(after);
    image->modified_seconds_=sec; image->modified_nanoseconds_=nsec;
    const auto flags=::fcntl(image->fd_,F_GETFL);
    if (flags<0 || ::fcntl(image->fd_,F_SETFL,flags&~O_NONBLOCK)<0) IoError("RomFS descriptor flags");
    lego::host::Sha256Stream hash;
    std::vector<std::uint8_t> buffer(1024U*1024U);
    for (std::uint64_t offset=0;offset<raw_bytes;) {
        const auto n=static_cast<std::size_t>(std::min<std::uint64_t>(buffer.size(),raw_bytes-offset));
        auto part=std::span(buffer).first(n); image->ReadRaw(offset,part); hash.Update(part); offset+=n;
    }
    image->CheckUnchanged(); image->sha256_=hash.Final();
    if (image->sha256_!=expected_hash) throw std::invalid_argument("RomFS SHA-256 mismatch: "+image->sha256_);
    return image;
#else
    (void)path;
    throw std::logic_error("safe RomFS backend unavailable on this host");
#endif
}
std::uint32_t RomfsImage::Read(std::uint64_t offset,std::span<std::uint8_t> output) const {
    if (output.size()>kMaxRomfsRead) throw std::length_error("RomFS host read bound exceeded");
    // Avoid the unsigned subtraction underflow in the pinned reference at >EOF.
    if (output.empty() || offset>=size()) return 0;
    const auto count=static_cast<std::uint32_t>(std::min<std::uint64_t>(output.size(),size()-offset));
    CheckUnchanged(); ReadRaw(view_offset_+offset,output.first(count)); CheckUnchanged();
    return count;
}
RomfsFileService::RomfsFileService(std::shared_ptr<const RomfsImage> image):image_(std::move(image)) {
    if(!image_)throw std::invalid_argument("RomFS session needs verified input");
}
bool RomfsFileService::CanHandle(const IpcCommandBuffer& q) const noexcept {
    if (q[0]==IpcMakeHeader(0x0804,0,0) || q[0]==IpcMakeHeader(0x0808,0,0)) return true;
    return q[0]==IpcMakeHeader(0x0802,3,2) && q[3]<=kMaxRomfsRead &&
           q[4]==((q[3]<<4U)|0xCU);
}
Result RomfsFileService::Handle(IpcRouter& router,Kernel&,GuestMemory& memory,
                                ThreadObject& thread,IpcCommandBuffer& q) {
    if(!CanHandle(q))return kResultNotFound;
    if (IpcCommandId(q[0])==0x0804) {
        const auto size=image_->size();q.fill(0);q[0]=IpcMakeHeader(0x0804,3,0);
        q[1]=kResultSuccess;q[2]=static_cast<std::uint32_t>(size);q[3]=static_cast<std::uint32_t>(size>>32);
        return kResultSuccess;
    }
    if (IpcCommandId(q[0])==0x0808) {
        // Pinned IVFCFile::Close returns false without releasing its shared
        // reader; File::Close ignores that boolean and replies success. Retain
        // this narrow HLE policy, not a claim about firmware close semantics.
        // The separate kernel handle and other image sessions remain valid.
        q.fill(0);q[0]=IpcMakeHeader(0x0808,1,0);q[1]=kResultSuccess;
        return kResultSuccess;
    }
    const auto length=q[3],descriptor=q[4],address=q[5];
    const auto offset=std::uint64_t(q[1]) | (std::uint64_t(q[2])<<32);
    const auto end=std::uint64_t(address)+length;
    // Preflight the complete declared output before file access, including any
    // tail that EOF would clip. No memory mapping changes inside this handler.
    if(end>0x100000000ULL || (length && !memory.IsWritable(address,length)))
        return kResultInvalidPointer;
    const auto cb=std::uint64_t(thread.tls_address)+kIpcCommandBufferOffset;
    if(length && address<cb+sizeof(IpcCommandBuffer) && cb<end) {
        router.RequestHostStop("RomFS read output aliases IPC reply");return kResultSuccess;
    }
    std::uint32_t count=0;
    try {
        // A failed/short host read commits neither data nor a guest success.
        // No placeholder zero bytes are sent to stand in for missing input.
        std::vector<std::uint8_t> bytes(length);
        count=image_->Read(offset,bytes);
        for(std::uint32_t i=0;i<count;++i) {
            if(!memory.Write8(address+i,bytes[i]))
                throw std::runtime_error("RomFS output failed after preflight");
        }
    } catch(const std::exception& error) {
        router.RequestHostStop(error.what());return kResultSuccess;
    }
    q.fill(0);q[0]=IpcMakeHeader(0x0802,2,2);q[1]=kResultSuccess;q[2]=count;
    q[3]=descriptor;q[4]=address;
    return kResultSuccess;
}
} // namespace lego::ctr
