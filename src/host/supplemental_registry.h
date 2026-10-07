#pragma once
#include "recomp/a32_runtime.h"
#include <algorithm>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace lego::host {
// Adds already-compiled, immutable blocks to verified holes only. This is not a
// runtime decoder or a missing-PC fallback. Base/extra op arrays must outlive us.
// The caller still authenticates EVERY resulting raw word against code.bin.
class SupplementalRegistry final {
    using Block = oot3d::recomp::a32::Block;
    using Shard = oot3d::recomp::a32::BlockShard;
    using Registry = oot3d::recomp::a32::Registry;
public:
    SupplementalRegistry(const Registry& base, std::span<const Block> additions) {
        if (!base.shards || !base.shard_count || (base.function_count && !base.functions))
            throw std::invalid_argument("invalid base registry");
        shards_.assign(base.shards, base.shards + base.shard_count);
        blocks_.resize(base.shard_count);
        std::uint64_t previous_end = 0;
        for (std::size_t i=0;i<shards_.size();++i) {
            const auto& s=shards_[i];
            if ((s.first_pc & 3U) || (s.last_pc & 3U) || s.first_pc>s.last_pc ||
                (i && s.first_pc<previous_end) || (s.block_count && !s.blocks))
                throw std::invalid_argument("invalid base shard");
            previous_end=std::uint64_t(s.last_pc)+4;
            std::uint64_t block_end=s.first_pc;
            for (std::uint32_t n=0;n<s.block_count;++n) {
                ValidateBlock(s.blocks[n],s);
                if (s.blocks[n].pc<block_end) throw std::invalid_argument("overlapping base blocks");
                block_end=End(s.blocks[n]);
            }
        }
        for (const auto& b:additions) {
            auto it=std::upper_bound(shards_.begin(),shards_.end(),b.pc,
                [](std::uint32_t pc,const Shard& s){return pc<s.first_pc;});
            if (it==shards_.begin()) throw std::invalid_argument("supplement outside base shards");
            --it;ValidateBlock(b,*it);
            const auto i=static_cast<std::size_t>(it-shards_.begin());
            auto& v=blocks_[i];
            if (v.empty() && it->block_count) v.assign(it->blocks,it->blocks+it->block_count);
            v.push_back(b);
        }
        for (std::size_t i=0;i<shards_.size();++i) {
            auto& v=blocks_[i];if(v.empty())continue;
            std::sort(v.begin(),v.end(),[](const Block& a,const Block& b){return a.pc<b.pc;});
            std::uint64_t end=shards_[i].first_pc;
            for(const auto& b:v) {
                if (b.pc<end) throw std::invalid_argument("supplement overlaps recorded code");
                end=End(b);
            }
            shards_[i].blocks=v.data();shards_[i].block_count=static_cast<std::uint32_t>(v.size());
        }
        registry_={shards_.data(),static_cast<std::uint32_t>(shards_.size()),
                   base.functions,base.function_count};
    }
    SupplementalRegistry(const SupplementalRegistry&)=delete;
    SupplementalRegistry& operator=(const SupplementalRegistry&)=delete;
    SupplementalRegistry(SupplementalRegistry&&)=delete;
    SupplementalRegistry& operator=(SupplementalRegistry&&)=delete;
    [[nodiscard]] const Registry& registry() const noexcept {return registry_;}
private:
    static std::uint64_t End(const Block& b) noexcept {
        return std::uint64_t(b.pc)+std::uint64_t(b.op_count)*4;
    }
    static void ValidateBlock(const Block& b,const Shard& s) {
        if (!b.ops || !b.op_count || (b.pc&3U) || b.pc<s.first_pc ||
            End(b)>std::uint64_t(s.last_pc)+4 || End(b)>0x100000000ULL)
            throw std::invalid_argument("invalid supplemental or base block extent");
    }
    std::vector<Shard> shards_;
    std::vector<std::vector<Block>> blocks_;
    Registry registry_{};
};
} // namespace lego::host
