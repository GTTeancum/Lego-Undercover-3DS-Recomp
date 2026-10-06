# LEGO Chase Begins — canonical continuation handoff

Continue in assistant scratch, NOT the user's PC or Work. Project: LEGO City
Undercover: The Chase Begins (Nintendo 3DS USA). This is headless native startup
reconstruction, not a playable port. POST THIS UPDATED MARKDOWN AND A RECOVERABLE
SOURCE/EVIDENCE CHECKPOINT EVERY WORK TURN. Screenshots must be meaningful genuine
game output, not blank VRAM, test patterns, reference screenshots or generated art.

## Published implementation and restored baseline

Implementation: `1bc3c7d5ecc7508c902d3d035bf590ea2dd5532e`.
Exact tested/uploaded implementation tree: `cd08e620bac867620213031bd50fc50aff715e58`.
Baseline: `237def70338cb07641a9daec4898b320f78a724b`, tree
`9497b84eaff20374b3c507ec73fe873f97d6782e`.
The final downloadable receipt records the subsequent report/delivery commit.
Use current remote parents and force=false (expected_sha when available).
Local Git is a reconstructed snapshot/index, NOT full remote history.

This turn restored the attached 237def7 archive: all 534 manifest entries and 338
source blobs verified, with exact baseline tree match. Private code/AOT and prepared
RomFS parts were already available. No reupload, CCI extraction or user-PC access.
The full GCC baseline reproduced the untouched CFG call at round 235 before edits.

## Current actual progress and remaining boundary

New explicit option: `--cfg-profile reference-stereo`.
It supplies ONLY block 0x00050005 using eight exact binary32 compatibility defaults
from pinned Azahar cfg_defaults.cpp. This is NOT recovered console calibration,
measured hardware reset state or a complete CFG save. The source does not fully
identify the eight fields; do not invent their meanings. The default profile stays
unconfigured. Unknown blocks/sizes/descriptors still stop without output.

The original game now receives the exact 32-byte configuration at 0x00594218:
request `00010082 00000020 00050005 0000020c 00594218`,
reply `00010042 00000000 0000020c 00594218` followed by zeros.
Data SHA: eb666a350f0f714ab7ca73c9ee5cdace24f90cb79b7afa30fd03be193b52c5b6.
Independent Python binary32 serialization matches every byte. Twenty-five bytes
change from the original output, while 64 neighbouring captured bytes stay unchanged.

Fresh original startup then reaches:

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=238
last_ipc_session=APT:U requested_service= request_header=0x004f0080
ipc_words= 004f0080 00000001 0000001e 00090020 00000000 00000000 00000000 00000000
```

This is SetAppCpuTimeLimit with arguments 1 and 30. Word 3 is outside the declared
request, hence stale buffer contents, NOT a handle argument. The call is untouched:
no CPU resource limit is applied and no success response returned. Existing-gamecoin
reaches the same call at round 230; this skips file initialization, not save readback.

Time remains 16713681 ns after one display period. The full 1842-word GPU image,
uploads, GSP page/epochs, LCD words and 6 MiB VRAM remain unchanged across CFG and
the rejected APT call. The APT service connection brings handles from 18 to 19;
the call itself adds none. All 78 logging-only capture files match between GCC/Clang.
VRAM is still entirely zero. NO logo, main menu, rendered frame or gameplay exists.

## Implementation scope

Eight implementation paths changed: CMakeLists.txt, cmake/LEGOHostRuntime.cmake,
src/host/main.cpp, src/runtime/ctr_runner.h/.cpp, src/services/cfg_service.h/.cpp,
tests/ctr_cfg_stereo_test.cpp. The runner change passes an immutable profile into CFG;
its scheduling logic is unchanged. No original game bytes, AOT, vendor/opcode,
production IPC, memory backend, APT, GPU, LCD, Y2R or filesystem implementation changed.

The handler supports only exact header 0x00010082, block 0x00050005, size 32 and
write-only mapped descriptor 0x20C. Full output and response permissions are checked.
The pre-existing private PrepareDeviceWrite/CommitDeviceWrite path reserves metadata
before output and commits without allocation, invalidating touched exclusive epochs.
Shared output, cross-region spans and reply aliases stop; invalid pointers return
transport InvalidPointer. These strict bounds and direct-buffer restrictions are
HOST POLICY, not complete firmware parity or error precedence. No NAND is created.
CFG keeps its previous shared-service session model; full session-limit parity is
not claimed. Other configuration reads and writes are not silently enabled.

The selected reference words, serialized little-endian:
42780000 43908000 4299999A 423851EC 41200000 40A00000 425E51EC 41AC8F5C.
They represent 62, 289, 76.80000305175781, 46.08000183105469, 10, 5,
55.58000183105469, 21.56999969482422 in the pinned order.

## Tests and evidence

Full GCC and Clang native builds link all 599 unchanged private AOT page units.
All 39 CTest suites pass under each compiler, plus 39 ROM-free Clang ASan/UBSan
suites with leak checking/halt-on-error. No earlier suite was removed or changed.
Twenty original-startup scenarios match stdout, exit code and newly created file
bytes across compilers. Strict-no-CFG retains round 235; no display clock retains
round 228; no explicit VRAM retains round 170; absent RomFS/PTM preserve older stops.
The matrix also checks malformed and missing profile options.

All 603 AOT archive members, code.bin and raw RomFS match their private backups.
Full IVFC-block verification was not repeated; the raw SHA matches the prior verified
image. Registry counts 111043 blocks/545111 words are STATIC inventory, not execution
counts or frames. No Windows/macOS build. No native test failure occurred.
Setup-only issues and bounded build interruptions: cfg-checkpoint/setup-notes.txt.

Current evidence directory: cfg-checkpoint/.
build_and_test.py, configure-*/build-* logs, ctest-gcc/clang/asan.txt, tests.json;
validate_matrix.py and validation-summary.json (20 cases);
make_trace.py / trace_ipc.cpp / trace_support.inc (diagnostic-only alternative IPC);
captures-gcc/clang (78 files each), verify_trace.py / trace-proof.json;
identity-proof.json, restore.json and references.json.
Production IPC is untouched. Raw captures/disassembly are PRIVATE.
LLVM wrapper disassembly uses section-relative offsets: add 0x100000 for guest PCs.
The wrapper at guest 0x0012B050 forms the mapped descriptor and checks the Result.

## Next exact work

Implement the observed APT SetAppCpuTimeLimit only after tracing its actual resource
state. The pinned APT handler calls PM:APP UpdateResourceLimit(CpuTime, value);
it is NOT a standalone stored percentage/success stub. PM and kernel limit behavior
remain uninspected in this checkpoint. Read that path, validate arguments/state and
determine implications for the current single-process kernel. Do not fake enforcement
or apply an unrelated thread priority. Then rerun original game code with all explicit
modes on a NEW empty private shared archive and follow the next observed operation.

Primary pin: azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
Read src/core/hle/service/cfg/cfg.cpp (6eb7302a942c8b34eaa7268eb0411a916a195020),
cfg_defaults.cpp (424664396a50c369dd7401b97372b924e28def2e),
apt/apt.cpp (e40bd9c82633aeac18b6116c11a931068bab0b75),
apt/apt_u.cpp (57539fbee163a7fbb0119222cd15abf36ea478cb).
Exact references also in private references.json; keep the pin, not mutable upstream.

## Scratch and reproducible run

Root /mnt/data/lego_recovery/. Source repo/, private AOT generated2/,
code restored/code.bin, raw RomFS game/prepared-romfs/romfs.bin,
parts/manifest romfs-library-roundtrip/. Builds build-gcc/, build-clang/, build-asan/.
Work and current captures cfg-checkpoint/. New private-state/cfg-* roots are
guest-created TEST state, not recovered NAND. Preserve unknown saves.
The paired validator removes only its own just-created, verified 20-byte gamecoin.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
mkdir -p private-state
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/private-state/apt-limit-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty --romfs game/prepared-romfs/romfs.bin --gpu-vram-mode reference-zero --display-clock-mode reference-idle --cfg-profile reference-stereo
```

Expected exit 3: untouched APT 0x004F0080 at round 238.
Use clang++ for Clang; omit LEGO_AOT_DIR for ROM-free sanitizers.
Matrix chunks: `python -S cfg-checkpoint/validate_matrix.py 0 6`, then `6 13`,
then `13 20`. Start 0 creates a new root. Trace drivers refuse existing captures;
select a new capture directory rather than overwrite unknown results.
Never silently enable/drop explicit PTM/VRAM/display/CFG modes.

## Recovery and persistence mandate

GitHub GTTeancum/Lego-Undercover-3DS-Recomp main; Library /LEGO-Chase-Recovery/.
Restore the latest checkpoint/MD, not historical source. Verify the archive's
CHECKPOINT-MANIFEST.json with verify_checkpoint.py. SOURCE-INDEX.json records
all exact Git blobs/modes, including ignored tracked reports. Restore its index
faithfully, never force-push reconstructed local history.

Original private inputs have separate backups: code.bin;
LEGO-Chase-current-AOT-599pages-2026-10-03.tgz (unpacks generated2/);
Prepared-RomFS/ two raw parts and romfs-parts.json (402653184 + 366526464 bytes).
Reassemble using repo/tools/restore_romfs_parts.py with the manifest beside parts.
The restore refuses existing outputs; verify instead of overwrite. No CCI extraction
or user reupload is needed. Raw RomFS is 769179648 bytes; native view offset 4096,
view length 769175552. Keep all integrity tables.
RomFS SHA: 6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf.
Code SHA: 5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
AOT SHA: 2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9.
Historical CCI SHA: 3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525.

The new backup nests unchanged LEGO-Chase-source-checkpoint-237def7.tgz under
historical/ with its handoff. That contains prior private captures and nested history.
Extract history separately, NEVER over current repo/. New source/evidence excludes
code/AOT/raw RomFS/CCI/7z/.git/native binaries/build directories. Private captures
must never enter public GitHub. Scratch may reset; Library/GitHub are recovery
paths, not permanent scratch. Actions snapshots expire after 30 days.
Publish tested source/reports and durable backups when tools permit. Never fabricate
CI, persistence, renders or gameplay. Post updated MD and source/evidence every turn.

## Hosted confirmation

GitHub Actions run 37410669875 on implementation 1bc3c7d passed both GCC and Clang
jobs. These are ROM-free suites, separate from the full local original-game runs.
