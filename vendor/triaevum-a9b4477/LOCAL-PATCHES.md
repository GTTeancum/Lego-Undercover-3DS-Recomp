# Local recovery patch — legacy DMB/DSB metadata

Framework baseline: TriAevum a9b447709d4405848d75352354891059cebb9ff8.
This directory is no longer byte-identical to that baseline: the following local
compatibility patch is intentional and tested.

File: recomp/a32_runtime.cpp, ExecuteBlock, Opcode::CoreAlu.
Baseline file SHA-256: c1ff86cb41aaa857c2c2ca4fc36a20c8dc852f716e164936271e283ee28d9bb0
Prior DMB/DSB-patched file SHA-256: cb8d5db7bc79fc2c5920993a46204c0d29b23e3bdb81f23dedacaa252247ea5f

The private LLVM-derived page archive classifies the observed CP15 DMB form as
CoreAlu. Its raw word at guest PC 0x00117BC0 is 0xEE074FBA. The pinned
recomp/a32_core.cpp already recognizes the architectural form and executes a
sequentially consistent native fence for synchronous guest-memory transactions.

The local dispatch patch routes only mask 0x0FFF0FFF == 0x0E070FBA, with normal
condition encoding and Rt != PC, from CoreAlu to the existing system backend.
Condition evaluation still happens in ExecuteBlock. The normal instruction loop
continues to the remaining operations in the block; no guest PC skip or general
unsupported-instruction success fallback is installed.

An attempted fallback adapter returned at pc+4, but FindBlock accepts exact block
starts, so that adapter stranded the block tail. It was rejected and is not in
the delivered runner. Tests execute a barrier followed by another operation in
the same block, and verify other unknown MCR forms still stop.

The LLVM generator now classifies this known form as CoreSystem for future
regeneration. The 603 private archive files (599 game pages) were not regenerated
or modified. Their raw words are still checked against the verified code image.
No other pinned runtime file was changed in this checkpoint.

## October 5 register-setup continuation: DSB

The original queue-submission routine reaches PC 0x00248404, raw 0xEE071F9A.
The unchanged private archive also categorizes this legacy DSB as CoreAlu.
The already-existing ExecuteCoreSystem handler recognizes 0x0E070F9A under
the same mask and emits the same conservative native fence. The local routing
predicate now admits DSB as well as DMB, still rejecting condition 0xF and Rt=PC.
This does not skip the instruction: it executes the native fence and continues
the current block. No original code/page, fence implementation or opcode is changed.

ctr_legacy_barrier_test covers both categories, all valid Rt fields, condition
pass/fail, unchanged CPU/exclusive state, in-block continuation, unknown-form
fallbacks and synchronous shared-memory stores without event/time advancement.
The obsolete DSB-negative case in ctr_clock_test is replaced by an unknown
CP15 form; valid DSB coverage now resides in the dedicated suite.

The previously documented generator correction covers DMB. DSB regeneration is
NOT changed in this checkpoint; the retained archive is handled by the narrow
runtime compatibility route. This remains a LOCAL patch, not upstream parity.

## October 5 callback-return continuation: recorded A32 suffix entries

The original callback returns to 0x001301F8, word 4 of the existing eleven-word
block at 0x001301E8. No original code or private PackedOp metadata is missing.
FindBlock retains its exact-start API. Dispatch now resolves an exact miss to a
bounded recorded suffix only in A32 state, using the actual requested PC and
remaining operations. The view is local and non-owning; immutable arrays are not
modified, and a native-candidate flag for the prefix is not inherited by a suffix.
There is no catch-all instruction decoder or successful missing-block fallback.

The helper rejects unaligned entry, Thumb state, gaps, end/overflow, missing
operation storage, cross-shard extent and an overlapping following record. The
existing immutable/sorted registry contract still applies: global validation and
raw-image matching occur in the host before execution, not in each lookup.
Exact-start lookup, explicit callbacks and unsupported operation handling are
otherwise unchanged. PC-relative operands use the actual suffix instruction PC.
Block budgets still count dispatched blocks, not instruction words.

ctr_block_entry_test covers conditional indirect call/return without prefix replay,
every retained word, repeated lookups, PC-relative access, packed condition/link
metadata, precise faults, unsupported fallback, exact/missing entries, callbacks,
native-candidate isolation, budgets, malformed extents and no invented events/time.
A diagnostic one-word partition of every ORIGINAL PackedOp reaches identical
final CPU/thread/GPU state and all 16388 readable CPU pages (67125248 bytes).
That oracle shares instruction backends and is not independent ARM emulation.
Neither the private AOT archive nor any opcode backend is changed by this slice.

Current recomp/a32_runtime.cpp SHA-256: 952fac140b5d860cd472c56c313231c4dcc8eb7b1719caeaa341503015030773

## October 7: bounded VFPv2 single-precision short vectors

The original program reaches supported scalar arithmetic with FPSCR LEN=2,
which denotes three short-vector iterations. The previous global vector-mode
guard rejected even scalar-bank destinations. The new bounded dispatch follows
ARM DDI 0274H sections 2.7 and 3.4.2: circular eight-register banks, scalar
destinations S0..S7, scalar Fm broadcasts, advancing Fn, valid stride encodings,
and scalar-only conversions/comparisons. It reuses the unchanged scalar integer
floating-point algorithms. Sticky exception flags accumulate; LEN/STRIDE and
other controls survive; the architectural PC advances once.

Unsupported encodings, double-precision vectors, enabled exception trapping and
shifted cross-lane source/destination overlap still stop without partial output.
Only aligned per-iteration overlap is supported. This is not full VFP pipeline,
hardware latency, trap/support-code or general alias-hazard emulation. Original
code.bin and every saved AOT page remain unchanged. No runtime decoding fallback
is introduced. The private original-run trace and public synthetic tests distinguish
actual game execution from finite test fixtures. Current file hashes are recorded
under checkpoint_patches in provenance.json; older provenance remains historical.
