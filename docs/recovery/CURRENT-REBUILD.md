# LEGO Chase Begins — canonical recovery handoff

Updated October 5, 2026. Read this first. Continue in scratch, NOT the user's PC
or Work. Project: LEGO City Undercover: The Chase Begins (Nintendo 3DS USA), not
LEGO Batman. Historical Recovery F/J gameplay is NOT current proof. Current native
build: headless startup reconstruction, not playable. Post this handoff every turn.

## Published source and verification

Implementation: `92a403adcb3ed726a74c4d852431bcdb0cb20138`.
Complete tested/uploaded implementation tree: `af36cadf084642f67a0715d9120202deb2e48308`.
Base main: `d8d30bed6f8e9dc84edaa87736d419de8a763c18`.
Restored baseline exactly matched `17f00ecaf461c6aa3ad74577545372903177be5f`.
All 721 previous manifest files and 257 indexed source files verified before edits.
The former first WriteHWRegs stop was reproduced, differing only in owned root name.

GitHub Actions 37334783231 passed both GCC and Clang jobs on implementation 92a403a.
Hosted tests are ROM-free, not game-data runs. The delivery commit adds reports and
this handoff only; the attached final receipt records its SHA and backup filename.
Local Git is a verified snapshot/index, not remote history. Use the current remote
parent and connector writes with force=false; never force-push snapshot history.

## Actual current boundary: command queue, not drawing

The original code now executes 64 direct WriteHWRegs and four WriteHWRegsWithMask
requests. The first four-byte zero store at relative 0x00401000 is retained as GPU
register state at 0x1EF01000. All 68 requests' before/input/mask/result values match
an independent replay against the pinned reset image. No GPU action is synthesized.

A NEW empty shared archive, explicit `--ptm-step-mode empty` and verified `--romfs`
now reach the following UNTOUCHED request:

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=166
r0=0x0006801b r1=0x000c0000 r3=0x10000800 r12=0x10000820 r14=0x00131898
last_ipc_session=gsp::Gpu requested_service= request_header=0x000c0000
```

This is TriggerCmdReqQueue. The actual guest-populated shared queue at 0x10000800
has header 0x00000100: index 0, pending count 1, status 0, should-stop 0. Its first
32-byte packet at 0x10000820 is:

```text
00000105 14003790 00007480 00000000 00000000 00000000 00000000 00000000
```

ID 5 is CacheFlush, NOT SubmitCmdList or a draw. First region: 0x14003790, length
0x7480 (29824 bytes); remaining two regions zero. The packet's unknown byte 1 is 1;
its stop byte is 0. This packet is NOT processed and its count is NOT decremented.
The GSP event remains UNSIGNALED, registered slot 0 is retained, and kernel time is
zero. No completion, vblank, frame, renderer, main menu or gameplay is established.
Final shared-page SHA: d2531c5e39bb7588224934f764b6b1c148b005e6c2d905a58631e0ac49f9699c.

Fresh and existing-file branches remain distinct. Existing gamecoin reaches the
same queue at round 158, skipping initialization. No RomFS option still stops at
SelfNCCH OpenFileDirectly round 79; fresh without empty PTM still stops at step count
round 71. Existing-file startup is not proof of save readback or gameplay save/load.

The game still creates/writes/closes its own 20-byte gamecoin.dat; default RTC SHA:
970a8b30f57b772c2c1c5686e634b9e4ab7055b43caec90e64076f81ed08b4b6.
Seven original RomFS metadata reads still total 4892 bytes and match exact input:
0/40 three times; then 40/12, 52/68, 120/212 and 332/4480. These are filesystem tables,
not rendered asset payloads. Audio, controls and initializer completion remain open.

## Register implementation policy and bounds

Exact direct header 0x00010082 accepts static ID 0; masked 0x00020084 accepts data
ID 0 and mask ID 1. Full readable input/mask spans, overflow and response permissions
are preflighted. New GuestMemory::SpansAlias catches overlapping response storage
including distinct VAs of the same shared page. Zero bytes dereference no pointer.
This helper is used on the register path; older services have not all been migrated.

Helper validation order matches pinned GSP: base alignment/range, maximum size 0x80,
then size alignment. Result codes: 0xE0E02A01, 0xE0E02BEC, 0xE0E02BF2 respectively.
The 0x732-word GPU bank spans relative 0x00400000 through 0x00401CC4. Complete batches
are staged/preflighted before mutation; unknown bank tails or active triggers cannot
partially store a prefix. LCD/other banks remain explicit host stops.

Five GPU::WriteReg actions are guarded. Fill controls 0x0040001C/0x0040002C and
transfer trigger 0x00400C18 stop when bit 0 would be set. Command-list triggers
0x004018F0/0x004018F4 stop on any nonzero value. Disabled triggers store values,
without finish bits or interrupts. Masks use (old & ~mask) | (data & mask).
Direct MMIO stores are NOT PICA command-list special-register execution.

The bank starts from the EXPLICIT PINNED-HLE constructor image: zero storage plus
all PicaCore::InitializeRegs assignments, including compatibility IRQ compare,
framebuffer and shader defaults. This is not a measured hardware reset dump or
working renderer/shader. The first masked request initially stopped until these
required defaults were inspected. State is module-shared across connections;
independent modules start independently. No extra GPU-rights condition is invented.

## Narrow archived DSB routing correction

After register setup the real code reached Fallback at PC 0x00248404, raw 0xEE071F9A.
The unchanged private page calls it CoreAlu. Existing a32_core.cpp already implements
this legacy DSB as a conservative native sequentially consistent fence. The local
ExecuteBlock metadata route previously admitted only DMB; it now also admits DSB
under mask 0x0FFF0FFF, normal condition and Rt != PC. It executes the existing fence
and continues inside the SAME block. No instruction, original code or AOT page is
skipped/rewritten. Unknown forms keep strict stops; no catch-all success is installed.

Only vendor/recomp/a32_runtime.cpp routing changed; the fence implementation did not.
See vendor/triaevum-a9b4477/LOCAL-PATCHES.md for exact baseline/patched hashes. DSB
regeneration is NOT changed; the prior generator correction still covers DMB only.
The old DSB-negative clock test now uses an actually unknown form, while the new
legacy-barrier suite covers valid forms, conditions, CPU/exclusive preservation,
in-block continuation and shared writes without fake event/time advancement.

No command execution, cache model, new scheduler timing, interrupts, rendering,
ReadHWRegs or active GPU trigger semantics are implemented. Prior GSP slots/event
ownership/shared mappings, FS/RomFS/PTM/APT/NDM/CFG behavior are retained. Production
IPC and kernel scheduler are unchanged. This turn changes GSP service, the memory
alias predicate, local vendor routing, CMake and tests. No Windows/macOS build claimed.

## Validation and current evidence

Full GCC baseline and Clang native builds link all 599 unchanged private AOT pages.
Final GCC 25/25 CTest, Clang 25/25 and Clang ASan/UBSan 25/25 ROM-free tests pass;
leak checking and halt-on-error enabled. Eleven scenarios match byte-for-byte:
fresh, existing, PTM-off, alternate RTC, no RomFS, no root, missing archive, invalid
root, invalid PTM mode, missing RomFS and wrong RomFS size. No test was suppressed.

All 603 private AOT archive members remain unchanged (599 C++ pages). Code and raw
RomFS hashes match; all three IVFC levels reverified, 187787 blocks. Registry
111043 blocks / 545111 raw words are STATIC validation counts, not frames or executed
instruction totals. A test compile initially used the wrong enum name; corrected
before all final suites. Optional capstone import failed; LLVM assembler/objdump
confirmed the barrier instead. Initial streaming exec was unsupported; normal finite
drivers did the actual builds. Intermediate logs remain in the private archive.

Current evidence: /mnt/data/lego_recovery/register-checkpoint/.
Final tests: ctest-gcc.txt, ctest-clang.txt, ctest-asan.txt; build-finished.json;
validation-summary.json. Full trace: trace-game.txt, trace_registers.py. Independent
replay/proof: make_proof.py, register-proof.json and read-proof.json. Trace IPC is a
logging-only alternate object; ordinary production GCC/Clang runs independently
reach the same boundary. Trace/native binaries and objects are excluded from backup.

Public report: reports/recovery-host/GSP-REGISTERS-DSB.md; REGISTER-FRESH-GCC.txt,
REGISTER-EXISTING-GCC.txt, REGISTER-PROOF.json and REGISTER-VALIDATION.json. Public
proof is a summary; the complete 68-write replay stays in the private checkpoint.

## Next exact work

Implement the observed TriggerCmdReqQueue and its actual CacheFlush packet using
the existing shared backing and owning GSP session. Reinspect pinned gsp_gpu.cpp
queue iteration, stop/status handling, index/count ordering and GPU::Execute's
CacheFlush case. Validate the actual regions and document what cache synchronization
means for this synchronous host. Do not invent an interrupt, completion flag, time
advance or renderer activity. Unsupported packets must remain explicit stops rather
than being silently consumed. Inspect subsequent real packets before extending GPU
execution. Use a NEW empty shared root, not pre-seeded state or an older save.

Primary pin: azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
Inspected this turn: src/core/hle/service/gsp/gsp_gpu.cpp parsers/helpers;
src/video_core/gpu.cpp MMIO/actions; src/video_core/pica/pica_core.h/.cpp complete
register/reset definitions; regs_external.h, regs_internal.h, regs_pipeline.h,
regs_shader.h; src/core/hle/service/gsp/gsp_command.h queue/packet layout.
The existing local native barrier handler is in vendor/triaevum-a9b4477/recomp/a32_core.cpp.

## Scratch and reproduction

Root /mnt/data/lego_recovery/. Source repo/; private pages generated2/;
code restored/code.bin; raw RomFS game/prepared-romfs/romfs.bin; prepared raw parts
romfs-library-roundtrip/. Builds build-gcc/, build-clang/, build-asan/.
Current logs register-checkpoint/; prior relay/priority/romfs/writefile/openfile/ptm/
createfile-checkpoint/ evidence is retained. Original CCI need not be re-extracted.
NEW matrix state: private-state/register-validation.3c_f4xqd/; other register-* roots
belong to this turn's diagnostics. Old state is preserved. These are NEW guest-created
test files, not recovered NAND. The paired validator resets only exact files it
just created and verified inside its own unique root. Never delete unknown saves.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
mkdir -p private-state
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/private-state/gsp-command-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty --romfs game/prepared-romfs/romfs.bin
```

Expected stop: TriggerCmdReqQueue 0x000C0000, round 166, diagnostic exit 3. Use
clang++ for Clang; omit LEGO_AOT_DIR for ROM-free builds. Sanitizer commands/flags
are in register-checkpoint/build_remaining.py. Full matrix script is validate.py.

## Reset recovery and every-turn mandate

GitHub GTTeancum/Lego-Undercover-3DS-Recomp main; private Library /LEGO-Chase-Recovery/.
Restore latest source/log checkpoint + code.bin + unchanged
LEGO-Chase-current-AOT-599pages-2026-10-03.tgz. Verify CHECKPOINT-MANIFEST.json before
editing. Unpack repo/logs beneath /mnt/data/lego_recovery, code into restored/, AOT
archive there (creates generated2/). SOURCE-INDEX.json preserves exact Git paths,
blob hashes and modes including ignored-but-tracked evidence. Do not lose those logs
when reconstructing the index. Never force-push snapshot history.

RomFS is already EXTRACTED and durable in Prepared-RomFS/. Materialize its two raw
parts (402653184 and 366526464 bytes) and romfs-parts.json into game/romfs-persistence/.
The helper refuses existing outputs; verify rather than overwriting an existing file:

```sh
mkdir -p game/prepared-romfs
python repo/tools/restore_romfs_parts.py game/romfs-persistence/romfs-parts.json game/prepared-romfs/romfs.bin
python repo/tools/verify_romfs.py game/prepared-romfs/romfs.bin
```

Raw RomFS 769179648 bytes, SHA 6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf.
Native view offset 4096, size 769175552; do not include IVFC prefix as filesystem
header or remove trailing integrity tables.
Code SHA 5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
AOT archive SHA 2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9.
Historical CCI SHA 3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525.
Six original parts remain under Game-archive/ as an independent recovery route.

Scratch may reset. Library prepared inputs/source checkpoints and GitHub are the
recovery paths. Source-snapshot Actions expire after 30 days. Connector access works;
direct container GitHub DNS failed. No Work/user-PC development was used.
MANDATE: tested checkpoints, push source/reports before ending, keep game bytes,
private AOT/binaries/test state OUT of public Git. Update this canonical handoff and
POST A DOWNLOADABLE COPY EVERY WORK TURN, plus a durable source/log checkpoint.
This file must let a new chat restore, build, reproduce the real boundary and continue
without guesses or unnecessary uploads. The attached receipt records final delivery.
