# LEGO Chase Begins — canonical recovery handoff

Updated October 5, 2026. Continue in scratch, NOT the user's PC or Work.
Project: LEGO City Undercover: The Chase Begins, Nintendo 3DS USA, not LEGO Batman.
The executable remains headless startup reconstruction. Historical Recovery F/J
claims are not current gameplay proof. POST THIS HANDOFF EVERY WORK TURN.

## Published implementation and source reconciliation

Implementation: `f8f7ad5117e8d8f0ca6de940e1f72746f8ac22d4`.
Exact tested/uploaded implementation tree: `4635ba40d8ddfed32006fe89a68ab02f173b570f`.
Baseline main: `2aa22c0d35c612c796a090921c2f5f6c243203b0`.
Baseline tree: `4c121762330199bc7c0a1fbd5957dd8ba30c968a`.
The downloadable handoff appends the final report/delivery commit and backup receipt.
Local Git is a reconstructed snapshot/index, NOT remote history. Publish with the
verified current remote parent and force=false; never force-push the local history.

The prior chat attachment DISPLAY-TRANSFER-PENDING was older than the fresh remote.
All 769 archive-manifest entries and 292 source blobs were checked and the archive
preserved separately, NOT discarded or overlaid onto the newer working repo.
Library preservation: /LEGO-Chase-Recovery/Preserved-Pending-Transfer/.
Local old source: pending-transfer-preserved/; the new backup nests its exact archive
under historical/. Never replace repo/ with that historical pending source.

The newer baseline already implemented explicit device-owned reference-zero VRAM
and the original transfer. Those are INHERITED, not this turn's implementation.
Source-snapshot run 37370604146 initially failed; a retry succeeded and produced
artifact 11371699542, downloaded as /mnt/data/LEGO-source-2aa22c0.zip. The ZIP/tar
checksums and full local source tree matched the remote before edits. The baseline
MissingBlock at 0x001301F8, thread 3, round 172 was reproduced in a full native build.
No game re-extraction, user upload or user-PC access was needed.

## New progress: recorded callback-return suffixes

Conditional BLXNE r0 at 0x001301F4 returns to 0x001301F8. That PC is word 4 inside
an existing eleven-word block starting at 0x001301E8, with seven operations left.
The return instruction is raw 0xE5D41076, PackedOp metadata 0xE0D. It was present in
the unchanged private archive; exact-start-only dispatch was the problem.

Dispatch now resolves an exact miss to a validated recorded A32 suffix. It uses the
actual requested PC and remaining immutable operations without prefix replay, code
rewriting, an interpreter fallback, or an invented instruction. FindBlock keeps its
exact-start public contract and cache. ExecuteBlock and all opcode backends are
unchanged. The temporary view is never cached and does not inherit the original
entry's native-candidate flag. Production block budgets are unchanged.

Five successful original-game entries through this suffix are recorded. Startup
then executes three additional supported PICA lists of 32, 176 and 48 bytes, with
their CacheFlush packets, and reaches the next real unsupported GPU operation.
No drawing implementation was added by this dispatcher correction.

## Current untouched boundary: MemoryFill

With a NEW empty shared archive, empty PTM, original RomFS and reference-zero VRAM:

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=193
last_ipc_session=gsp::Gpu requested_service= request_header=0x000c0000
host_ipc_error=GSP queue packet requires unimplemented GPU execution
queue_header=00000109
packet=01000102 1f070800 00000000 1f138800 00000000 00000000 000c8000 02010201
```

ID 2 is MemoryFill. The owner's slot-0 queue has index 9 and count 1. First range:
0x1F070800..0x1F138800, 819200 bytes, value zero. The second start is zero with a
residual end 0xC8000. Both control halfwords are 0x0201. It remains UNEXECUTED and
queued. The rejected call leaves the full GSP page, GPU registers/upload state and
device VRAM unchanged, event unsignaled and kernel time zero. No fill or PSC
notification was manufactured. Rounds and packet counts are not frames.

Existing-file startup reaches the same packet at round 185, skipping initialization.
Default strict VRAM still stops at DisplayTransfer round 170. Without RomFS the
old OpenFileDirectly stop remains at round 79; fresh without empty PTM still stops
at step count round 71. Preserve these distinct paths. Existing-file startup is
not gamecoin readback or gameplay save/load.

The inherited --gpu-vram-mode reference-zero is an EXPLICIT reference-HLE cold-bank
policy: 6 MiB device-owned storage, not a CPU mapping, recovered VRAM or artwork.
The initial 32768-byte RGBA4 transfer's source and destination are both zero in this
boot; earlier nonzero synthetic tests establish conversion behavior, not game images.
PICA setup, its genuine P3D waiter notification and supported transfers are inherited.
GPU timing remains synchronous host policy; no vblank or accurate duration is modeled.
No rendered frame, main menu, shader execution, audio, controls or gameplay exists yet.

## Validation and limits

Full GCC and Clang builds link all 599 unchanged private page units. Final reruns:
31/31 CTest with GCC, 31/31 with Clang, and 31/31 ROM-free Clang ASan/UBSan suites,
with leak checking and halt-on-error enabled. Fourteen production startup scenarios
have byte-identical compiler logs/exits. Original code, all 603 AOT archive members,
and raw RomFS hashes match their verified inputs. Full IVFC block verification was
not repeated; whole-file SHA matches the previously verified image. Registry counts
111043 blocks / 545111 words are STATIC inventory, not executed CPU totals.

The new suite covers taken/not-taken indirect callbacks without prefix replay, every
retained word entry, PC-relative access, packed condition/link metadata, exact faults,
unsupported callbacks, budgets, native-candidate isolation, gaps and malformed extents.
The helper rejects unaligned, Thumb, empty, missing-ops, cross-shard/overflow and
following-record-overlap cases. Global immutable/sorted/non-overlap validation and
raw code identity remain host-loader obligations. This is not arbitrary host-pointer
validation or full Thumb/interworking parity. No opcode or scheduler fix is hidden here.

A diagnostic partitions all 545111 original PackedOps into single-word exact-entry
blocks. Compared with recorded-block dispatch, it reaches identical captured CPU/
thread/GPU state, GSP page, next packet, and all 16388 readable CPU pages (67125248
bytes), with matching permissions. Page aggregate SHA:
4cb50710efe41be8a45ae239d9311015862c08a7398c2cbd2a781e31539eb605.
Both variants share instruction backends; this is block-partition equivalence,
NOT independent ARM emulation or validation of every private kernel internal field.
The separate logging-only trace reaches those same final snapshots.

The one-word diagnostic initially exhausted its smaller block budget before the
target and its target-specific dumper rejected that early stop. Logs were retained.
Both final diagnostic variants use 100000000 blocks; production defaults were not
changed. Unavailable streaming-exec attempts ran no build. Actual finite drivers
built and tested the source. No native test failure was suppressed.

Only four implementation paths changed: CMakeLists.txt, tests/ctr_block_entry_test.cpp,
vendor/triaevum-a9b4477/recomp/a32_runtime.cpp and vendor LOCAL-PATCHES.md.
Current runtime SHA: 952fac140b5d860cd472c56c313231c4dcc8eb7b1719caeaa341503015030773.
Original code/AOT, opcode backends, production IPC, GPU services, scheduler and file
backends are unchanged. No Windows/macOS build or renderer work was performed.

## Evidence and next exact work

Current private evidence: block-checkpoint/. Final tests: ctest-final-gcc/clang/asan.txt;
final-checks.json records original identities and final test results. Production
matrix: validate.py, validation-summary.json, per-case logs. Exact suffix entries:
suffix-trace.txt. Queue trace: queue-trace.txt. Partition comparison: run_oracle.py,
oracle-proof.json, oracle-recorded/, oracle-one-word/. trace-final/ matches both.
make_trace.py builds logging-only alternate IPC/runtime objects. make_diagnostic.py
builds the partition oracle. Their binaries/objects are excluded from the backup.
Public report: reports/recovery-host/A32-BLOCK-SUFFIX.md and BLOCK-PROOF.json,
BLOCK-VALIDATION.json, BLOCK-FRESH/EXISTING/STRICT-GCC.txt.

Next implement the actual queued MemoryFill, not an unconditional dequeue or signal.
Reinspect pinned GPU::Execute MemoryFill, GPU::MemoryFill, software blitter and PSC
interrupt selection/order. Validate the first 819200-byte range against real device
VRAM and interpret the disabled second start and both controls from source. Fill
actual bytes before reporting completion; preserve transactional failure behavior,
PICA state, callback suffixes and real relay/event ownership. Test nonzero patterns
because this boot's zero VRAM cannot itself demonstrate changed-byte correctness.
Then rerun unchanged game code on a NEW empty root and observe its next request.

Primary reference remains azahar-emu/azahar at
86a9f9236ae42bb5a2b995dbc933d599d8ea07ac. Paths: src/video_core/gpu.cpp,
src/video_core/renderer_software/sw_blitter.cpp, src/core/hle/service/gsp/gsp_command.h
and gsp_interrupt.h. These fill semantics are next work, not newly implemented.
This patch was derived from original code/AOT, local dispatch/metadata ABI and Arm's
Branch and Call Sequences Explained. Do not silently replace pinned HLE policy.

## Working paths and reproduction

Root: /mnt/data/lego_recovery/. Current source: repo/. AOT: generated2/.
Code: restored/code.bin. Raw RomFS: game/prepared-romfs/romfs.bin.
Prepared raw parts: romfs-library-roundtrip/. Builds: build-gcc/, build-clang/,
build-asan/. Baseline source archive: snapshot-2aa22c0/. Current logs: block-checkpoint/.
Historical transfer/submit/queue evidence remains in the preserved pending archive.
New owned roots: private-state/block-*. Final matrix: block-validation.hlg0n17d/.
All are new TEST state, not recovered NAND saves. Never reset unknown existing saves.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
mkdir -p private-state
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/private-state/memory-fill-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty --romfs game/prepared-romfs/romfs.bin --gpu-vram-mode reference-zero
```

Expected exit 3: MemoryFill remains pending at round 193. Use clang++ for Clang;
omit LEGO_AOT_DIR for ROM-free tests. Sanitizer flags: block-checkpoint/build_remaining.py.
final_checks.py records the pre-report implementation tree, not the later delivery tree.

## Durable recovery and every-turn mandate

GitHub: GTTeancum/Lego-Undercover-3DS-Recomp, main. Library: /LEGO-Chase-Recovery/.
Restore the latest source/evidence checkpoint and handoff. Verify CHECKPOINT-MANIFEST.json
with verify_checkpoint.py before extraction. Unpack current repo/logs beneath the root
above. SOURCE-INDEX.json records exact Git paths, modes and blobs, including ignored
tracked reports. Reconstruct the local index faithfully; it is not remote history.
Historical nested archives must be restored elsewhere, never over current repo/.

Private inputs have separate Library backups: code.bin, unchanged
LEGO-Chase-current-AOT-599pages-2026-10-03.tgz, Prepared-RomFS/ two raw parts and
romfs-parts.json. Put code in restored/ and unpack AOT at the root (generated2/).
Prepared parts are 402653184 and 366526464 bytes. Do not repeat CCI extraction.

```sh
mkdir -p game/prepared-romfs
python repo/tools/restore_romfs_parts.py game/romfs-persistence/romfs-parts.json game/prepared-romfs/romfs.bin
python repo/tools/verify_romfs.py game/prepared-romfs/romfs.bin
```

Restore refuses an existing output; verify rather than overwrite. Raw RomFS is
769179648 bytes, native offset 4096, view size 769175552. Do not expose the IVFC
prefix as the filesystem header or discard trailing tables. Raw RomFS SHA:
6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf.
Code SHA: 5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
AOT archive SHA: 2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9.
Historical CCI SHA: 3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525.
Six original archive parts remain in Game-archive/ as an alternate route.

Scratch may reset. Library inputs/checkpoints and GitHub are recovery paths, not a
promise of permanent scratch. Actions source snapshots expire after 30 days. Direct
container GitHub DNS failed; connector reads/writes and artifact download worked.
MANDATE: tested checkpoints, publish source/reports and persist backups when available;
never invent push/CI/persistence success. Keep game bytes, private AOT/native binaries
and state captures OUT of public Git. Update this canonical handoff and POST A
DOWNLOADABLE COPY EVERY WORK TURN plus a durable source/evidence checkpoint. A new
chat must be able to restore, build, reproduce the actual boundary and continue.

## Hosted CI confirmation

GitHub Actions run `37379880223` on implementation `f8f7ad5` completed successfully
for both GCC and Clang jobs. These hosted suites are ROM-free, not original-game runs.
