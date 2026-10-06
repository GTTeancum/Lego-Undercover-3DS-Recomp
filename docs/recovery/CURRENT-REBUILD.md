# LEGO Chase Begins — canonical continuation handoff

Updated October 6, 2026. Continue in assistant scratch, NOT the user's PC or Work.
Project: LEGO City Undercover: The Chase Begins (Nintendo 3DS USA). Headless native
startup reconstruction, NOT playable. POST AN UPDATED DOWNLOADABLE HANDOFF AND
SOURCE/EVIDENCE CHECKPOINT EVERY WORK TURN. Show only genuine meaningful game output;
there is still no logo, rendered frame, main menu or useful screenshot.

## Published implementation and exact baseline

Implementation: `3e9ecc1617fe4c472b4aeb306c292283add67a40`.
Exact tested/uploaded tree: `2dbb59ed4538fb020c9cbbb82fbecc1ad63e8f97`.
Base main: `38f0bbe5aad24bff95cea465f58487333a99cf2e`, tree
`5502c3652bf7ce1f87bbe96b0340411aa5bdd101`. The downloadable receipt records the
subsequent report/delivery commit. Use the fresh remote parent and force=false,
with expected_sha when available. Local Git is a snapshot/index, NOT remote history.

The attached predecessor was restored and verified: 591 manifest files and 342
source blobs, exact baseline tree. Original code/AOT and prepared RomFS parts were
available; no user upload, CCI extraction or user-PC access was needed. The full
baseline reproduced the untouched APT setter at round 238; all three threads were
processor 0. The game-specific ExHeader CPU maximum was NOT revalidated this turn.

## Actual progress: application CPU resource state, not core-1 enforcement

Original APT `SetAppCpuTimeLimit(1,30)` now updates the SAME kernel application
ResourceLimitObject exposed through existing resource handles. Current CpuTime
changes from 0 to 30, with the inherited maximum 80 unchanged. The response is
`004F0040 00000000`, with remaining IPC words cleared. The paired APT getter and
existing SVC resource readback are component-tested; the game has not called that
getter in this run. This is not a disconnected APT percentage or thread-priority edit.

Pinned PM:APP accepts values at or below the current maximum; larger unsigned
values return success without changing state. Accepted values are assignments,
not cumulative reservations. Initialization/registration, exact request shapes and
must_be_one=1 are required in this bounded handler; the latter is stricter than
the reference, which logs other values. No other resource category is implemented.

IMPORTANT: the reference updates a CORE-1 limiter. This host does NOT implement
core-1 preemption, CPU-cycle charging, full multicore execution or title launch
resource-policy reconstruction. An accepted update is supported only while every
live thread is processor 0. It activates a guard: future SVC thread creation for
processors 1..3 stops untouched before allocation, and the runner rejects externally
created/retargeted non-core-0 threads before execution. Default/all processor aliases
retain the inherited resolution to core 0. Invalid IDs retain existing validation.
A core-1 thread already present causes an accepted-value setter to stop unchanged.
The above-maximum no-op does not activate the guard. No fabricated enforcement,
preemption timer, priority change or forced wake is introduced.

## New exact untouched boundary

After accepting 30, original code closes the APT session, releases its mutex and
creates two events. Fresh startup then stops at:

```text
app_cpu_time_current=30 maximum=80 core0_only=1 core1_enforcement=unimplemented
stop=UnsupportedSvc pc=0x0025c9f4 detail=0x00000001 thread=1 dispatch_rounds=243
r0=00000004 r1=0e000000 r2=08045000 r3=00008000 r4=00000003
```

This is `ControlMemory(Map)`, operation 4, destination 0x0E000000, source 0x08045000,
size 0x8000 (32768 bytes), requested permissions 3. The source is mapped read/write;
the destination is unmapped. The source and CPU registers are unchanged across the
stop. Source SHA: c35020473aed1b4642cd726cad727b63fff2824ad68cedd7ffb73c7cbd890479.
No map, alias, copy, successful return or memory-accounting change is implemented.
The last_ipc_session remains APT because this stop is an SVC, not another IPC.

The inherited SVC path previously returned InvalidCombination for this unimplemented
operation. Initial exploration therefore entered the game's fatal-report path,
failed to connect to err:f, and spent the bounded run waiting while display periods
continued. Those exploratory logs are retained, NOT claimed as frame/gameplay
progress. The SVC bridge now leaves all non-Commit ControlMemory requests untouched
instead of inventing a guest error for missing host functionality. Commit behavior
and the memory backend are unchanged. Other ControlMemory operations remain next work.

Existing-gamecoin startup reaches the map at round 235. Fresh time remains
16713681 ns after one display period. No CFG retains round 235 at GetConfig; no
clock retains the old wait at round 228; no VRAM retains transfer round 170; absent
RomFS and unconfigured PTM retain their earlier stops. Existing-gamecoin is not
save readback. Original video memory is still zero under explicit reference-zero
policy; no rendered frame, shader execution or gameplay has been demonstrated.

## Verification and scope

Full GCC and Clang native builds link all 599 unchanged private AOT pages. All 40
CTest suites pass with each compiler; all 40 ROM-free Clang ASan/UBSan suites pass
with leak checking/halt-on-error. Twenty production cases match logs, exits and
newly created test-file bytes across compilers. No previous suite was removed or
changed. New tests cover all 0..80 assignments, extremes/no-op, actual resource
readback, session sharing, handle identity/exhaustion, protected replies, malformed
requests, core-1 guards, untouched non-Commit maps and nonzero synthetic VRAM.

All 86 private capture files match between GCC and Clang. The setter changes the
actual resource value only; time/handles are unchanged across that call. All 1842
GPU words, upload state, the GSP page/epochs and the full 6 MiB bank are unchanged.
The final map's CPU registers and full source span are likewise unchanged. These
are logging-only alternative IPC/runner captures; normal builds independently reach
the same stop. Synthetic test patterns never enter original-game execution.

Final identity checks match code.bin, whole raw RomFS and all 603 regular AOT archive
members. Full IVFC block checking was not repeated; raw SHA matches prior verified
image. Registry 111043 blocks / 545111 words is STATIC inventory, not executed
instruction counts, frames or completion percentages. No Windows/macOS build.

Ten implementation files changed: two CMake files; src/host/main.cpp;
src/runtime/ctr_kernel.h, new ctr_cpu_time.cpp, ctr_runner.h/.cpp, ctr_svc_bridge.cpp;
src/services/apt_service.cpp; tests/ctr_apt_cpu_limit_test.cpp. ctr_kernel.cpp,
production IPC, opcode/vendor/AOT, memory backends, GPU, FS and existing timer/priority
algorithms are unchanged. Runner edits add guards, not CPU scheduling enforcement.

GitHub Actions run 37451651219 on implementation 3e9ecc1 passed both GCC and Clang
jobs. Hosted suites are ROM-free, separate from full local original-game runs.

## Evidence and setup limitations

Current evidence: apt-limit-checkpoint/. Final logs: build-final-gcc/clang/asan.txt,
ctest-final-gcc/clang/asan.txt. validate_matrix.py and validation-summary.json contain
20 paired cases. make_trace.py, inherited-trace-support.inc, map_trace_snippet.inc
and generated trace sources reproduce captures-gcc/clang. verify_trace.py checks
trace-proof.json; verify_inputs.py checks identity-proof.json. Raw captures remain
PRIVATE. Exploratory captures before the explicit map stop are separately named.

setup-notes.txt records early new-test compile errors (fixed before final suites),
finite-call timeouts, unavailable streaming execution, a temporary CMake quote error,
and incomplete matrix attempts followed by the complete new-root run. These are not
hidden or reported as successful final tests. Retain the logs, not compiled objects.

## Next exact work

Inspect pinned SVC ControlMemory and process/VM Map handling, then implement the
observed 32 KiB request with actual backing/alias and source-permission semantics.
Do not allocate fresh zero memory, copy bytes, or return success as a substitute
for a required alias. Check state transitions, alignment/ranges, permissions,
reservation epochs, resource accounting, overlap and rollback. The pinned handler
implementation has not yet been inspected here; only its declaration was fetched.
Rerun unchanged original code in NEW test state and follow the next actual request.
Keep core-1 limitations explicit; do not remove guards merely to pass another stop.

Reference pin: azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
Inspected PM pm_app.cpp (df4ff2c48d66c339fd09c0388414f11cfb64022f), kernel
resource_limit.cpp (210f4d6359c832e5f03f2be1ba4f4a49ef65851c), thread.cpp
(164b5df04fc4a6b7d30de9503b6f17a46e54a935), and the earlier APT setter/getter path.
Exact provenance is in references.json. Do not substitute mutable upstream silently.

## Scratch, rebuild and durable recovery

Root /mnt/data/lego_recovery/. Source repo/; AOT generated2/; code restored/code.bin;
RomFS game/prepared-romfs/romfs.bin; parts/manifest romfs-library-roundtrip/.
Builds build-gcc/, build-clang/, build-asan/. Current work apt-limit-checkpoint/.
private-state/apt-* and apt-limit-validation.* are newly created TEST state, not
recovered NAND. Preserve unknown saves. The paired validator removes only its own
byte-verified fresh gamecoin to repeat with the other compiler.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
mkdir -p private-state
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/private-state/control-map-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty --romfs game/prepared-romfs/romfs.bin --gpu-vram-mode reference-zero --display-clock-mode reference-idle --cfg-profile reference-stereo
```

Expected exit 3: untouched ControlMemory Map, round 243. Use clang++ for Clang and
omit LEGO_AOT_DIR for ROM-free sanitizers. Keep all explicit policies. The matrix
script accepts start/stop indices; start 0 creates a new root and each case saves
progress. Final chunks were 0 3, 3 9, 9 15, 15 20. Trace drivers need new capture paths.

GitHub: GTTeancum/Lego-Undercover-3DS-Recomp main. Library: /LEGO-Chase-Recovery/.
Restore the latest archive, verify CHECKPOINT-MANIFEST.json with verify_checkpoint.py,
and reconstruct all SOURCE-INDEX.json paths/blobs/modes, including ignored reports.
Never force-push local snapshot history. The downloadable receipt names this delivery.
The previous 38f0bbe archive/handoff remain unchanged under historical/. Extract
history separately, NEVER over repo/; it retains earlier captures and nested backups.

Private inputs have separate backups: code.bin, unchanged
LEGO-Chase-current-AOT-599pages-2026-10-03.tgz (unpacks generated2/), and Prepared-RomFS/
two raw parts plus romfs-parts.json. Parts are 402653184 and 366526464 bytes. Restore
using repo/tools/restore_romfs_parts.py with the manifest beside the parts, creating
only the output parent directory first. It refuses existing output; verify rather
than overwrite. No CCI extraction or user reupload is needed. Raw RomFS size
769179648; native view offset 4096, size 769175552. Preserve integrity tables.
RomFS SHA: 6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf.
Code SHA: 5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
AOT SHA: 2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9.
Historical CCI SHA: 3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525.

Scratch can reset; Library/GitHub are recovery paths. Actions artifacts expire after
30 days. Keep game code, derived private AOT, raw RomFS, binaries and private captures
OUT of public GitHub. Publish tested source/reports and save private backups when
tools permit. Never fabricate CI, persistence, enforcement, screenshots or gameplay.
Post the updated Markdown and source/evidence checkpoint every work turn.
