#include "runtime/ctr_recorded_step.h"

namespace lego::ctr {
static bool FindRecordedSuffix(const a32::Registry& registry, std::uint32_t pc,
                               a32::Block* suffix) noexcept {
    if (!suffix || (pc & 3U) != 0U || !registry.shards || !registry.shard_count)
        return false;
    std::uint32_t low = 0U, high = registry.shard_count;
    while (low < high) {
        const auto middle = low + (high - low) / 2U;
        if (registry.shards[middle].first_pc <= pc) low = middle + 1U;
        else high = middle;
    }
    if (!low) return false;
    const auto& shard = registry.shards[low - 1U];
    if (pc > shard.last_pc || !shard.blocks || !shard.block_count ||
        (shard.first_pc & 3U) != 0U || (shard.last_pc & 3U) != 0U)
        return false;
    low = 0U;
    high = shard.block_count;
    while (low < high) {
        const auto middle = low + (high - low) / 2U;
        if (shard.blocks[middle].pc <= pc) low = middle + 1U;
        else high = middle;
    }
    if (!low) return false;
    const a32::Block& recorded = shard.blocks[low - 1U];
    if (!recorded.ops || !recorded.op_count || recorded.pc >= pc ||
        recorded.pc < shard.first_pc || (recorded.pc & 3U) != 0U)
        return false;
    const std::uint64_t end = std::uint64_t(recorded.pc) +
                              std::uint64_t(recorded.op_count) * 4U;
    if (end > 0x100000000ULL || end > std::uint64_t(shard.last_pc) + 4U ||
        std::uint64_t(pc) >= end ||
        (low < shard.block_count && end > shard.blocks[low].pc))
        return false;
    const auto offset = (pc - recorded.pc) / 4U;
    *suffix = {pc, recorded.ops + offset, recorded.op_count - offset, false};
    // native_candidate belongs to the original entry, not an arbitrary suffix.
    return true;
}

a32::ExecutionResult StepRecordedA32(const a32::Registry& registry, a32::GuestState& state,
                                 a32::MemoryBus& memory) {
    const auto pc=state.r[15];
    if ((pc&3U) || (state.cpsr&(1U<<5U)))
        return {a32::ExitKind::Unsupported,pc,a32::FallbackReason::Unsupported,0};
    const a32::Block* block=a32::FindBlock(registry,pc);
    a32::Block suffix{};
    if(!block && FindRecordedSuffix(registry,pc,&suffix))block=&suffix;
    if(!block)return {a32::ExitKind::MissingBlock,pc,a32::FallbackReason::None,0};
    if(!block->ops || !block->op_count)
        return {a32::ExitKind::Unsupported,pc,a32::FallbackReason::Unsupported,0};
    const a32::Block one{pc,block->ops,1,false};
    const auto result=a32::ExecuteBlock(one,state,memory);
    state.r[15]=result.pc;
    return result;
}

} // namespace lego::ctr
