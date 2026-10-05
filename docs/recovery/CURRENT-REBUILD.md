# LEGO Chase Begins — canonical recovery handoff

Updated October 5, 2026. Read this first. Continue in scratch, NOT the user's PC or
Work. Project: LEGO City Undercover: The Chase Begins (Nintendo 3DS USA), not LEGO
Batman. Historical Recovery F/J gameplay is not current proof. The native build
is headless startup reconstruction, not a playable port. Post this file every turn.

## Published implementation and verified baseline

Implementation: `a136cdeb05c500f9426007a05f3acbf9563b2a9c`.
Exact tested/uploaded implementation tree: `e40bd3048979a6f78c5f9e11fb5562da5b2054ec`.
Baseline main: `19139e9fcbdc53c1eda6076f47f34af615183ca2`.
Baseline tree: `e00c04ac723be2a662a56f8bb3a15781f3e9f4a1`.
All 370 prior manifest entries and 272 indexed source files verified. The full GCC
baseline reproduced the SubmitCmdList stop at round 167 before edits. Private code,
AOT and prepared RomFS were restored and hash-verified without user reuploads or
original CCI extraction. Local Git is a snapshot/index, NOT remote commit history.
The attached handoff's final receipt records the delivery commit and backup name.

GitHub Actions run `37355280419` passed both GCC and Clang jobs on implementation
a136cde. Hosted tests are ROM-free, not game-data execution. Publish through the
connector with the current remote parent and force=false; never force-push snapshots.

## Actual progress: original non-drawing PICA list executes

The original queued SubmitCmdList at guest 0x14003790 is 29824 bytes, SHA-256
824caecf736fbd9febe688ee15b2f1da3913f9c19cf9f5a89e4539e540877c73.
It now executes 559 command packets / 6590 register writes, including 4096 shader
program-word uploads, 96 uniform vectors and 1792 lighting-table writes. Every
implemented register and upload buffer matches an independent Python replay.

The 4096 program uploads comprise 512 VS words and 3584 GS words; VS mirroring
fills the remaining GS prefix. Final known program coverage is GS 4096, VS 512.
The original initialization values happen to be zero; these are guest writes, NOT
pre-seeded code or executed shader instructions. GS has 96 known float vectors;
VS has none. Unwritten uniforms retain explicit unknown flags. Lighting tables
0 through 6 receive 256 words each. This list has no draw-register writes.

The list's final irq_request value 0x12345678 matches the inherited reference-HLE
comparator and requests autostop. After supported effects succeed, the queue
advances index 1/count 1 to index 2/count 0, and P3D interrupt ID 5 is placed in
the owner's real relay ring. The retained one-shot EventObject is signaled.

The original waiting thread 3 is genuinely woken: WaitSynchAny -> Ready,
pending_wake=true, Result=0. It consumes the one-shot signal immediately, so the
event is already UNSIGNALED in the post-IPC snapshot. Do not misdiagnose that as
missing notification. The original thread subsequently executes and waits in
address arbitration. No fabricated wake, handle or unconditional completion signal.

IRQ delivery is explicitly synchronous host policy, NOT accurate GPU duration or
hardware scheduling. Kernel time remains zero. No vblank or frame completion exists.

## Next untouched packet: DisplayTransfer

Fresh startup now reaches:

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=170
last_ipc_session=gsp::Gpu requested_service= request_header=0x000c0000
host_ipc_error=GSP queue packet requires unimplemented GPU execution
queue_header=00000102
packet=01000103 1f5f8000 14013950 00800080 00800080 00004400 00000000 00000000
```

The packet is ID 3, DisplayTransfer, at index 2 / guest 0x10000860. Input address
0x1F5F8000, output 0x14013950, size words 0x00800080 each, flags 0x00004400.
It remains pending and UNEXECUTED. Full shared-page and GPU-state snapshots are
unchanged across the rejected call. Shared-page SHA-256:
83cf491b296dc411027337df9876776c907b67eabf222853b423dbeedbb3ee03.
Do not infer valid source pixels, VRAM initialization or conversion success yet.
No rendered frame, main menu, shader execution, audio, controls or gameplay proof.
Packet/write/dispatch counts are not frames or a completion percentage.

Fresh uses a NEW empty archive, explicit --ptm-step-mode empty and verified --romfs.
Existing gamecoin reaches DisplayTransfer at round 162, skipping initialization.
No RomFS retains OpenFileDirectly round 79; no empty PTM retains step count round 71.
Existing-file startup is not gamecoin readback or gameplay save/load. The game still
writes/closes its own 20-byte gamecoin.dat, default RTC SHA:
970a8b30f57b772c2c1c5686e634b9e4ab7055b43caec90e64076f81ed08b4b6.
Seven original RomFS metadata reads match all 4892 bytes: 0/40 three times, then
40/12, 52/68, 120/212 and 332/4480. These are filesystem tables, not rendered assets.

## Implemented scope and explicit limits

New StagePicaStartupList interprets PICA command headers separately from direct
MMIO. It handles LE value/header pairs, byte-mask expansion, repeated/sequential
parameters, 8-byte padding, passive registers and observed upload side effects.
Raw upload-port values are retained where the reference uses the raw argument,
even with mask zero. Program capacities, VS/GS mirroring, integer/bool uniforms,
packed float24/float32 transfers and lighting type/index wrap follow inspected code.
Float values are IEEE32 bit patterns matching the reference f24 container, not a
claim of implemented shader arithmetic or hardware precision. Partial upload
vectors persist between successful lists; unwritten uniforms must remain unknown.

Topology/restart reconfigure/reset only an EMPTY assembler: all draw and immediate
paths stop. Draw 0x22E/0x22F and chaining 0x23C/0x23D are rejected even for zero data
or zero mask. Immediate/default attributes, swizzle, fog/procedural uploads,
out-of-range registers and ambiguous repeated non-upload special batches also stop.
The reference sometimes ignores invalid input; these stricter stops are host policy.

Submit accepts only the observed readable, 8-byte-aligned linear-heap slice, flags=0,
do_flush=0, length <=1 MiB. The cap is host work policy, not console capacity.
Channel-0 size/address/trigger setup uses the inspected VA-to-physical conversion;
trigger clears only after success. General GPU physical addressing is not modeled.
Snapshots reject aliases with the GSP page or IPC response, including shared backing
aliases. Response capacity and all eligible packets are checked before mutation.

A disposable plan stages all register/upload changes. Invalid/unsupported later
packets or IRQ-ring state leave even a valid prefix pending, with no partial GPU
state, queue advance or event. This whole-batch atomicity is diagnostic host policy,
not established console error precedence. Unknown/full/failed interrupt rings stop;
overflow recovery is not implemented. A list without a matching IRQ request never
creates P3D merely because it reaches the end.

The real retained-object event helper mirrors existing one-shot/sticky/pulse wake
semantics, including handles closed after registration. It allocates no handle and
advances no time. Scheduler algorithms and old handle-based SignalEvent source are
unchanged. CacheFlush behavior, real shared mappings, ownership/slots and earlier
FS/RomFS/PTM/APT/NDM/CFG behavior remain inherited. No new draw/transfer, cache model,
async GPU timing, Windows or macOS build is claimed.

Ten implementation files changed: CMakeLists.txt, cmake/LEGOHostRuntime.cmake,
ctr_kernel.h (declaration only), new ctr_service_event.cpp, gsp_gpu_service.h,
gsp_command_queue.cpp, new pica_startup.h/.cpp and two new tests. Original game bytes,
private AOT, vendor, production IPC, ctr_kernel.cpp and filesystem backends unchanged.

## Validation and reproducible evidence

Full GCC and Clang native builds link all 599 unchanged private page units.
Final GCC 28/28 CTest, Clang 28/28 and Clang ASan/UBSan 28/28 ROM-free suites pass,
with leak checking/halt-on-error enabled. Eleven production scenarios match byte-for-
byte between compilers: fresh, existing, PTM-off, alternate RTC, no RomFS, no root,
missing archive, invalid root, invalid mode, missing RomFS and wrong RomFS size.
All 603 AOT members, original code and raw RomFS match their verified backups. Full
IVFC-block verification was not repeated; raw SHA matches the prior verified image.
Registry 111043 blocks / 545111 words remains STATIC inventory, not executed CPU count.

New tests cover all byte masks, grouping/padding, raw upload ports, upload capacities,
mirroring, packed float golden values, partial vectors, LUT wrap/type guards, IRQ
match/autostop, unsupported effects, atomic batches, source/reply aliases, real
waiters, relay wrap/fullness, nonowner calls and retained-event/full-handle behavior.
One test compile needed an explicit method return type; failure log retained and
fixed before final passing builds. An unsupported streaming-container invocation
ran no build; normal finite drivers performed the successful builds. No suppressed
failure. Final trace was rebuilt from the final source after the event-helper split.

Current private evidence: submit-checkpoint/. Baseline: baseline-game.txt and
baseline-run.json; final tests: ctest-gcc/clang/asan.txt and final-finished.json.
Production matrix: validate.py, validation-summary.json and scenario logs.
trace_submit.py builds a logging-only alternate IPC object; production IPC is unchanged.
verify_submit.py independently replays original-command-list.bin against BEFORE/AFTER
snapshots and checks every implemented state array/known bitmap, the full relay page,
real thread-3 wake and next packet remaining untouched. It also verifies RomFS reads.
Public summaries: reports/recovery-host/PICA-STARTUP-SUBMIT.md, SUBMIT-PROOF.json,
SUBMIT-VALIDATION.json, SUBMIT-FRESH-GCC.txt and SUBMIT-EXISTING-GCC.txt.
Captured list/state bytes remain PRIVATE, excluded from GitHub. Trace binaries and
objects are excluded from source/evidence backups; their build scripts are retained.

## Next exact work

Inspect the pending DisplayTransfer packet and its actual source/destination backing
before implementing it. Read pinned GPU::Execute DisplayTransfer, MemoryTransfer,
software blitter addressing/format/tiling/conversion, and current guest memory/VRAM
mapping. Do not create empty VRAM just to make a transfer succeed or assume the
0x1F5F8000 source has valid pixels. Derive flags/size behavior from inspected code and
original memory. A transfer must move/convert actual bytes before any PPF notification.
Preserve PICA upload state and the demonstrated real P3D wake. Run a NEW empty
shared archive with explicit empty PTM and original RomFS; observe the next request.

Primary reference: azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
This turn read pica_core.cpp parser/batch/special handlers, shader_setup.cpp/.h,
packed_attribute.h, pica_types.h, regs_shader.h, regs_lighting.h, pica_core.h and
gsp_interrupt.h; earlier GPU SubmitCmdList/physical routing and GSP relay code are
also available. Prefix src/video_core/ for PICA files, src/core/hle/service/gsp/ for
interrupts. Exact blob identities: submit-checkpoint/references.json. Full transfer
semantics are NEXT work, not already implemented.

## Scratch paths and build commands

Root /mnt/data/lego_recovery/. Source repo/, private pages generated2/,
code restored/code.bin, raw RomFS game/prepared-romfs/romfs.bin. Prepared raw parts
romfs-library-roundtrip/. Builds build-gcc/, build-clang/, build-asan/.
Current evidence submit-checkpoint/; previous queue-checkpoint/ preserved.
NEW final matrix: private-state/submit-validation.58ohmzz7/. Other submit-* roots
are this turn's diagnostics; their exact trace path is in trace-console.txt.
These files are guest-created TEST state, not recovered NAND. Older roots remain
untouched. The paired validator resets only its own byte-verified fresh test file.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
mkdir -p private-state
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/private-state/gsp-transfer-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty --romfs game/prepared-romfs/romfs.bin
```

Expected diagnostic exit 3: DisplayTransfer pending, round 170. Use clang++ for
Clang; omit LEGO_AOT_DIR for ROM-free tests. Sanitizer flags and commands are in
submit-checkpoint/build_remaining.py and final_validate.py. No user-PC development.

## Reset recovery and every-turn mandate

GitHub GTTeancum/Lego-Undercover-3DS-Recomp, main. Library /LEGO-Chase-Recovery/.
Restore latest source/evidence archive and handoff, NOT the historical pending
register checkpoint. Verify CHECKPOINT-MANIFEST.json with verify_checkpoint.py;
unpack repo/logs beneath the root above. SOURCE-INDEX.json preserves exact Git
paths/blob hashes/modes, including ignored-but-tracked logs. Reconstruct the index
faithfully but never treat its local history as the remote history.

Private inputs have separate backups: code.bin, unchanged
LEGO-Chase-current-AOT-599pages-2026-10-03.tgz, and Prepared-RomFS/ two raw parts plus
romfs-parts.json. Put code in restored/; unpack AOT at the root (creates generated2/).
Raw parts are 402653184 and 366526464 bytes. Restore without repeating CCI extraction:

```sh
mkdir -p game/prepared-romfs
python repo/tools/restore_romfs_parts.py game/romfs-persistence/romfs-parts.json game/prepared-romfs/romfs.bin
python repo/tools/verify_romfs.py game/prepared-romfs/romfs.bin
```

Restore refuses existing output; verify rather than overwrite it.
Raw RomFS 769179648 bytes; view offset 4096, size 769175552. Do not expose IVFC prefix
as filesystem header or discard trailing tables. Raw SHA:
6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf.
Code SHA: 5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
AOT SHA: 2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9.
Historical CCI SHA: 3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525.
Original six archive parts under Game-archive/ remain an alternate recovery route.
Older GSP-REGISTERS-PENDING is preserved separately/nested under historical/; never
overlay it on newer repo/. Scratch may reset. Library/GitHub are recovery routes;
Actions source snapshots expire after 30 days. Connector writes work in this turn.

MANDATE: tested checkpoints; publish source/reports and durable backups when available;
never invent push/CI/persistence success. Keep original game data, private AOT/native
binaries/test state OUT of public Git. Update this canonical handoff and POST A
DOWNLOADABLE COPY EVERY WORK TURN plus the source/evidence checkpoint. A new chat
must be able to restore, rebuild, reproduce the actual stop and continue from it.
