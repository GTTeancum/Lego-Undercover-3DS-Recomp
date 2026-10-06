#pragma once
#include <array>
#include <memory>
#include "runtime/ctr_shared_memory.h"
#include "services/pica_startup.h"

namespace lego::ctr {
// Prepared for synchronous commit with no guest execution between the phases.
// No pixels, rasterizer calls, counter pokes or display-present claims.
struct DisplayPeriodPlan {
    struct Step {
        std::shared_ptr<EventObject> event;
        std::uint32_t relay_base{},slot_offset{},missed_offset{},missed_value{},fb_base{};
        std::uint8_t interrupt{},count{},dirty_byte{};
        bool queue{},missed{},update_fb{};
        std::array<std::uint32_t,7> info{};
        std::uint32_t physical_left{},physical_right{};
    };
    std::array<Step,8> steps{};
    std::size_t count{};
    std::array<std::uint8_t,0x1000> before_page{};
    PicaGpuRegisters before_registers{};
    std::array<std::array<std::uint32_t,7>,2> before_cached{};
    std::shared_ptr<void> state;
    std::uint64_t generation{};
};
} // namespace lego::ctr
