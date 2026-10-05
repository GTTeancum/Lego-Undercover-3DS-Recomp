# RGBA4 DisplayTransfer and explicit VRAM provenance

Checkpoint: October 5, 2026. Implementation `dfe787298ad8c1ae5ff3cde1fd8a4630bf9536cd`.
Complete tested/uploaded implementation tree: `59d5e04bf5840af94594d3fcf06cce5fe5e4d5d7`.
Base: `ffd6b88e2e9452d18a054222ac23e16f2b440939`.

## Actual transfer and next stop

The original code submits this packet at the owner's queue index 2:

```text
01000103 1f5f8000 14013950 00800080 00800080 00004400 00000000 00000000
```

It is a 128 x 128 RGBA4 transfer from Morton 8x8 tiled VRAM to linear FCRAM,
32768 bytes. Source VA selector 0x1F5F8000 corresponds to device PA 0x185F8000;
destination 0x14013950 corresponds to PA 0x20013950. Baseline inspection found
no CPU mapping for the VRAM source and complete writable destination backing.

Default behavior remains strict: no configured VRAM means no transfer, dequeue
or PPF notification. With explicit `--gpu-vram-mode reference-zero`, the launcher
creates a device-owned 6 MiB bank using the inspected pinned HLE's zero-initializing
`make_unique<u8[]>(VRAM_SIZE)` policy. This is NOT recovered console memory, a
measured hardware reset, original game artwork or per-transfer pixel substitution.
It creates no CPU mapping and never resets the bank on a transfer.

Under that option, the original transfer now stages/detiles source bytes, commits
them to the actual destination, updates the transfer registers and only then
publishes PPF interrupt 4 to the real owner's relay. Queue 0x00000102 becomes
0x00000003. The destination matches an independent inverse-Morton replay and the
source is unchanged. PICA upload state remains unchanged by this transfer.

Important evidence limit: BOTH the configured source and original destination
were already zero in this startup. Their common SHA-256 is
`c35020473aed1b4642cd726cad727b63fff2824ad68cedd7ffb73c7cbd890479`.
That boot trace alone cannot distinguish correct detiling from a no-op on pixels.
Separate nonzero synthetic-image suites verify the byte transformation, including
all 65536 RGBA4 encodings, non-square dimensions, tile boundaries and a 1 MiB case.

The PPF event is signaled after commit. Thread 3 was in address arbitration rather
than an event wait at that instant, so this is NOT a claim that PPF immediately woke
that thread. The earlier genuine P3D waiter wake is independently reverified.
Synchronous IRQ delivery does not advance guest time and is not hardware timing.

```text
stop=MissingBlock pc=0x001301f8 detail=0x00000000 thread=3 dispatch_rounds=172
```

The existing-file branch reaches the same PC at round 164. Without the VRAM option,
the new handler preserves the prior DisplayTransfer boundary at round 170. No
rendered frame, main menu, shader execution or gameplay is established.

## Bounded implementation

Only flags 0x4400, equal nonzero whole-tile dimensions, aligned VRAM input and
private linear-heap output are supported. The 1 MiB cap is host work policy, not a
console capacity. Other formats, scaling, flipping, cropping, texture copy, general
GPU addressing, CPU VRAM mapping and active MMIO triggers remain unsupported.

Whole eligible queue batches are staged before mutation. Response aliases, shared
service output, protected/unmapped/cross-region spans, invalid/full IRQ rings,
unsupported later packets and dependent later command lists stop without committing
a prefix. Transfer-to-PICA dependency forwarding is not implemented. Independent
PICA/transfer batches preserve register and IRQ order. Overlapping transfer outputs
are committed in queue order from independently staged device-source bytes.

Device-write preparation allocates reservation metadata before dequeue without
changing byte data or existing tokens. Commit performs no allocation, writes to
preflighted private backing and invalidates precisely affected granules. General
shared backing and cross-region device writes are rejected. A commit-invariant
failure would explicitly report the already-dequeued partial state and send no PPF;
it is not disguised as success. No such failure occurred in the validated path.

## Validation

GCC and Clang full native builds link all 599 unchanged private AOT page units.
GCC 30/30, Clang 30/30 and Clang ASan/UBSan 30/30 ROM-free suites pass. Fourteen
production startup scenarios have byte-identical compiler logs. Source identities
match for original code, all 603 AOT members and raw RomFS. Full IVFC block checking
was not repeated; raw SHA matches the previously verified image.

Tests cover nonzero conversion, every RGBA4 encoding, strict dimensions/ranges,
permissions/aliases, real retained events, PPF ring wrap, 15-packet batches, full
handles, unsupported-tail rollback, mixed PICA/transfer order and exclusive epochs.
Independent proof checks every transfer/relay/register byte and replays the prior
PICA initialization list and all seven original RomFS reads (4892 bytes).

Final evidence is in private `transfer-checkpoint/`: final-finished.json,
ctest-gcc/clang/asan.txt, validation-summary.json, transfer-proof.json,
submit-proof.json and the logging-only trace scripts/captures. Production IPC,
vendor runtime, scheduler, PICA execution and filesystem backends are unchanged.
No Windows/macOS build is claimed. A finite validation driver exceeded one container
call's timeout after successful builds/proofs; the complete rerun passed all steps.

## Next: a covered block-interior return

The unchanged page records `{0x001301E8, ops + 121, 11}`. The missing entry
0x001301F8 is word offset 4 INSIDE it, following conditional BLXNE r0 at 0x001301F4.
Current vendor FindBlock accepts exact start PCs only. No dispatcher correction was
applied here. Implement validated interior-entry/suffix handling or correct block
generation without reexecuting the prefix, skipping code or accepting unknown gaps.

## Primary reference

All reference paths are pinned to azahar-emu/azahar commit
`86a9f9236ae42bb5a2b995dbc933d599d8ea07ac`: `src/core/memory.cpp` for device allocation;
`src/video_core/gpu.cpp` for routing and PPF ordering; `renderer_software/sw_blitter.cpp`
and `utils.h` under `src/video_core/` for transfer flags and Morton addressing.
Exact blob identities are recorded in private `transfer-checkpoint/references.json`.
