# Recorded A32 callback-return suffixes

Checkpoint: October 5, 2026. Implementation `f8f7ad5117e8d8f0ca6de940e1f72746f8ac22d4`;
exact tested/uploaded tree `4635ba40d8ddfed32006fe89a68ab02f173b570f`.
Baseline main `2aa22c0d35c612c796a090921c2f5f6c243203b0`, tree
`4c121762330199bc7c0a1fbd5957dd8ba30c968a`.

## Recovered baseline, not a duplicate VRAM implementation

The previous chat attachment was DISPLAY-TRANSFER-PENDING. Its 769 manifest entries
and 292 source blobs were verified and preserved separately. Fresh main was newer:
it already implemented device-owned reference-zero VRAM and the original transfer.
Those features are inherited, NOT this checkpoint's new work. The pending archive
remains recoverable in Library Preserved-Pending-Transfer and the private backup.

The failed source-snapshot job was retried successfully: run 37370604146, artifact
11371699542. Its source ZIP/tar checksums and complete reconstructed Git tree matched
the recorded baseline before editing. No user upload or CCI extraction was needed.
The unmodified baseline reproduced MissingBlock at 0x001301F8, thread 3, round 172.

## Cause and correction

The return PC is word 4 of an existing eleven-word block at 0x001301E8. Conditional
BLXNE r0 at 0x001301F4 saves this return address. The next recorded instruction is
raw 0xE5D41076 with packed metadata 0xE0D. No original instruction bytes are absent.
The exact-start FindBlock API could not resolve that interior return.

Dispatch now tries a bounded recorded suffix after an exact lookup miss, only in
A32 state. The view starts at the actual PC and points to the remaining immutable
PackedOps. ExecuteBlock, raw values and metadata are unchanged. It does not replay
the prefix, bypass the return or decode new code. Its native-candidate flag is false:
an optimization for the original entry is not assumed valid for arbitrary suffixes.

Checks reject unaligned entries, Thumb suffix resolution, missing operation storage,
zero/overflow/cross-shard extents, gaps, end addresses and overlap with a subsequent
record. Host-owned registry arrays retain their immutable/sorted/non-overlap contract;
the loader validates global inventory and original raw-word identity. This is not
validation of arbitrary host pointers or a general Thumb/interworking implementation.
Exact lookup/cache, opcode backends, production IPC, GPU services and scheduler stay
unchanged. A suffix consumes one existing block budget; the production limit remains
unchanged. No successful missing-block fallback is installed.

## Original execution and next boundary

A logging-only trace observes five entries at 0x001301F8, each with the correct
remaining seven operations. After the inherited display transfer, original code
submits three more supported PICA lists of 32, 176 and 48 bytes, with their preceding
CacheFlush packets. No drawing backend was added.

Fresh production startup now stops at round 193 (existing-file startup: 185):

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=193
last_ipc_session=gsp::Gpu request_header=0x000c0000
host_ipc_error=GSP queue packet requires unimplemented GPU execution
queue_header=00000109
packet=01000102 1f070800 00000000 1f138800 00000000 00000000 000c8000 02010201
```

This is MemoryFill, ID 2, slot 0/index 9/count 1. The first requested range is
0x1F070800..0x1F138800 (819200 bytes), value zero. The second start is zero; its
residual end is 0xC8000. Both control halfwords are 0x0201. The packet is NOT consumed.
Across rejection, the full GSP page, registers, upload arrays and device VRAM are
unchanged; the event remains unsignaled and kernel time zero. MemoryFill semantics
and PSC notification remain next work, not an implemented operation.

The explicit --gpu-vram-mode reference-zero policy remains necessary. It describes
an inherited reference-HLE cold bank, not recovered console pixels or artwork.
Default strict mode still stops at DisplayTransfer, round 170. No rendered frame,
main menu, gameplay, audio, controls or accurate GPU timing has been demonstrated.

## Validation

Full GCC and Clang builds link all 599 original AOT pages. All 31 CTest suites pass
with GCC, Clang and Clang ASan/UBSan (ROM-free, leak checking and halt-on-error).
Fourteen production scenarios have identical compiler logs and exits, preserving
strict/unconfigured and invalid-input branches. No native test failure was hidden.
The new suite covers conditional callbacks without duplicated prefix effects, every
word entry, PC-relative instructions, packed metadata, exact faults, budgets, hooks,
native-candidate isolation, malformed extents and no invented platform side effects.

A separate diagnostic partitions all 545111 original PackedOps into single-word,
exact-entry blocks. Compared with the retained block layout, it reaches identical
captured CPU/thread/GPU state, GSP page, next packet, and all 16388 readable CPU pages
(67125248 bytes, with matching permissions). Aggregate page digest:
4cb50710efe41be8a45ae239d9311015862c08a7398c2cbd2a781e31539eb605.
This checks block-partition equivalence; both runs share instruction backends and
are NOT independent ARM emulation or proof of complete kernel/hardware correctness.
The logging-only trace independently reaches those same final snapshots.

The initial one-word diagnostic exhausted its smaller block budget before the target,
then its target-specific dumper rejected that early stop. Those logs are retained.
Both final diagnostic variants used 100000000 blocks and succeeded at the same stop;
ordinary production runs keep the old limit. An unavailable streaming-exec attempt
ran no build; finite drivers performed the recorded builds. Source-snapshot retry
and diagnostic-budget correction are recovery/setup events, not suppressed tests.

Final identity checks match code.bin, raw RomFS and all 603 AOT archive members.
Full IVFC block checking was not repeated; raw SHA matches the previously verified
image. Registry inventory counts are not executed instruction counts or frames.

Public summaries: BLOCK-PROOF.json, BLOCK-VALIDATION.json and BLOCK-*-GCC.txt.
Complete scripts, tests, traces and PRIVATE state snapshots: block-checkpoint/ in
the private source/evidence backup. No game bytes, private AOT or binaries are public.
The only implementation paths changed are CMakeLists.txt, the new block-entry test,
vendor a32_runtime.cpp and LOCAL-PATCHES.md. No generation or opcode change occurred.

References: original verified code/AOT records; the local immutable PackedOp/Block
ABI, Dispatch and ExecuteBlock; Arm's primary explanation of branch/call return
semantics (Branch and Call Sequences Explained). Next fill implementation should
reinspect the pinned azahar-emu/azahar revision
86a9f9236ae42bb5a2b995dbc933d599d8ea07ac rather than guessing transfer/IRQ behavior.

## Hosted CI confirmation

GitHub Actions run `37379880223` on implementation `f8f7ad5` completed successfully
for both GCC and Clang jobs. These hosted suites are ROM-free, not original-game runs.
