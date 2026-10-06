# LEGO Chase Begins — canonical continuation handoff

Continue in assistant scratch, NOT the user's PC or Work. Nintendo 3DS USA:
LEGO City Undercover: The Chase Begins. This is headless native reconstruction,
not playable. POST AN UPDATED DOWNLOADABLE HANDOFF AND SOURCE/EVIDENCE EVERY TURN.
Only post genuine meaningful game visuals, never generated art, test patterns or blank buffers.

## Current checkpoint and restored baseline

Implementation: `1cf1c1d1ec6af90854c4dac44d997a511d9ed70d`.
Exact tested/uploaded implementation tree: `871749f15b6f11e42326da896feb31703405ed1b`.
Base main: `dfa588edbf998155981aa838533b73f12cd82f15`, tree
`64d781d22bb05d7e584f7cfd8e73445a30e4d3c0`. The final downloadable receipt records
this checkpoint's subsequent report commit and backup name. Local Git is a snapshot,
NOT remote history. Re-read remote main and publish with its parent and force=false.

The previous LCD change had only hosted ROM-free verification. Local execution now
works. Its ZIP and inner archive checksum, all 355 manifest entries and all 333
source blobs were verified; the reconstructed index matched the base tree exactly.
Private code/AOT and the two prepared RomFS parts were present and reverified.
No user upload, original CCI extraction, Work or user-PC access was needed.

The FULL baseline now confirms LCD succeeds. It then took the original Break at
0x0011FB80, round 234, because srv: returned ServiceNotRegistered for y2r:u. A
separate discovery-only run exposed the original DriverInitialize request 0x002B0000.
No Break or failed lookup was bypassed. Those baseline/intermediate logs are retained.

## Actual progress and next untouched request

The original LCD zero request gets `000B0040 00000000`; both LCD words remain zero.
Y2R DriverInitialize now gets `002B0040 00000000`, with genuine configuration reset
and the same retained one-shot completion event cleared, not signaled. Initial
input-line width becomes 1024; input-lines remains zero according to the pin.
No YUV conversion, DMA, converted pixels, completion signal or extra time occurs.

Fresh original startup now stops at:

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=235
last_ipc_session=cfg:u requested_service= request_header=0x00010082
ipc_words= 00010082 00000020 00050005 0000020c 00594218 00000000 00000000 00000000
```

Pinned CFG names command 1 GetConfig and block 0x00050005 StereoCameraSettings.
This requests 32 bytes at guest 0x00594218. The complete IPC and destination remain
untouched. No camera settings or calibration were guessed or supplied. This is
NOT a successfully read configuration block. Existing-gamecoin reaches the same
request at round 227; it skips file initialization and is not a save-readback test.

Logging-only GCC and Clang captures match byte-for-byte for LCD, Y2R and CFG calls.
At each call, all 1842 GPU words, serialized upload state, 4096-byte shared page,
all shared epochs and full 6 MiB VRAM are unchanged. Both LCD words remain zero;
GSP event stays unsignaled. Time stays 16713681 ns and there are three threads.
Handle count is 17 at LCD, 18 at Y2R/CFG after the actual service connection.
The two display callbacks and all older GPU paths remain inherited, not new work.

VRAM is still entirely zero under the explicit reference-HLE cold-bank policy.
No useful screenshot, rendered frame, main menu, shader execution or gameplay.

## Implemented scope and safeguards

Y2rUserService permits one connected session, following the pinned service limit.
A duplicate handle retains the same session; a failed handle allocation releases
the temporary lease. State and the one-shot EventObject survive disconnect/reconnect.
An independent endpoint has independent state. The registration endpoint itself
cannot execute IPC. Only the exact zero-parameter 0x002B0000 request is supported.

DriverInitialize resets input/output formats, rotation, alignment, coefficients,
width, alpha, planar Y/U/V and destination buffers. Pinned SetInputLines(1024)
does not assign the lines field; input_lines, src_yuyv and padding are deliberately
retained. Nondefault pure-state tests verify that distinction. Initial values follow
the reference's value initialization, not recovered hardware registers.

Initialization clears the existing event without allocating a guest handle, waking
threads, signaling completion or changing guest memory outside the IPC response.
The inherited full response readability/writability preflight runs before mutation.
Malformed requests and every other Y2R operation remain explicit stops, including
StartConversion, IsBusyConversion, GetTransferEndEvent and DriverFinalize. No
conversion scheduler, dithering behavior or buffer I/O is implemented.

Six runtime/test/build paths changed: CMakeLists.txt, cmake/LEGOHostRuntime.cmake,
src/runtime/ctr_runner.cpp, new services/y2r_user_service.h/.cpp and the new
ctr_y2r_initialize_test.cpp. The runner change only registers this service.
Production IPC, GPU/LCD code, scheduler, memory, vendor, AOT and file backends are unchanged.

The seventh implementation path, tools/package_romfree_checkpoint.py, now checks
exact configured CTest names instead of a stale hardcoded 37-suite count. It fails
closed if this repository's literal foreach list is malformed or duplicated.
Hosted package receipts refer to separate private evidence rather than an obsolete
fixed predecessor. This is packaging support, not runtime progress.

## Validation and evidence

Full GCC and Clang native executables link all 599 unchanged private AOT pages.
All 38 CTest suites pass with GCC and Clang; all 38 ROM-free Clang ASan/UBSan suites
pass with leak checks and halt-on-error. Seventeen production scenarios have
byte-identical compiler logs, exit codes and resulting test-file bytes. Strict modes
remain distinct: no display clock waits at round 228; no explicit VRAM stops at the
earlier transfer round 170; absent RomFS and unconfigured PTM preserve earlier stops.

New tests cover exact reset fields, preserved fields, one-session limits, duplicate
lifetime, reconnect, independent endpoints, real-event clearing, full handle tables,
failed-connection rollback, protected/partial/write-only replies, malformed IPC and
untouched unsupported conversions. No previous test was removed or changed.

Original code, whole RomFS and all 603 regular AOT backup members match their inputs.
Full IVFC checking was not repeated; the raw SHA matches the previously verified image.
111043 registry blocks / 545111 words are STATIC inventory, not execution or frames.
No Windows/macOS build. Clang build and initial monolithic matrix each hit a container
call timeout; bounded continuation/new-root matrix completed. Streaming sessions are
unavailable. Setup notes retain these limitations; no native test failure was hidden.

Current private evidence: lcd-native-checkpoint/. Important files: baseline-game.txt,
discovery-game.txt, initialize-game.txt, ctest-gcc/clang/asan.txt, trace-proof.json,
identity-proof.json, validation-summary.json, references.json and setup-notes.txt.
make_trace.py builds alternate logging-only IPC objects; production IPC is unchanged.
trace_support.inc and verify_trace.py serialize and check all compared state; the
52 files per compiler under captures-gcc/clang are PRIVATE, not public GitHub.
validate_matrix.py runs bounded ranges 0 6, 6 11, 11 17 on one owned new test root;
start 0 always creates a new root. Earlier partial roots are not reused or deleted.

## Next exact work

Inspect pinned CFG GetConfig, block permissions/size, StereoCameraSettings structure
and the original caller. Establish a justified source or explicit reference policy
for block 0x00050005 before returning data. Do not fill the 32-byte destination with
guessed zeros or silently impersonate recovered console calibration. Then rerun
unchanged code from NEW test state with the explicit modes below. The next real
request, not assumptions about cameras or video playback, determines further work.

Reference: azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
This turn inspected service/cam/y2r_u.cpp/.h (DriverInitialize, SetInputLines,
construction/state), service/cfg/cfg.h (block names), cfg_u.cpp (GetConfig command).
These paths have prefix src/core/hle/. Exact blob hashes: references.json.
Y2R files are under cam/, not service/y2r/. Configuration data semantics are next work.

## Scratch and reproduction

Root /mnt/data/lego_recovery/. Current source repo/; private AOT generated2/;
code restored/code.bin; raw RomFS game/prepared-romfs/romfs.bin; raw parts and manifest
romfs-library-roundtrip/. Builds build-gcc/, build-clang/, build-asan/.
New private-state/y2r-* and lcd-baseline.* directories contain guest-created TEST
state, not recovered NAND. Do not overwrite unknown saves. Trace drivers refuse
existing capture directories. Older source/evidence remains separate in historical/.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
mkdir -p private-state
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/private-state/cfg-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty --romfs game/prepared-romfs/romfs.bin --gpu-vram-mode reference-zero --display-clock-mode reference-idle
```

Expected exit 3, CFG GetConfig pending, round 235. Use clang++ for Clang. Omit
LEGO_AOT_DIR for ROM-free sanitizer builds. Never silently enable or drop the modes.

## Durable recovery and mandate

GitHub GTTeancum/Lego-Undercover-3DS-Recomp main; Library /LEGO-Chase-Recovery/.
Restore the latest private source/evidence checkpoint, not the older hosted-only ZIP.
Verify CHECKPOINT-MANIFEST.json before extraction. SOURCE-INDEX.json records exact
Git paths, modes and blob identities including ignored tracked reports. Reconstruct
the index faithfully, but never replace remote history with a local snapshot.

Private inputs remain separately backed up: code.bin; unchanged
LEGO-Chase-current-AOT-599pages-2026-10-03.tgz; Prepared-RomFS/ two uncompressed parts
and romfs-parts.json. AOT unpacks as generated2/. Part sizes: 402653184 and 366526464.
Restore with repo/tools/restore_romfs_parts.py using the manifest beside the parts;
restore refuses existing output. No CCI re-extraction or user upload is necessary.
Raw RomFS size 769179648, native view offset 4096, view bytes 769175552. Retain tables.
RomFS SHA 6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf.
Code SHA 5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
AOT SHA 2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9.
Historical CCI SHA 3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525.

Preserve both previous archives: LEGO-Chase-LCD-checkpoint-dfa588e.zip for hosted
source/evidence and LEGO-Chase-source-checkpoint-da5c9a9.tgz for private display
captures and nested prior history. The new checkpoint retains both unchanged.
Scratch may reset; Library and GitHub are recovery paths, not permanent scratch.
Actions artifacts expire after 30 days. Keep private code/AOT/RomFS/captures/binaries
OUT of public GitHub. Publish tested source/reports and save private checkpoints
when tools permit. Never invent push, CI, persistence, screenshots or gameplay.
POST THE UPDATED HANDOFF AND RECOVERABLE SOURCE/EVIDENCE FILE EVERY WORK TURN.

## Hosted confirmation

Actions run 37407926702 on implementation 1cf1c1d passed both GCC and Clang jobs.
These are ROM-free hosted suites, separate from the full local original-game runs.
