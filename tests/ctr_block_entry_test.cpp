#include "runtime/ctr_runner.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace {
using namespace lego::ctr;
using namespace lego::ctr::a32;
int failures = 0;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL " << __LINE__ << ": " #x "\n"; ++failures; } } while (0)
constexpr PackedOp Op(std::uint32_t raw, Opcode opcode,
                      Condition condition = Condition::Al, std::uint8_t flags = 0) {
    return {raw, EncodeMetadata(opcode, condition, flags)};
}
constexpr PackedOp stop = Op(0xEF00007F, Opcode::Svc);

void ConditionalIndirectReturn() {
    // A conditional BLX can return to the middle of a retained block. Replaying
    // the prefix would increment r5 twice (or call the callback again).
    static constexpr std::array<PackedOp, 6> main_ops{
        Op(0xE2855001, Opcode::Add), Op(0xE3510000, Opcode::Cmp),
        Op(0x112FFF30, Opcode::BranchReg, Condition::Ne, Link),
        Op(0xE5D42001, Opcode::CoreMemory), Op(0xE2833001, Opcode::Add), stop};
    static constexpr std::array<PackedOp, 2> callback_ops{
        Op(0xE2866001, Opcode::Add), Op(0xE12FFF1E, Opcode::BranchReg)};
    static constexpr std::array<Block, 2> blocks{{
        {0x00100000, main_ops.data(), main_ops.size()},
        {0x00100100, callback_ops.data(), callback_ops.size()}}};
    static constexpr BlockShard shard{0x00100000, 0x00100FFC, blocks.data(), blocks.size()};
    static constexpr Registry registry{&shard, 1, nullptr, 0};
    GuestMemory memory;
    CHECK(memory.Map(0x08000000, 16, MemoryPermission::Read | MemoryPermission::Write));
    CHECK(memory.Write8(0x08000001, 0xA7));
    for (auto take_call : {false, true}) {
        GuestState cpu{}; cpu.cpsr = 0x10; cpu.r[0] = 0x00100100;
        cpu.r[1] = take_call; cpu.r[4] = 0x08000000;
        auto result = Dispatch(registry, 0x00100000, cpu, memory, nullptr, nullptr, 8);
        CHECK(result.kind == ExitKind::Svc && result.pc == 0x00100014 && result.detail == 0x7F);
        CHECK(cpu.r[5] == 1 && cpu.r[6] == unsigned(take_call));
        CHECK(cpu.r[2] == 0xA7 && cpu.r[3] == 1);
        if (take_call) CHECK(cpu.r[14] == 0x0010000C);
        CHECK(FindBlock(registry, 0x00100000) == &blocks[0]);
        CHECK(FindBlock(registry, 0x0010000C) == nullptr); // Exact API unchanged.
    }
}

void EveryRecordedWordAndBudget() {
    static constexpr std::array<PackedOp, 9> ops{
        Op(0xE2800001, Opcode::Add), Op(0xE2800001, Opcode::Add),
        Op(0xE2800001, Opcode::Add), Op(0xE2800001, Opcode::Add),
        Op(0xE2800001, Opcode::Add), Op(0xE2800001, Opcode::Add),
        Op(0xE2800001, Opcode::Add), Op(0xE2800001, Opcode::Add), stop};
    static constexpr Block block{0x00101000, ops.data(), ops.size(), true};
    static constexpr BlockShard shard{0x00101000, 0x00101FFC, &block, 1};
    static constexpr Registry registry{&shard, 1, nullptr, 0};
    GuestMemory memory;
    for (unsigned repeat = 0; repeat < 4; ++repeat) for (unsigned offset = 0; offset < ops.size(); ++offset) {
        GuestState cpu{}; cpu.cpsr = 0xA0000010; cpu.fpscr = 0x23000010;
        cpu.vfp[5] = 0x7FC01234; cpu.thread_pointer = 0x1FF82000;
        cpu.exclusive_address = 0x08001234; cpu.exclusive_token = 91;
        cpu.exclusive_size = 4; cpu.exclusive_valid = true;
        const auto original = cpu;
        const auto pc = block.pc + 4 * offset;
        auto result = Dispatch(registry, pc, cpu, memory, nullptr, nullptr, 1);
        CHECK(result.kind == ExitKind::Svc && result.pc == block.pc + 32);
        CHECK(cpu.r[0] == 8 - offset && cpu.cpsr == original.cpsr);
        CHECK(cpu.vfp == original.vfp && cpu.fpscr == original.fpscr && cpu.thread_pointer == original.thread_pointer);
        CHECK(cpu.exclusive_address == original.exclusive_address && cpu.exclusive_token == 91 && cpu.exclusive_valid && cpu.exclusive_size == 4);
        CHECK(block.pc == 0x00101000 && block.ops == ops.data() && block.op_count == 9 && block.native_candidate);
        GuestState zero{}; zero.r[0] = 13;
        result = Dispatch(registry, pc, zero, memory, nullptr, nullptr, 0);
        CHECK(result.kind == ExitKind::BlockLimit && result.pc == pc && zero.r[0] == 13);
    }
    // A suffix consumes one dispatcher block budget, then an adjacent exact block
    // begins on the next dispatch. No boundary word is dropped or re-executed.
    static constexpr std::array<PackedOp, 3> adjacent_ops{
        Op(0xE2800001, Opcode::Add), Op(0xE2811001, Opcode::Add), stop};
    static constexpr std::array<Block, 2> adjacent_blocks{{
        {0x00102000, adjacent_ops.data(), 2}, {0x00102008, adjacent_ops.data() + 2, 1}}};
    static constexpr BlockShard adjacent_shard{0x00102000, 0x00102FFC, adjacent_blocks.data(), 2};
    static constexpr Registry adjacent_registry{&adjacent_shard, 1, nullptr, 0};
    GuestState cpu{};
    auto result = Dispatch(adjacent_registry, 0x00102004, cpu, memory, nullptr, nullptr, 1);
    CHECK(result.kind == ExitKind::BlockLimit && result.pc == 0x00102008 && cpu.r[0] == 0 && cpu.r[1] == 1);
    result = Dispatch(adjacent_registry, result.pc, cpu, memory, nullptr, nullptr, 1);
    CHECK(result.kind == ExitKind::Svc && result.pc == 0x00102008 && cpu.r[1] == 1);
}

void ActualPcAndMetadata() {
    // Prefix would fault; suffix MOV reads the actual instruction PC+8. Its LDR
    // also uses the suffix instruction's PC, not the original block's address.
    static constexpr std::array<PackedOp, 5> ops{
        Op(0xE5940000, Opcode::Ldr32), Op(0xE1A0200F, Opcode::MovReg),
        Op(0xE59F3004, Opcode::Ldr32), Op(0xE3A01044, Opcode::MovImm, Condition::Ne), stop};
    static constexpr Block block{0x00103000, ops.data(), ops.size()};
    static constexpr BlockShard shard{0x00103000, 0x00103FFC, &block, 1};
    static constexpr Registry registry{&shard, 1, nullptr, 0};
    GuestMemory memory;
    CHECK(memory.Map(0x00103014, 4, MemoryPermission::Read | MemoryPermission::Write));
    CHECK(memory.Write32(0x00103014, 0xC0DEBEEF));
    for (auto z : {0U, kFlagZ}) {
        GuestState cpu{}; cpu.cpsr = z | 0x10;
        auto result = Dispatch(registry, 0x00103004, cpu, memory, nullptr, nullptr, 1);
        CHECK(result.kind == ExitKind::Svc && result.pc == 0x00103010);
        CHECK(cpu.r[2] == 0x0010300C && cpu.r[3] == 0xC0DEBEEF);
        CHECK(cpu.r[1] == (z ? 0U : 0x44U) && cpu.cpsr == (z | 0x10));
    }
    static constexpr std::array<PackedOp, 3> fault_ops{
        Op(0xE2800001, Opcode::Add), Op(0xE5941000, Opcode::Ldr32), stop};
    static constexpr Block fault_block{0x00104000, fault_ops.data(), fault_ops.size()};
    static constexpr BlockShard fault_shard{0x00104000, 0x00104FFC, &fault_block, 1};
    static constexpr Registry fault_registry{&fault_shard, 1, nullptr, 0};
    GuestState cpu{}; cpu.r[4] = 0x09000000;
    auto result = Dispatch(fault_registry, 0x00104004, cpu, memory, nullptr, nullptr, 1);
    CHECK(result.kind == ExitKind::MemoryFault && result.pc == 0x00104004 && result.detail == 0x09000000);
    CHECK(cpu.r[0] == 0 && cpu.r[15] == result.pc);
}

struct Calls { unsigned entries{}, native{}, fallback{}; std::uint32_t pc{}, raw{}, metadata{}; };
void Entry(std::uint32_t pc, GuestState&, MemoryBus&, void* context) {
    auto& calls = *static_cast<Calls*>(context); ++calls.entries; calls.pc = pc;
}
bool Native(std::uint32_t, GuestState&, MemoryBus&, ExecutionResult*, std::uint32_t, std::uint32_t*, void* context) {
    ++static_cast<Calls*>(context)->native; return false;
}
ExecutionResult Fallback(FallbackReason reason, std::uint32_t pc, const PackedOp& op,
                         GuestState&, MemoryBus&, void* context) {
    auto& calls = *static_cast<Calls*>(context); ++calls.fallback;
    calls.pc = pc; calls.raw = op.raw; calls.metadata = op.metadata;
    return {ExitKind::Unsupported, pc, reason, op.raw};
}
void HooksAndUnsupported() {
    static constexpr std::array<PackedOp, 3> ops{
        Op(0xE2800001, Opcode::Add), Op(0xE7F123F4, Opcode::Unsupported), stop};
    static constexpr Block block{0x00105000, ops.data(), ops.size(), true};
    static constexpr BlockShard shard{0x00105000, 0x00105FFC, &block, 1};
    static constexpr Registry registry{&shard, 1, nullptr, 0};
    GuestMemory memory; GuestState cpu{}; Calls calls{};
    auto result = Dispatch(registry, 0x00105004, cpu, memory, Fallback, &calls, 3,
                           Entry, &calls, nullptr, 0, nullptr, nullptr, nullptr, 0, Native, &calls);
    CHECK(result.kind == ExitKind::Unsupported && result.pc == 0x00105004 && cpu.r[0] == 0);
    CHECK(calls.entries == 1 && calls.fallback == 1 && calls.native == 0);
    CHECK(calls.pc == 0x00105004 && calls.raw == ops[1].raw && calls.metadata == ops[1].metadata);
    // Exact native candidates keep the old callback path.
    calls = {}; cpu = {};
    result = Dispatch(registry, 0x00105000, cpu, memory, Fallback, &calls, 3,
                      nullptr, nullptr, nullptr, 0, nullptr, nullptr, nullptr, 0, Native, &calls);
    CHECK(calls.native == 1 && cpu.r[0] == 1);
}

void GapsAndMalformedExtents() {
    static constexpr std::array<PackedOp, 2> ops{Op(0xE2800001, Opcode::Add), stop};
    static constexpr std::array<Block, 2> blocks{{{0x00106010, ops.data(), 2}, {0x00106030, ops.data(), 2}}};
    static constexpr Block later{0x00108000, ops.data(), 2};
    static constexpr std::array<BlockShard, 2> shards{{
        {0x00106000, 0x00106FFC, blocks.data(), 2}, {0x00108000, 0x00108FFC, &later, 1}}};
    static constexpr Registry registry{shards.data(), 2, nullptr, 0};
    GuestMemory memory;
    for (auto pc : {0x00105FFCU,0x00106000U,0x0010600CU,0x00106011U,0x00106012U,
                    0x00106015U,0x00106018U,0x0010602CU,0x00106038U,0x00107004U,
                    0x00108008U,0x00109000U,0xFFFFFFFFU}) {
        GuestState cpu{}; cpu.r[0] = 97;
        auto result = Dispatch(registry, pc, cpu, memory, nullptr, nullptr, 2);
        CHECK(result.kind == ExitKind::MissingBlock && result.pc == pc && cpu.r[0] == 97);
    }
    for (auto pc : {0x00106014U,0x00106034U,0x00108004U}) {
        GuestState cpu{}; auto result = Dispatch(registry, pc, cpu, memory, nullptr, nullptr, 2);
        CHECK(result.kind == ExitKind::Svc && result.pc == pc && cpu.r[0] == 0);
        cpu = {}; cpu.cpsr = 0x30;
        result = Dispatch(registry, pc, cpu, memory, nullptr, nullptr, 2);
        CHECK(result.kind == ExitKind::MissingBlock && result.pc == pc); // No Thumb reinterpretation.
    }
    static constexpr std::array<Block, 6> malformed{{
        {0x00110000, nullptr, 2}, {0x00112000, ops.data(), 0},
        {0x00114000, ops.data(), 1025}, {0x00116000, ops.data(), 0xFFFFFFFFU},
        {0x00118002, ops.data(), 2}, {0xFFFFFFF8, ops.data(), 3}}};
    static constexpr std::array<BlockShard, 6> bad_shards{{
        {0x00110000,0x00110FFC,&malformed[0],1}, {0x00112000,0x00112FFC,&malformed[1],1},
        {0x00114000,0x00114FFC,&malformed[2],1}, {0x00116000,0x00116FFC,&malformed[3],1},
        {0x00118000,0x00118FFC,&malformed[4],1}, {0xFFFFF000,0xFFFFFFFC,&malformed[5],1}}};
    static constexpr Registry bad{bad_shards.data(),6,nullptr,0};
    for (auto pc : {0x00110004U,0x00112004U,0x00114004U,0x00116004U,0x00118004U,0xFFFFFFFCU}) {
        GuestState cpu{}; auto result = Dispatch(bad, pc, cpu, memory, nullptr, nullptr, 2);
        CHECK(result.kind == ExitKind::MissingBlock && result.pc == pc);
    }
    static constexpr std::array<Block, 2> overlaps{{{0x0011A000, ops.data(), 8}, {0x0011A010, ops.data(), 2}}};
    static constexpr BlockShard overlap_shard{0x0011A000,0x0011AFFC,overlaps.data(),2};
    static constexpr Registry overlap{&overlap_shard,1,nullptr,0};
    GuestState cpu{};
    CHECK(Dispatch(overlap,0x0011A004,cpu,memory,nullptr,nullptr,1).kind == ExitKind::MissingBlock);
    const Registry empty{};
    CHECK(Dispatch(empty,0x00100004,cpu,memory,nullptr,nullptr,1).kind == ExitKind::MissingBlock);
}

void RunnerSuffixHasNoPlatformSideEffects() {
    static constexpr std::array<PackedOp, 3> ops{
        Op(0xE5940000,Opcode::Ldr32),Op(0xE2855001,Opcode::Add),stop};
    static constexpr Block block{0x0010A000,ops.data(),ops.size()};
    static constexpr BlockShard shard{0x0010A000,0x0010AFFC,&block,1};
    static constexpr Registry registry{&shard,1,nullptr,0};
    Kernel kernel; GuestMemory memory; NativeRunner runner(registry,memory,kernel);
    CHECK(runner.InitializeMainThread(0x0010A004));
    Handle event = 0; CHECK(kernel.CreateEvent(&event,unsigned(ResetType::OneShot))==0);
    auto object = std::dynamic_pointer_cast<EventObject>(kernel.handles().Get(event));
    auto handles = kernel.handles().OpenHandleCount(); auto threads = kernel.threads().size();
    auto result = runner.Run(1,2);
    CHECK(result.reason==RunnerStopReason::UnsupportedSvc && result.exit.pc==0x0010A008);
    CHECK(runner.live_state().r[5]==1 && kernel.now_ns()==0 && !object->signaled());
    CHECK(kernel.handles().OpenHandleCount()==handles && kernel.threads().size()==threads);
}
}
int main() {
    ConditionalIndirectReturn(); EveryRecordedWordAndBudget(); ActualPcAndMetadata();
    HooksAndUnsupported(); GapsAndMalformedExtents(); RunnerSuffixHasNoPlatformSideEffects();
    if (failures) return EXIT_FAILURE;
    std::cout << "PASS: recorded A32 suffixes preserve callback return, metadata, PC, budgets and strict gaps\n";
    return EXIT_SUCCESS;
}
