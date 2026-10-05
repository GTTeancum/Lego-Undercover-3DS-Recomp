# LEGO Chase Begins — canonical recovery handoff

Updated October 5, 2026. Continue in scratch, NOT on the user's PC or Work.
Project: LEGO City Undercover: The Chase Begins (Nintendo 3DS USA), not LEGO Batman.
Historical Recovery F/J gameplay is not current proof. This native executable is
headless startup reconstruction, not a playable port. POST THIS FILE EVERY WORK TURN.

## Published implementation and verified baseline

Implementation: `dfe787298ad8c1ae5ff3cde1fd8a4630bf9536cd`.
Exact tested/uploaded implementation tree: `59d5e04bf5840af94594d3fcf06cce5fe5e4d5d7`.
Baseline main: `ffd6b88e2e9452d18a054222ac23e16f2b440939`.
Baseline tree: `88041f1d514ec879e3bb118fbacd62e1c16a172b`.
All 561 prior manifest entries and 282 indexed source files verified. The full GCC
baseline reproduced the original DisplayTransfer stop at round 170 before editing.
Code/AOT and prepared RomFS were restored and hash-verified without user reuploads,
original CCI extraction or user-PC access. Local Git is a snapshot/index, NOT remote
history. Publish through connector with the verified current parent and force=false.
The attached final receipt records the report/delivery commit and backup filename.

## Required distinction: default strict versus explicit reference VRAM

Baseline inspection found the transfer's VRAM source unmapped on the CPU bus and
the destination fully writable. It did NOT find recovered source pixels.

New CLI option: `--gpu-vram-mode reference-zero`. This explicitly creates a 6 MiB
DEVICE-OWNED VRAM bank before service sessions connect, following the inspected
pinned HLE's `MemorySystem::Impl` zero-initializing `make_unique<u8[]>(VRAM_SIZE)`.
It is not measured hardware reset state, recovered console VRAM, original artwork
or a per-transfer zero substitution. No CPU mapping is added. The bank persists,
reads use its actual current bytes, and configuration cannot replace an active bank.

Without this option the bank stays unconfigured. The pending transfer remains an
untouched host stop at round 170, with no output, dequeue or PPF. Invalid mode names
and missing arguments are CLI errors. Do not silently enable the reference policy.

## Actual progress and next stopping point

The original pending packet is:

```text
01000103 1f5f8000 14013950 00800080 00800080 00004400 00000000 00000000
```

It now executes under the explicit option: 128 x 128 RGBA4, tiled Morton 8x8 source
to linear destination, 32768 bytes. Source selector 0x1F5F8000 uses device physical
0x185F8000. Destination VA 0x14013950 maps through the supported linear-heap slice
to PA 0x20013950. Transfer registers are populated, staged bytes committed, trigger
cleared and PPF interrupt ID 4 queued only after byte effects succeed.

Queue header 0x00000102 becomes 0x00000003. Source is unchanged, output matches an
independent inverse-Morton replay, all shared-page changes are independently checked,
and PICA upload state is preserved. The retained event is signaled after transfer.
Thread 3 was in address arbitration, not an event wait at that instant: do NOT
claim PPF immediately woke it. The prior genuine P3D event-wait wake remains verified.
Kernel time stays zero; interrupt delivery is synchronous host policy, not GPU timing.

IMPORTANT: both the source bank region and original destination were already zero
in this startup. Their common SHA-256 is
`c35020473aed1b4642cd726cad727b63fff2824ad68cedd7ffb73c7cbd890479`.
The boot trace alone is not proof of nonzero-pixel conversion. Separate ROM-free
nonzero-pattern tests cover all 65536 RGBA4 encodings, non-square/tile-edge cases
and the 1 MiB bound. No game image or rendered output has been demonstrated.

With a NEW empty archive, explicit empty PTM, original RomFS and reference-zero VRAM:

```text
stop=MissingBlock pc=0x001301f8 detail=0x00000000 thread=3 dispatch_rounds=172
```

Existing gamecoin reaches the same PC at round 164, skipping file initialization.
Default strict VRAM stops at DisplayTransfer round 170; no RomFS retains the old
OpenFileDirectly round 79; fresh without empty PTM retains step-count round 71.
Keep those branches separate. Existing-file startup is not gamecoin readback or
proof of gameplay save/load. No rendering, main menu, shader execution, audio,
controls, completed initializer count or gameplay is established. Rounds are not frames.

## Next exact work: a block-interior return, NOT missing original code

The unchanged private AOT page already contains:

```text
{0x001301E8U, kLegoPage00130Ops + 121U, 11U}
```

0x001301F4 is raw 0x112FFF30 (conditional BLXNE r0). The callback return address
0x001301F8 is word offset 4 inside that recorded block; its raw word is 0xE5D41076.
Current vendor `FindBlock()` accepts only exact block starts. No dispatcher or AOT
fix was applied this turn. Next inspect `a32_runtime.cpp` dispatch/ExecuteBlock and
the original callback path, then support validated recorded suffix entry or correct
generation. Do not restart the prefix, jump over the missing return, invent an
instruction or add a catch-all interpreter/success path. Test alignment, gaps,
block ends, indirect conditional calls, exact starts and packed op metadata.

Then run original code again with the four explicit inputs/options below and a NEW
empty test root. The next actual request determines subsequent work. Preserve PICA
uploads, real P3D behavior and completed byte transfer; do not fabricate vblank.

## Implementation scope and safety policies

Only exact transfer flags 0x4400, equal nonzero whole-tile dimensions, 8-byte-aligned
VRAM input and private writable linear-heap output are supported. Maximum staged
bytes is 1 MiB (host work bound, NOT console capacity). RGBA4 decoding/re-encoding
is a per-pixel identity, so the two source bytes are moved according to Morton
addressing. No scaling, flip, crop, other format/layout, texture copy, fill, active
MMIO trigger, drawing, generic GPU physical mapping or CPU VRAM mapping is added.

The whole eligible queue batch is preflighted/staged before any dequeue, register,
output or IRQ mutation. Invalid/unsupported tails roll back valid prefixes. A later
PICA list depending on an earlier staged transfer output remains unsupported;
dependency forwarding is not implemented. Independent mixed PICA/transfer batches
preserve register and IRQ order, with 15-packet wrap tested. Whole-batch atomicity
and stricter malformed-input stops are host policy, not console error precedence.

New device-write preparation preallocates exclusive epoch metadata without changing
bytes or existing tokens. Commit performs no allocation, writes preflighted private
backing, and invalidates affected reservation granules. Shared service-backed,
protected, unmapped and cross-region output is rejected. IPC/GSP backing aliases
are rejected. The real relay ring and event are checked before effects; full or
invalid relay rings stop. No unconditional interrupt/completion signal is used.
A hypothetical commit-invariant failure explicitly reports partial state after
dequeue and emits no PPF; none occurred in the tested synchronous path.

13 implementation files changed: CMakeLists.txt, cmake/LEGOHostRuntime.cmake,
src/host/main.cpp, ctr_memory.h, ctr_runner.h/.cpp, new ctr_device_write.cpp,
gsp_gpu_service.h, gsp_command_queue.cpp, new gsp_display_transfer.h/.cpp, and two
new display/queue transfer suites. Runtime paths use src/runtime/, service paths
src/services/. Vendor, production IPC, ctr_kernel.cpp/scheduler, ctr_memory.cpp,
PICA implementation and filesystem backends are unchanged. No Windows/macOS build.

## Validation and exact evidence

Full GCC and Clang native builds link all 599 unchanged private AOT page units.
Final GCC 30/30 CTest, Clang 30/30 and Clang ASan/UBSan 30/30 ROM-free suites pass,
leak checking and halt-on-error enabled. Fourteen production scenarios produce
byte-identical compiler logs: fresh, existing, strict-no-vram, invalid-vram-mode,
missing-vram-mode, mode-off, rtc-next-day, no-romfs, no-root, missing-archive,
invalid-root, invalid-mode, missing-romfs and wrong-romfs-size.

All 603 AOT members (599 page files), original code and raw RomFS remain unchanged.
Full IVFC block verification was not repeated; raw SHA matches the prior verified
image. Registry 111043 blocks / 545111 words is STATIC inventory, not executed CPU
count. Independent replay rechecks the prior PICA list: 6590 writes, 4096 program
uploads, 96 uniform vectors and 1792 lighting-table writes, with genuine P3D wake.
Seven original RomFS metadata reads still match all 4892 bytes. The guest creates,
writes and closes its own 20-byte gamecoin.dat, default-clock SHA:
`970a8b30f57b772c2c1c5686e634b9e4ab7055b43caec90e64076f81ed08b4b6`.

Current private evidence: `transfer-checkpoint/`. Final receipt final-finished.json
records zero for every build/test/trace/proof/matrix step. Tests: ctest-gcc.txt,
ctest-clang.txt, ctest-asan.txt. Matrix: validate.py and validation-summary.json.
Proofs: transfer-proof.json, verify_transfer.py, submit-proof.json and
verify_inherited_submit.py. trace_transfer.py builds a logging-only alternate IPC
object; production IPC is unchanged. Private captures include the original PICA
list, GPU/upload/page snapshots and transfer input/output; do not publish raw bytes.
Trace/native binaries and .o objects are excluded from the source/log archive.

Public: reports/recovery-host/GSP-DISPLAY-TRANSFER.md, TRANSFER-FRESH-GCC.txt,
TRANSFER-EXISTING-GCC.txt, TRANSFER-STRICT-GCC.txt, TRANSFER-PROOF.json and
TRANSFER-VALIDATION.json. A validation driver exceeded one container call's timeout
after passing builds/proofs; the complete finite rerun passed all steps. Setup
failures are recorded in interrupted-validation-note.txt, not hidden or called tests.

## Inspected primary references

Pin: azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
- src/core/memory.cpp: explicit device VRAM allocation/zero-init, blob b5eb9b829293cbbc722111e28a3c2f0e93824d86.
- src/video_core/gpu.cpp: transfer routing, physical conversion, trigger/PPF order, blob 40f29fea0867b0e8cf4d89a8753ddabf02c3b54b.
- src/video_core/renderer_software/sw_blitter.cpp: dimensions/format/layout, blob 0a68afd07c3e22fdfaaf441d4744ef79abb2a86f.
- src/video_core/utils.h: Morton addressing, blob 5ad06ef398300162166ca339b9ffe8ebfd872919.
Earlier regs_external.h and gsp_interrupt.h define RGBA4 and PPF=4. Exact reference
receipt: transfer-checkpoint/references.json. These are pinned HLE semantics, not
blanket hardware parity. Do not use mutable upstream to silently alter the baseline.

## Scratch paths and commands

Root: /mnt/data/lego_recovery/. Source repo/; private AOT generated2/;
code restored/code.bin; raw RomFS game/prepared-romfs/romfs.bin; prepared raw parts
romfs-library-roundtrip/. Builds build-gcc/, build-clang/, build-asan/.
Current evidence transfer-checkpoint/; previous submit-checkpoint/ and queue-checkpoint/
are retained. Historical register material remains in historical/, never over repo/.
Final NEW matrix root: private-state/transfer-validation.31ss8qc0/. Other transfer-*
roots belong to this turn's diagnostics; trace-run.json records its exact path.
These are newly guest-created TEST files, not recovered NAND. Older roots remain
untouched; the paired validator resets only its owned, byte-verified fresh files.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
mkdir -p private-state
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/private-state/block-return-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty --romfs game/prepared-romfs/romfs.bin --gpu-vram-mode reference-zero
```

Expected diagnostic exit 3: MissingBlock at 0x001301F8, thread 3, round 172.
Omit the VRAM option to reproduce strict DisplayTransfer stop, not the new boundary.
Use clang++ for Clang; omit LEGO_AOT_DIR for ROM-free tests. Sanitizer commands:
transfer-checkpoint/build_remaining.py; final driver: final_validate.py.

## Durable recovery and continuing mandate

GitHub: GTTeancum/Lego-Undercover-3DS-Recomp main. Library: /LEGO-Chase-Recovery/.
Restore latest source/evidence archive and handoff, NOT historical pending source.
Verify CHECKPOINT-MANIFEST.json using verify_checkpoint.py; unpack repo/logs under
the root above. SOURCE-INDEX.json records exact tracked paths, Git blob identities
and modes including ignored-but-tracked logs. Reconstruct the index faithfully;
its local snapshot history must NEVER replace the remote history.

Private inputs have separate backups: code.bin, unchanged
LEGO-Chase-current-AOT-599pages-2026-10-03.tgz, and Prepared-RomFS/ two raw parts plus
romfs-parts.json. Put code in restored/; unpack AOT in the root (creates generated2/).
Prepared parts are 402653184 and 366526464 bytes. Do not repeat original CCI extraction.

```sh
mkdir -p game/prepared-romfs
python repo/tools/restore_romfs_parts.py game/romfs-persistence/romfs-parts.json game/prepared-romfs/romfs.bin
python repo/tools/verify_romfs.py game/prepared-romfs/romfs.bin
```

Restore refuses existing output. Verify rather than overwrite it. Raw RomFS is
769179648 bytes, view offset 4096 and size 769175552; do not expose IVFC prefix as
filesystem header or drop trailing tables. SHA:
6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf.
Code SHA: 5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
AOT SHA: 2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9.
Historical CCI SHA: 3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525.
Original six archive parts in Game-archive/ remain an alternate recovery path.

Scratch may reset. Library inputs/checkpoints and GitHub are recovery paths;
Actions source snapshots expire after 30 days. Keep original game bytes, private
AOT/native binaries/test captures OUT of public Git. Update this canonical handoff
and POST A DOWNLOADABLE COPY EVERY WORK TURN, plus a durable source/log checkpoint.
Never fabricate push, CI, persistence, rendering or gameplay success. A new chat
must be able to restore, rebuild, reproduce the actual boundary and continue.

## Hosted CI status at report publication

Actions run `37369854262` targets implementation `dfe7872`. Clang completed
successfully. The GCC job is still queued, with no test result yet. Local full GCC
and Clang builds and all 30 suites passed as documented above; do not report a
hosted GCC success without checking that job. The final attached receipt retains
the latest status actually observed.
