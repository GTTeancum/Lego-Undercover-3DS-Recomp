# LEGO Chase Begins — canonical recovery handoff

Updated October 5, 2026. Read this first. Continue in scratch, NOT the user's PC or
Work. Project: LEGO City Undercover: The Chase Begins (Nintendo 3DS USA), not LEGO
Batman. Historical Recovery F/J gameplay is not current proof. The native build
remains headless startup reconstruction, not playable. Post this handoff every turn.

## Published implementation and reconciled source

Implementation: `811083b6d68d6cd10eaf7f22a07574973d0442e0`.
Exact tested/uploaded implementation tree: `ef981f540f716197161e2c1f46d0d41446c195ba`.
Remote base: `05b01f75d05b1ef6762ca44f1ff183860326762e`.
Base tree: `1f8a0e16351c40ffdc3b7cc5212832374df51aea`.
The final downloadable receipt records the subsequent report/delivery commit.

The previous chat attachment was GSP-REGISTERS-PENDING (older, 60 direct writes,
masked-write stop). It was NOT discarded: all 782 manifest entries were verified,
source compared with the newer remote, and the exact archive and handoff saved in
/LEGO-Chase-Recovery/Preserved-Pending-Registers/. Historical contents are preserved
under pending-preserved/ in scratch and as a nested archive in the new backup.
Never overlay that historical pending source over this newer working repository.

A fresh main read revealed 05b01f7, already containing 64 direct plus four masked
writes, a pinned-HLE reset register image and the archived legacy DSB routing fix.
These are INHERITED, not this turn's new work. Source was recovered from Actions
run 37335606464, artifact 11355887677, into snapshot-05b01f7/ and repo/. Its full
local tree matched the recorded base exactly. The baseline queue stop at round 166
was reproduced before edits. No game re-extraction or user upload was needed.
Local Git is a snapshot/index, NOT remote history. Publish with the current remote
parent and force=false. GitHub and Library writes work again in this checkpoint.

## Actual progress and next untouched packet

Original game code processes one CacheFlush in its GPU-rights owner's slot 0:

```text
TriggerCmdReqQueue request: 000c0000
packet: 00000105 14003790 00007480 00000000 00000000 00000000 00000000 00000000
queue header before: 00000100
queue header after:  00000001
reply: 000c0040 00000000
```

This advances index 0 to 1 and pending count 1 to 0. The 29824 bytes at 0x14003790
compare byte-for-byte equal before/after the call. SHA-256:
824caecf736fbd9febe688ee15b2f1da3913f9c19cf9f5a89e4539e540877c73.
The event remains UNSIGNALED, shared mapping remains real, and kernel time stays 0.
Only the queue header changes; no GPU completion, interrupt or data copy is invented.

The original game next enqueues SubmitCmdList (ID 1) at index 1, VA 0x10000840:

```text
01000101 14003790 00007480 00000000 00000000 00000000 00000000 00000000
```

The list is the same 29824-byte readable region. Flags and do_flush are zero;
stop byte is zero; unknown header bytes 1 and 3 are both 1. Current queue header
00000101 means index 1, count 1, status 0, should_stop 0. This packet is NOT consumed
or executed. Its shared page before/after SHA-256 is unchanged:
a8df46c356cf1acbaa3545d5ab705664ba4155dc6b3b5892da97bdae96d513b9.

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=167
r0=0006801b r1=000c0000 r3=10000800 r12=10000840 r14=00131898
last_ipc_session=gsp::Gpu requested_service= request_header=0x000c0000
host_ipc_error=GSP queue packet requires unimplemented GPU execution
```

No PICA command-list execution, drawing, rendering, main menu, audio, controls,
completed initializer count or gameplay is established. Packets/rounds are not frames.
Fresh startup requires a NEW empty archive, explicit --ptm-step-mode empty, and
verified --romfs. Existing gamecoin reaches the same packet at round 159, skipping
initialization; that is not gamecoin readback or gameplay save/load. No RomFS retains
OpenFileDirectly at round 79; no empty PTM retains step count at round 71.

The game still creates/writes/closes its own 20-byte gamecoin.dat. Default-clock SHA:
970a8b30f57b772c2c1c5686e634b9e4ab7055b43caec90e64076f81ed08b4b6.
Seven RomFS metadata reads match the original input: 0/40 three times, then 40/12,
52/68, 120/212 and 332/4480, total 4892 bytes. These are filesystem tables, not assets
being rendered. No console history, fake coin balance or game data was pre-seeded.

## New implementation scope and explicit policies

TriggerCmdReqQueue selects the GPU-rights OWNER's slot, not the requesting session's
slot. A connected nonowner can trigger the owner's queue, following the reference.
The real 4096-byte GSP backing contains four 0x200 queues starting at offset 0x800;
each has 15 packets of 0x20 bytes at queue offset 0x20. No new thread is a queue slot.

Only CacheFlush executes. Its three nonzero regions require readable single-region
spans and wide address bounds; zero sizes dereference no pointer. The synchronous
host has no split CPU/GPU cache or asynchronous GPU. Existing writes already target
authoritative bytes. A conservative native seq_cst fence provides ordering, not a
device-cache simulation, transfer, interrupt or clock advance. Pinned CacheFlush's
GPU::Execute case performs no extra action.

Index/count advance before the packet operation. should_stop sets STOPPED without
dequeue; an already STOPPED queue is untouched; packet stop sets STOPPED after that
packet. Empty queues do not rewrite the header. Ring fields must be valid; unknown
or failed status is unsupported. The exact reference only tests status==STOPPED;
more general comments are not interpreted as discovered failure-recovery semantics.

Diagnostic safety policy preflights the WHOLE eligible batch before any progress.
An unsupported later packet or invalid region leaves a valid prefix pending too.
A packet stop ends eligibility, so packets behind it are not examined. This batch
atomicity and error precedence are NOT proven console behavior. Non-CacheFlush,
no-owner and unsupported mapping/physical-address cases remain explicit host stops.

Existing IPC response preflight runs first. New UsesSharedBacking detects a response
alias anywhere in the GSP page, including an alternate VA; such requests stop before
shared mutation. Header writes invalidate their actual backing reservation granule.
Independent source/packet reservations are retained. No handle allocation, event
signal/clear, wakeup, thread creation or time advancement occurs. Full handle tables
work. General mapped-buffer translation, async timing, GPU addressing/cache models,
active triggers and rendering remain incomplete.

Exactly six implementation files changed from 05b01f7: CMakeLists.txt,
cmake/LEGOHostRuntime.cmake, src/runtime/ctr_memory.h, src/services/gsp_gpu_service.h,
new src/services/gsp_command_queue.cpp and tests/ctr_gsp_queue_test.cpp.
Vendor, original code/private AOT, production IPC router, scheduler, extdata/RomFS,
PTM, APT, NDM and CFG behavior were not changed in this turn. No Windows/macOS build.

## Validation and evidence

Full GCC and Clang native builds include all 599 unchanged AOT page units. Final
GCC 26/26 CTest, Clang 26/26 and Clang ASan/UBSan 26/26 ROM-free suites passed with
leak checking and halt-on-error. Eleven production startup scenarios have byte-
identical compiler logs: fresh, existing, PTM-off, alternate RTC, no RomFS, no root,
missing archive, invalid root, invalid PTM mode, missing RomFS, wrong RomFS size.
All 603 regular AOT members, original code and raw RomFS hashes remain unchanged.
Full IVFC-block verification was NOT repeated here; the whole raw SHA matches the
previously verified image. Registry 111043 blocks / 545111 words is STATIC inventory.

New tests cover original CacheFlush, three ranges, readonly/zero-size regions, full
15-packet wrap, stop flags, malformed ring/status/headers, invalid spans, atomic
unsupported tails, nonzero owner slots, owner-vs-caller selection, duplicate lifetime,
full handles, response permissions/aliases and exclusive-reservation coherence.
No native test failure was suppressed. A one-space upload transcription discrepancy
was corrected to the exact tested tree before publishing. An optional expected_sha
connector argument was rejected at binding; the normal force=false update succeeded.

Current private evidence/scripts: queue-checkpoint/. Final files:
ctest-gcc.txt, ctest-clang.txt, ctest-asan.txt, final-finished.json,
final-evidence-finished.json, validation-summary.json, trace-game.txt, queue-proof.json.
trace_queue.py builds a LOGGING-ONLY alternate IPC object; ordinary production IPC
is unchanged and independently reaches the same stop. make_proof.py independently
replays all shared-page bytes around both requests and verifies source/read hashes.
Trace binaries/objects are excluded from the source/log archive.
Public evidence: reports/recovery-host/GSP-CACHEFLUSH-QUEUE.md, QUEUE-FRESH-GCC.txt,
QUEUE-EXISTING-GCC.txt, QUEUE-PROOF.json and QUEUE-VALIDATION.json.

## Next exact work

Implement the actual SubmitCmdList packet, not a dummy dequeue. First inspect the
pinned GPU::Execute SubmitCmdList, VirtualToPhysicalAddress, PicaCore::ProcessCmdList
and internal register decoder/mask/special-register effects. The actual list lives
at guest 0x14003790, size 0x7480. It is not a sequence of passive WriteHWRegs calls.
Capture/inspect original list bytes privately as necessary; preserve their hash and
queue state. Do not claim command completion, draw execution, render output or
signal an interrupt until its real supported effects justify it. Keep unknown
commands/active triggers explicit stops and retain fresh/existing negative tests.

Primary source pin: azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
This turn inspected src/core/hle/service/gsp/gsp_command.h, gsp_gpu.cpp queue loop,
and src/video_core/gpu.cpp CacheFlush. The complete PICA SubmitCmdList path is next
work, not already implemented. Earlier register/reset/barrier references are in
reports/recovery-host/GSP-REGISTERS-DSB.md and vendor LOCAL-PATCHES.md.

## Scratch and reproduction

Root /mnt/data/lego_recovery/. Source repo/; private AOT generated2/;
code restored/code.bin; raw RomFS game/prepared-romfs/romfs.bin. Prepared parts:
romfs-library-roundtrip/. Builds: build-gcc/, build-clang/, build-asan/.
Current logs: queue-checkpoint/. Older pending source/evidence: pending-preserved/,
which must NEVER replace repo/. New final matrix: private-state/queue-validation.s3v8pcme/;
final trace: private-state/queue-trace.cj6he6v0/. Other queue-* roots are also NEW test
state. Old historical roots remain in the preserved archive. These are not console
NAND saves. The validator only resets its own just-created, byte-verified files.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
mkdir -p private-state
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/private-state/gsp-submit-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty --romfs game/prepared-romfs/romfs.bin
```

Expected diagnostic exit 3: TriggerCmdReqQueue/SubmitCmdList pending, round 167.
Use clang++ for Clang; omit LEGO_AOT_DIR for ROM-free tests. Exact sanitizer flags
and drivers: queue-checkpoint/build_remaining.py and final_validate.py.

## Durable recovery and every-turn mandate

GitHub: GTTeancum/Lego-Undercover-3DS-Recomp main. Library: /LEGO-Chase-Recovery/.
Use the latest source/log checkpoint, NOT historical GSP-REGISTERS-PENDING. Verify
CHECKPOINT-MANIFEST.json before editing; unpack current repo/logs beneath the scratch
root. SOURCE-INDEX.json records all exact Git paths/blob hashes/modes, including
ignored-but-tracked logs. Restore the local index faithfully; it is not remote history.

Private inputs have separate backups: code.bin; LEGO-Chase-current-AOT-599pages-2026-10-03.tgz;
and Prepared-RomFS/ two raw parts plus romfs-parts.json. Put code.bin in restored/;
unpack AOT in the root (creates generated2/). Restore raw RomFS without repeating CCI
extraction. Part sizes 402653184 and 366526464; restore refuses existing output.

```sh
mkdir -p game/prepared-romfs
python repo/tools/restore_romfs_parts.py game/romfs-persistence/romfs-parts.json game/prepared-romfs/romfs.bin
python repo/tools/verify_romfs.py game/prepared-romfs/romfs.bin
```

Raw RomFS: 769179648 bytes, SHA 6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf.
View offset 4096, bytes 769175552; do not expose IVFC prefix or remove trailing tables.
Code SHA 5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
AOT SHA 2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9.
Historical CCI SHA 3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525.
Original six parts in Game-archive/ remain an alternate route; no CCI was needed here.

Scratch may reset. Library prepared inputs/source checkpoints and GitHub are the
recovery paths; Actions source snapshots expire after 30 days. No Work or user-PC
access was used. MANDATE: tested checkpoints, push source/reports and persist backups
when available; never fabricate a push, CI or persistence result. Keep game bytes,
private AOT/binaries/test state OUT of public Git. Update this canonical handoff and
POST A DOWNLOADABLE COPY EVERY WORK TURN plus the source/log checkpoint. It must let
a new chat restore, build, reproduce the real stop and continue without reuploads.

## Hosted CI confirmation

GitHub Actions run `37343431474` on implementation `811083b` completed successfully
for both GCC and Clang jobs. Hosted tests are ROM-free, not original-game execution.
