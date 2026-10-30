#include "services/hid_user_service.h"
#include <algorithm>
#include <limits>

namespace lego::ctr {
HidUserService::SharedState::SharedState()
    : memory(std::make_shared<ServiceSharedMemoryObject>(1)) {
    // Pinned HID module owns five distinct unsignaled OneShot events and a
    // 4 KiB page. These objects survive client handle closure/reconnection.
    for (auto& event : events) event = std::make_shared<EventObject>(ResetType::OneShot);
}
HidUserService::HidUserService() : shared_(std::make_shared<SharedState>()) {}

bool HidUserService::PublishHostInput(Kernel& kernel, const HostInputState& input,
                                      std::uint64_t ticks) noexcept {
    // Layout/ring semantics follow Azahar 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac,
    // src/core/hle/service/hid/{hid.h,hid.cpp}. This independently authored
    // byte writer implements pad/touch only. Raw entries remain untouched, as in
    // that HLE reference; host coordinates do not claim physical calibration.
    if ((input.buttons & ~0xFFFU) || input.circle_x < -154 || input.circle_x > 154 ||
        input.circle_y < -154 || input.circle_y > 154 ||
        (input.touching && (input.touch_x >= 320 || input.touch_y >= 240)) ||
        ticks > std::uint64_t(std::numeric_limits<std::int64_t>::max()) ||
        (shared_->samples && ticks < shared_->last_input_ticks) ||
        shared_->samples == std::numeric_limits<std::uint64_t>::max()) return false;

    constexpr std::size_t kPadTouchBytes = 0x108;
    std::array<std::uint8_t,kPadTouchBytes> bytes{};
    std::copy_n(shared_->memory->bytes().begin(),bytes.size(),bytes.begin());
    const auto write16=[&](std::size_t offset,std::uint16_t value) {
        for (unsigned i=0;i<2;++i) bytes[offset+i]=std::uint8_t(value>>(i*8));
    };
    const auto write32=[&](std::size_t offset,std::uint32_t value) {
        for (unsigned i=0;i<4;++i) bytes[offset+i]=std::uint8_t(value>>(i*8));
    };
    const auto write64=[&](std::size_t offset,std::uint64_t value) {
        for (unsigned i=0;i<8;++i) bytes[offset+i]=std::uint8_t(value>>(i*8));
    };
    auto buttons=input.buttons;
    const int x=input.circle_x,y=input.circle_y;
    // Above radius 40, 30/60-degree sectors set the circle direction bits.
    // Squared integer comparisons avoid divide-by-zero and floating rounding.
    if (x*x+y*y>1600) {
        if (x && y*y<3*x*x) buttons |= x>0 ? 1U<<28 : 1U<<29;
        if (!x || 3*y*y>x*x) buttons |= y>0 ? 1U<<30 : 1U<<31;
    }
    const auto index=shared_->next_input_index;
    write32(0x10,index);
    write32(0x1C,buttons);
    const std::size_t pad=0x28+index*16;
    write32(pad,buttons);
    write32(pad+4,buttons & ~shared_->previous_buttons);
    write32(pad+8,shared_->previous_buttons & ~buttons);
    write16(pad+12,static_cast<std::uint16_t>(input.circle_x));
    write16(pad+14,static_cast<std::uint16_t>(input.circle_y));
    write32(0xB8,index);
    const std::size_t touch=0xC8+index*8;
    write16(touch,input.touching?input.touch_x:0);
    write16(touch+2,input.touching?input.touch_y:0);
    write32(touch+4,input.touching?1U:0U);
    if (index==0) {
        std::copy_n(bytes.begin(),8,bytes.begin()+8);
        std::copy_n(bytes.begin()+0xA8,8,bytes.begin()+0xB0);
        write64(0,ticks);
        write64(0xA8,ticks);
    }
    // A fixed, in-bounds write cannot partially update the shared backing.
    if (!shared_->memory->Write(0,bytes)) return false;
    shared_->next_input_index=(index+1)%8;
    shared_->previous_buttons=buttons;
    shared_->last_input_ticks=ticks;
    ++shared_->samples;
    if (!kernel.SignalEventObject(*shared_->events[0])) return false;
    return kernel.SignalEventObject(*shared_->events[1]);
}

Result HidUserService::CreateSessionHandler(std::shared_ptr<IpcService>* out) {
    if (!out) return kResultInvalidPointer;
    for (auto& weak : shared_->sessions) {
        if (!weak.expired()) continue;
        auto identity = std::make_shared<SessionIdentity>();
        auto session = std::shared_ptr<HidUserService>(new HidUserService(shared_,identity));
        weak = identity;
        *out = std::move(session);
        return kResultSuccess;
    }
    return kResultMaxConnectionsReached;
}

Result HidUserService::Handle(IpcRouter&, Kernel& kernel, GuestMemory&, ThreadObject&,
                              IpcCommandBuffer& command) {
    if (!CanHandle(command)) return kResultNotFound;
    // The router has preflighted the entire readable/writable reply before this
    // method. Returning six genuine copied handles does not sample or reset input.
    const std::array<std::shared_ptr<KernelObject>,6> objects{
        shared_->memory, shared_->events[0], shared_->events[1], shared_->events[2],
        shared_->events[3], shared_->events[4]};
    std::array<lego::ctr::Handle,6> handles{};
    const auto result = kernel.handles().CreateCopies(objects, handles);
    if (result != kResultSuccess) return result; // no partial reply/handle exports
    command.fill(0);
    command[0] = IpcMakeHeader(0x000A,1,7);
    command[1] = kResultSuccess;
    command[2] = IpcCopyHandleDesc(6);
    for (std::size_t i = 0; i < handles.size(); ++i) command[3+i] = handles[i];
    return kResultSuccess;
}
} // namespace lego::ctr
