# LEGO Chase Begins — canonical continuation handoff

Updated October 5, 2026. Continue in scratch, NOT the user's PC or Work.
Project: LEGO City Undercover: The Chase Begins (Nintendo 3DS USA), not LEGO Batman.
The executable is a headless startup reconstruction, NOT a playable port.
Historical Recovery F/J claims are not current gameplay proof. POST THIS FILE EVERY TURN.

## Published implementation

Implementation: `8a2cf0153f39ffcbf75e98bcd77b6378b07749b6`.
Exact tested/uploaded implementation tree: `6b079aa558863eb0ece5bcddad4339e2207abe11`.
Baseline main: `8bad0fc0e253489ccf24c31dac7d92b00adb1c81`.
Baseline tree: `431bd3c3dcb6e6faf4b0b17bb1a6aea532921b2d`.
The attached final receipt records delivery SHA, hosted CI and backup filename.
Local Git is a reconstructed snapshot/index, NOT remote history. Publish with the
verified current remote parent and force=false; never force-push snapshot history.

The attached 8bad0fc archive was restored. All 429 manifest members and 300 source
blobs verified; the local Git tree exactly matched the remote before edits. Private
code/AOT and the two prepared RomFS parts were already mounted and were restored
without user upload or CCI extraction. Baseline round-193 MemoryFill was reproduced.

## New progress: real device VRAM MemoryFill

The original packet now executes:

```text
01000102 1f070800 00000000 1f138800 00000000 00000000 000c8000 02010201
queue header: 00000109 -> 0000000a
reply:        000c0040 00000000
```

Channel 0 writes 819200 bytes of 32-bit zero patterns at 0x1F070800..0x1F138800.
The second start is zero, so its residual end/control fields are ignored and its
registers unchanged. After the bytes are committed, PSC0 (ID 0) is queued; trigger
bit 0 clears and finish bit 1 sets, final control 0x0202. The complete register
image, full VRAM bank and shared page match independent Python replay.

Thread 3 genuinely changes WaitSynchAny -> Ready, pending_wake=true, Result=0.
Its one-shot event is consumed immediately and appears unsignaled after the call.
Handle count remains 17 and guest time remains zero. Synchronous notification is
HOST POLICY, not accurate GPU duration, new timing, or an invented vblank.

IMPORTANT: original VRAM was already zero from the explicit reference-HLE cold-bank
policy, so this run changes zero VRAM bytes. Nonzero ROM-free tests/captures, never
inserted into original-game runs, establish changed-byte correctness: 819200 bytes
for a 32-bit fill, 2304 bytes for overlapping dual channels, and 384 for a 24-bit
fill. GCC/Clang captures match; independent replay checks all bank/register/relay
bytes. PICA upload state is preserved. This is not drawing or recovered artwork.

## Next actual untouched request

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=198
last_ipc_session=gsp::Gpu requested_service= request_header=0x000c0000
host_ipc_error=DisplayTransfer format/layout/scaling flags are unsupported
queue_header=0000010a
packet=01000103 1f070800 1f300000 01900200 019001e0 01001004 00000000 00000000
```

This is ID 3 DisplayTransfer, not another fill. Both addresses select device VRAM.
Pinned field decoding gives input 512x400 RGBA8, programmed output 480x400, RGB8
output, horizontal half-scale, and crop flag. The reference formula yields a
240x400 result (288000 RGB8 bytes); that conversion and VRAM output are NOT supported
yet. Current StageDisplayTransfer supports only the older equal-size RGBA4 0x4400
VRAM-to-private-linear-heap case. Do not treat this as a generic memcpy.

The next packet is not consumed. Full captured VRAM, GPU registers, upload state,
GSP page and kernel snapshots are unchanged across rejection. Page SHA:
`b633bcdb420102646e9ba4288ca37c649ae50f12dde7d1c5e258806802d405fc`.
No rendered frame, main menu, shader execution, audio, controls or gameplay proof.
Rounds/packets/test counts are NOT frames or completion percentages.

Fresh startup requires a NEW empty shared root, --ptm-step-mode empty, verified
RomFS and --gpu-vram-mode reference-zero. Existing-gamecoin reaches round 190,
skipping initialization; that is not save readback. Strict default (no VRAM option)
still stops at the earlier DisplayTransfer at round 170. No RomFS retains the
round-79 OpenFileDirectly stop; fresh without empty PTM retains step count round 71.
The guest still creates/writes/closes gamecoin.dat; default RTC SHA:
`970a8b30f57b772c2c1c5686e634b9e4ab7055b43caec90e64076f81ed08b4b6`.

## Implementation policies and limits

StageMemoryFill stages both channels and registers with no visible mutation. Zero
start disables the complete channel. Nonzero start with trigger clear stores setup
only: no bytes, finish modification or IRQ. Triggered channels support 16/24/32-bit
little-endian patterns; simultaneous width flags prioritize 24-bit. Both nonzero
starts suppress channel-0 IRQ and make triggered channel 1 request PSC0; otherwise
a triggered channel requests its corresponding PSC. The selection uses starts,
not both trigger bits. Channel 1 overwrites overlaps after channel 0.

Supported ranges are nonempty 8-byte-aligned half-open VRAM spans, with one-past-bank
end accepted. Control bits outside 0x0303 stop. Triggered channels are capped at
1 MiB each; partial 24-bit patterns stop instead of overrunning. These restrictions,
end policy and malformed-input precedence are HOST SAFETY POLICY, not full firmware
parity or console capacity. No FCRAM fills, CPU VRAM maps, active MMIO triggers,
new caches, rendering, or asynchronous GPU execution were added.

The whole eligible queue and IRQ capacity are preflighted before any visible effect.
Unsupported tails preserve valid prefixes. A transfer source depending on an earlier
staged fill stops; transaction forwarding is not implemented. Separate calls read
committed fills, and independent mixed PICA/fill batches preserve order. VRAM Write
allocates nothing after staging. Failed staging allocation changes no bytes. A
hypothetical commit invariant failure reports partial state after dequeue and emits
no PSC for its failed channel; none occurred in validation.

Existing response permissions, GSP-page aliases, real event ownership, shared
reservation epochs, ring wrap and stop rules remain. Device VRAM has no CPU mapping
or CPU exclusive epochs. The explicit reference-zero bank is inherited, not a new
blank-memory shortcut. It follows pinned HLE initialization, not measured hardware
or recovered pixels. Existing A32 recorded-suffix callback support is unchanged.

Eight implementation paths changed: two CMake files, gsp_gpu_service.h,
gsp_command_queue.cpp, new gsp_memory_fill.h/.cpp, and tests/ctr_memory_fill_test.cpp
and ctr_gsp_fill_test.cpp. Production IPC, vendor/opcode backends, scheduler, PICA
interpreter, prior transfer backend and file backends are unchanged.

## Tests and evidence

Full GCC and Clang native builds link all 599 unchanged private AOT pages.
All 33 CTest suites pass with GCC and Clang, and all 33 ROM-free Clang ASan/UBSan
suites pass with leak checking/halt-on-error. Fourteen production startup cases
have byte-identical compiler logs/exits. New tests cover patterns, capacity,
disabled/dual channels, overlap, IRQ selection, mixed PICA/fill order, 15-packet
wrap, real waiters, closed event handles, full handles, response protections,
shared aliases, exclusive reservations, transaction dependencies and allocation failure.

All 603 AOT backup members, original code and raw RomFS hashes match their verified
inputs. Full IVFC-block checking was not repeated; the raw SHA matches the earlier
verified image. Registry 111043 blocks / 545111 words remains STATIC inventory.
No Windows/macOS build was performed. No previous test was removed or altered.
The first compile had aggregate braces wrong; fixed before passing tests, log kept.
An initial foreground matrix hit the container timeout; the full unique-root rerun
passed all fourteen cases. The setup failures are documented, not suppressed tests.

Current evidence: fill-checkpoint/. Core files: fill-proof.json, identity-proof.json,
validation-summary.json, ctest-fill-first.txt, ctest-clang.txt, ctest-asan.txt,
remaining-finished.json. make_trace.py uses a logging-only alternative IPC object;
production IPC is unchanged. original-captures/ holds private real-run snapshots;
single32-*, dual-overlap-* and fill24-* are explicitly SYNTHETIC test captures.
verify_fill.py independently replays all fill bytes/registers/relay effects.
run_synthetic.py and validate.py reproduce the compiler comparisons. Native/trace
binaries and objects are excluded from backups; build sources/scripts remain.

Public report: reports/recovery-host/GSP-MEMORY-FILL.md. Public proof/logs:
FILL-PROOF.json, FILL-VALIDATION.json, FILL-FRESH-GCC.txt, FILL-EXISTING-GCC.txt,
FILL-STRICT-GCC.txt. Full captures remain private, never public GitHub.

## Next exact work

Implement the precise queued 0x01001004 DisplayTransfer above. Reinspect pinned
GPU::Execute/MemoryTransfer, sw_blitter.cpp, regs_external.h, color.h and Morton
addressing, especially scaling/averaging/cropping and VRAM destination handling.
Source is the just-filled bank range; destination must be the existing device bank,
not a fabricated CPU mapping. Preserve whole-batch safety and source dependencies.
Only actual completed byte effects may justify PPF. Do not inject a frame or vblank.
Then rerun unchanged game code on a NEW empty test root and inspect the next request.

Reference pin: azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
This turn inspected GPU::Execute MemoryFill and GPU::MemoryFill in
src/video_core/gpu.cpp, blob 40f29fea0867b0e8cf4d89a8753ddabf02c3b54b, and software
MemoryFill in src/video_core/renderer_software/sw_blitter.cpp, blob
0a68afd07c3e22fdfaaf441d4744ef79abb2a86f. Exact pinned sources were read through
GitHub after web-mirror cache misses. Preserve pinned semantics, not mutable upstream.

## Scratch and reproduction

Root /mnt/data/lego_recovery/; source repo/; private AOT generated2/;
code restored/code.bin; raw RomFS game/prepared-romfs/romfs.bin; prepared parts
romfs-library-roundtrip/. Builds build-gcc/, build-clang/, build-asan/.
Current logs/captures fill-checkpoint/. Prior evidence block-checkpoint/ remains.
Older transfer/submit/queue evidence is preserved in historical nested archives;
never overlay old source over repo/. New private-state/fill-* roots are TEST state,
not recovered console NAND. The validator resets only its own verified new files.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
mkdir -p private-state
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/private-state/scaled-transfer-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty --romfs game/prepared-romfs/romfs.bin --gpu-vram-mode reference-zero
```

Expected diagnostic exit 3: scaled DisplayTransfer pending at round 198.
Use clang++ for Clang; omit LEGO_AOT_DIR for ROM-free tests. Sanitizer commands are
in fill-checkpoint/build_remaining.py. Trace/synthetic drivers refuse existing
capture directories; use new paths or verify their ownership before rerunning.

## Durable recovery and continuing mandate

GitHub: GTTeancum/Lego-Undercover-3DS-Recomp main. Library: /LEGO-Chase-Recovery/.
Restore the latest source/evidence checkpoint and handoff. Validate its manifest
with verify_checkpoint.py before extraction. SOURCE-INDEX.json preserves all tracked
Git paths, blob identities and modes, including ignored-but-tracked reports. Restore
that index faithfully; never force-push reconstructed local snapshot history.

Private inputs are separately backed up: code.bin; unchanged
LEGO-Chase-current-AOT-599pages-2026-10-03.tgz; Prepared-RomFS/ two raw parts and
romfs-parts.json. Put code in restored/, unpack AOT under the root (generated2/).
Prepared parts are 402653184 and 366526464 bytes; no CCI re-extraction is necessary.

```sh
mkdir -p game/prepared-romfs
python repo/tools/restore_romfs_parts.py romfs-library-roundtrip/romfs-parts.json game/prepared-romfs/romfs.bin
python repo/tools/verify_romfs.py game/prepared-romfs/romfs.bin
```

Restore refuses an existing output; verify rather than overwrite. Raw RomFS is
769179648 bytes; native view offset 4096 and size 769175552. Preserve the complete
image, including its integrity tables. Raw SHA:
6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf.
Code SHA: 5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
AOT SHA: 2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9.
Historical CCI SHA: 3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525.
Six original parts in Game-archive/ remain an alternate recovery path.

Scratch may reset. Library inputs/checkpoints and GitHub are recovery paths, not a
promise of permanent scratch. Actions source snapshots expire after 30 days.
MANDATE: tested checkpoints; publish source/reports and durable backups when possible;
never invent push/CI/persistence or gameplay success. Keep game bytes/private AOT,
compiled binaries and private state captures OUT of public Git. Update this canonical
handoff and POST A DOWNLOADABLE COPY EVERY WORK TURN plus source/evidence backup.
A new chat must be able to restore, build, reproduce the real boundary and continue.

## Hosted CI confirmation

GitHub Actions run `37384971072` on implementation `8a2cf01` completed successfully
for both GCC and Clang jobs. Hosted suites are ROM-free, not original-game runs.
