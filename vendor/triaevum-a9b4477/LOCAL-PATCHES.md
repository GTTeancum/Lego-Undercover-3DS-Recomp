# Local recovery patch — legacy DMB metadata

Framework baseline: TriAevum a9b447709d4405848d75352354891059cebb9ff8.
This directory is no longer byte-identical to that baseline: the following local
compatibility patch is intentional and tested.

File: recomp/a32_runtime.cpp, ExecuteBlock, Opcode::CoreAlu.
Baseline file SHA-256: c1ff86cb41aaa857c2c2ca4fc36a20c8dc852f716e164936271e283ee28d9bb0
Patched file SHA-256: bbd35e12f431ada54b96134310602c2a23ab80e7ffb11acdae69d844891e1eeb

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
