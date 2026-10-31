#include "runtime/ctr_recorded_step.h"
#include "runtime/ctr_memory.h"
#include <array>
#include <cstdlib>
#include <iostream>

namespace {
using namespace lego::ctr;
int failures{};
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL " << __LINE__ << ": " #x "\n"; ++failures; } } while (0)
constexpr std::uint32_t pc=0x00900000U;
constexpr a32::PackedOp Move(unsigned value) {
    return {(14U<<28U)|(1U<<25U)|(13U<<21U)|value,
            a32::EncodeMetadata(a32::Opcode::MovImm,a32::Condition::Al,a32::Immediate)};
}
constexpr auto seven=Move(7U),fortytwo=Move(42U);

void ReusedShardStorage() {
    // Every registry is immutable during its own lifetime. Reuse its shard
    // storage only after that registry has died. Retain both block arrays so
    // the broken cache fails deterministically without dereferencing freed data.
    const a32::Block first[]{{pc,&seven,1,false}};
    const a32::Block second[]{{pc-4U,&seven,1,false},{pc,&fortytwo,1,false},
                             {pc+0x40U,&seven,1,false}};
    a32::BlockShard storage{pc-4U,pc+0xffcU,first,1};
    {
        const a32::Registry original{&storage,1,nullptr,0};
        CHECK(a32::FindBlock(original,pc)==&first[0]);
        CHECK(a32::FindBlock(original,pc)==&first[0]);
    }
    storage.blocks=second; storage.block_count=3;
    {
        const a32::Registry replacement{&storage,1,nullptr,0};
        CHECK(a32::FindBlock(replacement,pc)==&second[1]);
        GuestMemory memory;
        a32::GuestState initial{}; initial.r[15]=pc; initial.cpsr=0xA0000010U;
        auto stepped=initial;
        const auto one=StepRecordedA32(replacement,stepped,memory);
        CHECK(one.kind==a32::ExitKind::Fallthrough && one.pc==pc+4U);
        CHECK(stepped.r[0]==42U && stepped.cpsr==initial.cpsr);
        const auto stopped=stepped;
        CHECK(StepRecordedA32(replacement,stepped,memory).kind==a32::ExitKind::MissingBlock);
        CHECK(stepped.r==stopped.r && stepped.cpsr==stopped.cpsr);
        auto dispatched=initial;
        const auto full=a32::Dispatch(replacement,pc,dispatched,memory,nullptr,nullptr,4U);
        CHECK(full.kind==a32::ExitKind::MissingBlock && full.pc==pc+4U);
        CHECK(dispatched.r[0]==42U && dispatched.cpsr==initial.cpsr);
    }
    storage.blocks=nullptr; storage.block_count=0;
    {
        const a32::Registry empty{&storage,1,nullptr,0};
        CHECK(a32::FindBlock(empty,pc)==nullptr);
    }
}

void MissingThenPresent() {
    // A cached miss must not hide a block in the next registry that occupies
    // the same host storage, even when both array sizes remain identical.
    constexpr auto target=pc+0x1000U;
    const a32::Block absent{target+8U,&seven,1,false};
    const a32::Block present{target,&fortytwo,1,false};
    a32::BlockShard storage{target,target+0xffcU,&absent,1};
    {
        const a32::Registry original{&storage,1,nullptr,0};
        CHECK(a32::FindBlock(original,target)==nullptr);
        CHECK(a32::FindBlock(original,target)==nullptr);
    }
    storage.blocks=&present;
    {
        const a32::Registry replacement{&storage,1,nullptr,0};
        CHECK(a32::FindBlock(replacement,target)==&present);
    }
}

void ReusedBlockStorage() {
    // Keeping the table pointer and count is not a lifetime identity either.
    // Sorted records can occupy different indices in a later registry.
    constexpr auto target=pc+0x2000U;
    std::array<a32::Block,2> blocks{{{target,&seven,1,false},
                                    {target+8U,&seven,1,false}}};
    const a32::BlockShard storage{target-4U,target+0xffcU,blocks.data(),2};
    {
        const a32::Registry original{&storage,1,nullptr,0};
        CHECK(a32::FindBlock(original,target)==&blocks[0]);
    }
    blocks={{{target-4U,&seven,1,false},{target,&fortytwo,1,false}}};
    {
        const a32::Registry replacement{&storage,1,nullptr,0};
        CHECK(a32::FindBlock(replacement,target)==&blocks[1]);
        CHECK(a32::FindBlock(replacement,target+8U)==nullptr);
    }
    blocks={{{target-4U,&seven,1,false},{target+8U,&fortytwo,1,false}}};
    {
        const a32::Registry later{&storage,1,nullptr,0};
        CHECK(a32::FindBlock(later,target)==nullptr);
        CHECK(a32::FindBlock(later,target+8U)==&blocks[1]);
    }
}

void ReplacedShardLayoutAndBounds() {
    constexpr auto target=pc+0x3000U;
    const a32::Block block{target,&fortytwo,1,false};
    std::array<a32::BlockShard,2> shards{{
        {target-0x1000U,target-4U,nullptr,0}, {target,target+0xffcU,&block,1}}};
    {
        const a32::Registry original{shards.data(),2,nullptr,0};
        CHECK(a32::FindBlock(original,target)==&block);
    }
    shards={{{target,target+0xffcU,&block,1},
             {target+0x1000U,target+0x1ffcU,nullptr,0}}};
    {
        const a32::Registry replacement{shards.data(),2,nullptr,0};
        CHECK(a32::FindBlock(replacement,target)==&block);
    }
    // Shorter live arrays must not be indexed using unchecked cached indices.
    shards[0]={target,target+0xffcU,nullptr,0};
    {
        const a32::Registry replacement{shards.data(),1,nullptr,0};
        CHECK(a32::FindBlock(replacement,target)==nullptr);
        CHECK(a32::FindBlock(replacement,target-4U)==nullptr);
    }
    const a32::Registry no_shards{nullptr,0,nullptr,0};
    CHECK(a32::FindBlock(no_shards,target)==nullptr);
    shards[0]={target+4U,target+0xffcU,&block,1};
    {
        const a32::Registry out_of_range{shards.data(),1,nullptr,0};
        CHECK(a32::FindBlock(out_of_range,target)==nullptr);
    }
}

void RevalidatedGapAndShorterTables() {
    constexpr auto target=pc+0x6000U;
    std::array<a32::Block,3> blocks{{{target-8U,&seven,1,false},
        {target-4U,&seven,1,false},{target+4U,&seven,1,false}}};
    a32::BlockShard shard{target-8U,target+0xffcU,blocks.data(),3};
    {
        const a32::Registry first{&shard,1,nullptr,0};
        CHECK(a32::FindBlock(first,target)==nullptr); // Insertion index two.
        CHECK(a32::FindBlock(first,target)==nullptr);
        CHECK(a32::FindBlock(first,target+8U)==nullptr); // Past the last entry.
    }
    // The old miss's former left neighbor is now the actual entry; checking
    // only the cached right neighbor would incorrectly preserve the miss.
    blocks={{{target-4U,&seven,1,false},{target,&fortytwo,1,false},
             {target+4U,&seven,1,false}}};
    {
        const a32::Registry second{&shard,1,nullptr,0};
        CHECK(a32::FindBlock(second,target)==&blocks[1]);
    }
    blocks[0]={target+8U,&fortytwo,1,false}; shard.block_count=1;
    {
        const a32::Registry shorter{&shard,1,nullptr,0};
        CHECK(a32::FindBlock(shorter,target)==nullptr);
        CHECK(a32::FindBlock(shorter,target+8U)==&blocks[0]);
    }
    const a32::Block present{target,&seven,1,false};
    std::array<a32::BlockShard,2> shards{{
        {target-0x1000U,target-4U,nullptr,0},{target,target+0xffcU,&present,1}}};
    {
        const a32::Registry two{shards.data(),2,nullptr,0};
        CHECK(a32::FindBlock(two,target)==&present); // Prime shard index one.
    }
    shards[0]={target,target+0xffcU,&present,1};
    {
        const a32::Registry one{shards.data(),1,nullptr,0};
        CHECK(a32::FindBlock(one,target)==&present);
    }
}

void CollidingLookups() {
    // These PCs share a direct-map bucket; every hit must still use the live
    // registry. Both original arrays and operation words remain unchanged.
    constexpr auto a=pc+0x4000U,b=a+0x10000U;
    const a32::Block first{a,&seven,1,false}, second{b,&fortytwo,1,false};
    const a32::BlockShard sa{a,a+0xffcU,&first,1},sb{b,b+0xffcU,&second,1};
    const a32::Registry ra{&sa,1,nullptr,0},rb{&sb,1,nullptr,0};
    for(unsigned i=0;i<32;++i) {
        CHECK(a32::FindBlock(ra,a)==&first);
        CHECK(a32::FindBlock(rb,b)==&second);
        CHECK(a32::FindBlock(ra,b)==nullptr);
        CHECK(a32::FindBlock(rb,a)==nullptr);
    }
}
}
int main() {
    ReusedShardStorage();MissingThenPresent();ReusedBlockStorage();
    ReplacedShardLayoutAndBounds();RevalidatedGapAndShorterTables();CollidingLookups();
    if(failures) return EXIT_FAILURE;
    std::cout << "PASS: registry lookup hints survive host-storage reuse, replaced tables, moved indices, cached misses, layout changes and collisions; stepping/dispatch use only live records\n";
}
