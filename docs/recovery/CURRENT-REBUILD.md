# LEGO Chase Begins — canonical continuation handoff

Updated October 5, 2026. Continue in scratch, NOT the user's PC or Work.
Project: LEGO City Undercover: The Chase Begins (Nintendo 3DS USA), not LEGO Batman.
This is a headless native startup reconstruction, NOT a playable port. Historical
Recovery F/J claims are not current gameplay proof. POST THIS FILE EVERY WORK TURN.

## Published source and restored baseline

Implementation: `3a403ad3e8428a070b025ee31f8983a818cfc33b`.
Exact tested/uploaded implementation tree: `b31e9e47a7eed04f24c9fc8f19225693e5069f22`.
Base main: `0d953eccb16b094bb7a07bb9715a1bf61ceace78`.
Base tree: `02fe02f966fa987f03305fedb41a412046eef7ec`.
All 494 baseline manifest entries and 310 source blobs verified before edits.
The original scaled-transfer stop at round 198 was reproduced with the full GCC
baseline. Private code/AOT and prepared RomFS parts were already mounted; no user
upload, original CCI extraction, user-PC or Work access was needed.

Local Git is a reconstructed snapshot/index, NOT remote history. Publish with the
verified current remote parent and force=false; never force-push local snapshots.
The final attached receipt identifies the subsequent report/delivery commit.
GitHub Actions run 37388750201 passed both GCC and Clang jobs on implementation
3a403ad. Hosted tests are ROM-free, not original-game execution.

## Actual progress: six scaled VRAM transfers

The formerly pending flags=0x01001004 DisplayTransfer now executes against the
existing device bank: tiled RGBA8 input -> horizontally averaged RGB8 linear output.
All inputs declare 512x400. Four programmed 480x400 transfers produce 240x400,
288000-byte outputs; two programmed 480x320 transfers produce 240x320, 230400-byte
outputs. Total: SIX transfers and 1612800 output bytes, not frames.

Input -> output selectors in original order:
1F070800 -> 1F300000; 1F070800 -> 1F38CA00; 1F098800 -> 1F000000;
1F070800 -> 1F346500; 1F070800 -> 1F3D2F00; 1F098800 -> 1F038400.
Actual byte commits precede trigger-clear and PPF notifications. The worker consumes
those notifications. Every captured bank byte, all 1842 registers and complete GSP
page match independent inverse-Morton replay. Upload state is preserved.

IMPORTANT: original source and destination pixels are already zero under the explicit
inherited --gpu-vram-mode reference-zero policy. This is a reference-HLE cold bank,
not recovered console VRAM or artwork. The original run changes ZERO VRAM bytes.
Nonzero synthetic captures establish conversion: original-shape changes 286189 bytes,
all-pairs changes 195422, crop changes 295; independent replay matches everything.
Synthetic patterns never enter original-game runs. No CPU VRAM map or per-transfer
reset/substitution was added. No rendering, main menu, shader execution or gameplay.

## New exact boundary: display-interrupt wait, not a queued command

```text
stop=WaitingNoRunnableThread pc=0x0024b324 detail=0x00000000 thread=1 dispatch_rounds=228
last_ipc_session=gsp::Gpu requested_service= request_header=0x000c0000
ipc reply=000c0040 00000000
```

Final command header=0x00000001: index 1, pending count 0. Relay count=0, error=0,
real retained GSP event UNSIGNALED; kernel time=0. All three guest threads are waiting.
Thread 1: WaitArb, arbiter handle 1, address 0x00371CA4 containing -1, type 1,
comparison 0, untimed. Thread 3: WaitSynchAny, real GSP event handle 0x0007001C,
infinite timeout. Thread 2: existing APT three-object wait. Neither a new wake nor
vblank was injected. These are captured actual states, not an assumption of completion.

Original disassembly plus live stack resolves the main caller:
0x00106538 calls 0x00257A68 with mode 0x402. It snapshots counters at 0x00593C84
and 0x00593C88, waits until BOTH change, and yields through 0x0024E06C, GSP handshake
0x00130244, and address-wait helper 0x00251D88. The GSP object is at 0x00371C40;
its handshake word is +0x64 and state byte +0x76. The original relay worker sets the
state and signals the handshake after processing an interrupt.

The live callback table maps interrupt ID 2 to 0x0011F4C8 and ID 3 to 0x0011F50C.
Those original functions increment the two counters. Pinned gsp_interrupt.h names
IDs 2/3 PDC0/PDC1. The runtime has no periodic display-interrupt source. This is the
next missing platform behavior, not proof that all remaining initialization is done.
Do not simply poke counters, fake a vblank, return wait success, or schedule an
arbitrary completion. Rendering and display timing are separate unimplemented areas.

Existing-gamecoin startup reaches this wait at round 220. Strict no-VRAM still stops
at the earlier DisplayTransfer, round 170. No RomFS retains round 79; no explicit
empty PTM retains round 71. Keep all branches distinct. Existing-gamecoin is not a
save-readback proof. Fresh tests must use a NEW empty private shared archive.

## New implementation and retained limits

Only exact flags 0x01001004 and inherited 0x4400 are supported. New input dimensions
must be nonzero whole tiles, programmed output within input with even nonzero width.
Output height may crop vertically; width is halved. RGBA8 bytes A,B,G,R become RGB8
bytes B,G,R by wide-sum pair averaging with truncating division. No byte overflow.
No vertical flip, other scales/formats/layouts, texture copies or in-place overlap.
Source and output are separate aligned, bounded device spans, capped independently
at 1 MiB (host policy, NOT firmware capacity). Programmed dimensions remain in regs.

Plans now record input_bytes separately from output bytes and select device versus
CPU-backed output explicitly. Earlier staged fills/transfers that overlap the FULL
input span cause a host stop, including source tails beyond output size. Whole-batch
atomicity, owner selection, response/relay guards and stop markers are retained.
Transaction-memory forwarding remains unimplemented. Unrelated CPU mappings with the
same numeric VA are not device backing; the CPU bytes/reservations stay unchanged.
FCRAM commits preserve the inherited reservation-safe path. PPF follows real bytes.

Exactly FIVE implementation files changed: CMakeLists.txt, gsp_display_transfer.h/.cpp,
gsp_command_queue.cpp and new tests/ctr_scaled_transfer_test.cpp. No original code,
private AOT, vendor/opcode backend, kernel, scheduler, memory backend, production IPC,
PICA upload interpreter, FS/RomFS/PTM/APT/NDM/CFG or explicit reset policy changed.
Synchronous PPF/P3D/PSC timing remains host policy; no accurate GPU timing is claimed.

## Verification and evidence

Full GCC and Clang native builds link all 599 unchanged AOT pages. All 34 CTest
suites pass with each compiler; all 34 ROM-free Clang ASan/UBSan suites pass with
leak checks and halt-on-error. Fourteen startup scenarios have byte-identical logs.
New tests include all 65536 channel-value pairs, odd rounding, alpha/color order,
crop/tile edges, caps, spans, failed-plan preservation, device/CPU independence,
full handles, closed retained event, real waiter, owner/nonowner, ring wrap, stop
markers, protected replies, batch rollback, mixed PICA/transfer and dependencies.
No prior test was removed or altered. No native compilation or test failure occurred.
One streaming-container invocation was unavailable; finite drivers did the builds.

Original code, raw RomFS and all 603 AOT archive members match their backups. Full
IVFC-block checking was not repeated; raw SHA matches the previously verified image.
Registry 111043 blocks / 545111 words is STATIC inventory, not CPU execution or frames.

Current private evidence: scaled-checkpoint/. Tests: ctest-gcc/clang/asan.txt.
Build drivers: baseline_driver.py, build_remaining.py. Matrix: validate.py and
validation-summary.json (14 cases). Proof: verify_scaled.py and scaled-proof.json.
Captures: final-captures/ (original) and original-shape/all-pairs/crop-gcc/clang
(EXPLICITLY SYNTHETIC). Original-captures/ is the earlier identical-stop trace.
make_trace.py creates logging-only IPC/runner alternatives; ordinary builds independently
reach the same wait. final-captures/svc-trace.txt and final-waits.txt show actual
threads, stacks, GSP state. Original-code .s/disassembly files are PRIVATE, not GitHub.
The diagnostic final dump resolves observed handle 0x6801B; it does not create one.

Public report: reports/recovery-host/GSP-SCALED-TRANSFER.md, SCALED-PROOF.json,
SCALED-VALIDATION.json and SCALED-*-GCC.txt. Complete captures/scripts stay in backup.
No Windows/macOS build was performed. Historical evidence stays nested, not overwritten.

## Next exact work

Implement a justified display-event/timing model for the observed PDC0/PDC1 wait.
Read pinned GPU::VBlankCallback, its frequency source and timing scheduling, and GSP
PDC handling/relay/framebuffer-update policies. Inspect the original callback chain
and test ordering, deadlines, duplicate/overflow behavior, idle wake and strict stops.
Keep time advancement tied to a modeled event, not an unconditional unblock.
Do not equate periodic display notification with a drawn frame or implement it by
writing the two counters. Rerun unchanged code from NEW test state and observe the
next real operation. Preserve the supported PICA, fill and both transfer paths.

Primary reference pin: azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
This turn: sw_blitter.cpp DisplayTransfer (0a68afd07c3e22fdfaaf441d4744ef79abb2a86f),
common/vector_math.h Vec4 (8eaae0f7b5ff9cfd95956de78359a48fd2648dbb),
gsp_interrupt.h (8bd37d05144a9a636b0011770a3e8c1e0571ecc1). Prior color.h/utils.h
and GPU::MemoryTransfer were already inspected. Exact references: references.json.
Original code/captures and LLVM disassembly establish the actual wait caller.

## Scratch and reproducible commands

Root /mnt/data/lego_recovery/. Source repo/, private pages generated2/, code
restored/code.bin, raw RomFS game/prepared-romfs/romfs.bin. Prepared raw parts:
romfs-library-roundtrip/. Builds build-gcc/, build-clang/, build-asan/.
Current logs scaled-checkpoint/; fill-checkpoint/ retains prior evidence locally.
New private-state/scaled-* roots are guest-created TEST state, not recovered NAND.
The validator only removes its own byte-verified fresh file to pair compiler runs.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
mkdir -p private-state
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/private-state/display-clock-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty --romfs game/prepared-romfs/romfs.bin --gpu-vram-mode reference-zero
```

Expected diagnostic exit 3: WaitingNoRunnableThread, round 228. Use clang++ for
Clang; omit LEGO_AOT_DIR for ROM-free tests. Do not silently drop the explicit modes.

## Durable recovery and continuing mandate

GitHub GTTeancum/Lego-Undercover-3DS-Recomp main. Library /LEGO-Chase-Recovery/.
Restore the latest source/evidence archive and handoff. verify_checkpoint.py validates
CHECKPOINT-MANIFEST.json before extraction. SOURCE-INDEX.json records exact paths,
Git blobs and modes including ignored tracked evidence. Restore its index faithfully;
local history is not remote history. The complete prior 0d953ec archive stays nested
under historical/. Extract historical evidence separately, NEVER over current repo/.

Private inputs remain separately backed up: code.bin, unchanged
LEGO-Chase-current-AOT-599pages-2026-10-03.tgz, and Prepared-RomFS/ two raw parts plus
romfs-parts.json. Code goes in restored/; AOT unpacks under the root as generated2/.
Prepared part sizes 402653184 and 366526464; no original CCI re-extraction needed.

```sh
mkdir -p game/prepared-romfs
python repo/tools/restore_romfs_parts.py romfs-library-roundtrip/romfs-parts.json game/prepared-romfs/romfs.bin
python repo/tools/verify_romfs.py game/prepared-romfs/romfs.bin
```

Restore refuses existing output. Verify instead of overwriting. Raw RomFS size
769179648, native view offset 4096, view bytes 769175552; preserve integrity tables.
RomFS SHA: 6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf.
Code SHA: 5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
AOT SHA: 2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9.
Historical CCI SHA: 3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525.
Six original parts remain in Game-archive/ as an independent recovery route.

Scratch may reset. Library inputs/checkpoints and GitHub are recovery paths;
Actions source snapshots expire after 30 days. MANDATE: tested checkpoints; publish
source/reports and durable backups when available; never fabricate push/CI/persistence,
rendering or gameplay. Keep original game bytes/private AOT/binaries/captures OUT
of public Git. Update this canonical handoff and POST A DOWNLOADABLE COPY EVERY WORK
TURN plus source/evidence backup sufficient for a new chat to recover and continue.
