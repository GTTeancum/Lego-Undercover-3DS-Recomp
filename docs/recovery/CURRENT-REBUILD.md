# LEGO Chase Begins — canonical continuation handoff

Updated October 6, 2026. Work in assistant scratch, NOT the user's PC or Work.
Project: LEGO City Undercover: The Chase Begins, Nintendo 3DS USA. This is a
headless native startup reconstruction, NOT playable. Post a downloadable updated
Markdown handoff and recoverable source/evidence checkpoint every work turn.
Show only meaningful genuine game visuals; none exist at this checkpoint.

## Canonical source: reconciled, not the older pending variant

Implementation: `274806aef6fdbba3dec5abbc17ee7c59a894f672`.
Exact tested/uploaded tree: `dbd7c64d873de0da2a2c9678e54bb244c8297b5d`.
Parent main: `4e6c9cdd3ea33554eaa0591c1bb71c11780c91cb`.
Parent tree: `5293eb38219f5ba1871ed24c04ecb788d586b3b3`.
The appended downloadable receipt records the final report/delivery commit.
Use freshly read remote parents and force=false with expected_sha when publishing.
Local Git is a reconstructed snapshot/index, NOT the remote history.

This turn began with CORE1-BUDGET-PENDING based on 095af71. That archive passed
440 manifest-file and 356 source-blob checks. An independent reference-dual variant
was implemented/tested. Before publication, main advanced with overlapping work to
4e6c9cd. We retained the newer remote scheduler instead of overwriting it. Its
source snapshot was downloaded and reconstructed to the exact parent tree above:
Actions run 37472326460, artifact 11416783537; ZIP SHA
0f456ad631f0273fa0ed21efdc9ac327595af4095dacf7150a5931342e31d871.
Inner source tar SHA c095d4de43bcbcbd2353108785d4c4e0f8ba3f89e7c7d3e765d3bb3723ad6306.
Only the verified launch-header/resource-ceiling addition was carried onto that
source. The alternative scheduler is preserved privately, NOT merged or canonical.

## Actual current boundary

With all explicit options and the original ExHeader:

```text
cpu_ticks=4839693 core0_instructions=2180849 core1_instructions=0 quota_transitions=0
display_periods=1 guest_now_ns=18051022
app_cpu_time_current=30 maximum=30 core0_only=0 core1_enforcement=diagnostic_windows
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=248
last_ipc_session=hid:USER request_header=0x000a0000
```

The original game maps/protects the existing 32 KiB stack alias, creates the actual
processor-1 worker and changes its priority to 49. There are four threads and 22
handles. The child is Ready at 0x00104DF4, TLS 0x1FF82600. It has executed ZERO
instructions; the budget remains in its system phase, next deadline 5180070.
The main thread reaches HID before the worker's first eligible application window.
Synthetic tests of both-core execution are not proof this game's worker ran.

HID is discovery-only. Command 0xA is GetIPCHandles and remains untouched. No input
shared page, event handles, controller state, sample timestamps, polling timer or
successful HID reply is supplied. Words after the zero-parameter header are stale
buffer contents, not additional arguments. All 6 MiB of VRAM remains zero under
the inherited explicit reference-zero policy. No logo, rendered frame, main menu,
executed shader, audio, useful screenshot or gameplay has been demonstrated.

Without --exheader, diagnostic-dual reaches the same HID call but retains maximum
80. Strict default still stops before processor-1 CreateThread at 0x00102FCC,
round 245, time 16713681 ns. Diagnostic mode's default block budget stops earlier;
use --block-limit 100000000. Do not silently enable or omit explicit policies.

## Verified original launch metadata

Recovered the original CCI by streaming the existing six split archive parts,
checking its full 1073741824-byte SHA against the established image. The NCCH
ExHeader hash also verified. The preserved 2048-byte ExHeader SHA is
`d7641f0a3bb89697ca8f0751e88d31703b54aa185ac78f9e74e41169dfcdc004`.
Its descriptor at 0x210 is 0x009E: Multi scheduling, maximum CPU 30. Program ID
00040000000AD500, main ideal processor 0, priority 48, application category 0.
Affinity bits are 1; do NOT invent a prohibition of the observed core-1 creation
from that field. The pinned SVC path accepts processor 1.

New optional --exheader FILE is accepted only with --cpu-mode diagnostic-dual.
The launcher validates exact length, full SHA and expected title fields before
configuring the actual kernel application resource object. Maximum becomes 30,
current initially 0, and the original APT setter later sets current 30. Wrong or
short inputs, incompatible mode and live policy replacement fail without a launch.
Layout parsing alone is not authenticity. No raw title bytes are in public tests.
Without the option, the inherited ceiling remains 80; this avoids silently changing
old commands. The recovered header is now a small private prepared input, so another
CCI extraction is unnecessary once the checkpoint/private backup is restored.

This verifies launch metadata, NOT instruction latency, full firmware launch policy
or cycle-accurate scheduling. The parent diagnostic model issues one recorded A32
instruction per logical core per tick, core 0 before core 1, with independent
contexts and explicit app windows. It is single-host-threaded, not parallel hardware.
Its core selection, quota/event algorithm, recorded stepping, priority operation,
HID discovery and memory alias behavior were preserved from 4e6c9cd.

Six implementation files changed: CMakeLists.txt, src/host/launch_header.h,
src/host/main.cpp, src/runtime/ctr_kernel.h, src/runtime/ctr_dual_core.cpp,
tests/ctr_launch_header_test.cpp. The kernel change adds prelaunch resource setup;
no GPU, opcode/vendor/AOT, memory backend, filesystem or HID handler is changed.

## Final canonical verification and evidence

Full GCC/Clang native builds link all 599 unchanged private AOT pages. All 45 CTest
suites pass with each compiler; all 45 ROM-free Clang ASan/UBSan suites pass with
leak checking/halt-on-error. All 44 parent suites are retained, plus launch-header
coverage. Ten paired CLI/startup cases match stdout, exit and owned test-file bytes.
They cover authenticated and inherited launch, strict mode, invalid identity/length/
path, incompatible mode, missing/empty arguments and default instruction budget.

Six paired final capture files match exactly: thread/CPU/quota state, GPU words,
serialized uploads, GSP page, full IPC and complete VRAM. The raw state images also
match the alternative at this common stop, except its differently reported metadata.
This is not an independent CPU oracle. Original code.bin, raw RomFS and all 603
AOT archive members match their private backups. Full IVFC verification was not
repeated; the raw SHA matches the previously verified image. Registry 111043 blocks
and 545111 words remain STATIC inventory, not frame counts or progress percentages.
No Windows/macOS build. No game screenshot is presented from synthetic test data.

Canonical evidence: launch-checkpoint/. build_all.py/build-status.json and
ctest-final-{gcc,clang,asan}.txt; validate_final.py/matrix-summary.json (10 cases);
capture_final.py and final-capture-{gcc,clang}/ (6 files each); native-proof.json,
comparison.json and identity-proof.json. Capture hosts link canonical libraries;
they only add final logging, not production IPC changes. Binaries are excluded.
publication_rebuild.py and publication-build.json record the GCC/Clang host rebuild
and all45 retests after an equivalent zero-print expression was matched to upload.

The separately tested alternative has 45 DIFFERENT suites and a 15-case matrix.
Do not combine those counts or use them instead of final canonical validation.
Its source/evidence and unused publication plans remain historical/private only.
Current reconciliation and setup notes distinguish both implementations explicitly.

## Next exact work

Implement HID GetIPCHandles from the pinned service's actual shared-memory/event
ownership contract. Inspect hid.cpp, its shared layout, constructor and sampling
schedule before returning handles. Check handle-allocation rollback, lifetime,
permissions and the observed mapping. Do not return dummy events or fabricated
controller samples. Rerun unchanged game code, then observe when the original
processor-1 worker actually executes. Preserve the title-header input and explicit
diagnostic timing limitation; a created worker is not proof of executed gameplay.

Reference pin: azahar-emu/azahar at 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
Title layout: src/core/file_sys/ncch_container.h. Launch initialization:
src/core/hle/kernel/resource_limit.cpp. SVC priority path: kernel/svc.cpp and
kernel/errors.h. HID command identity: service/hid/hid_user.cpp. Kernel/service
paths have prefix src/core/hle/. Keep pinned references, not mutable upstream.

## Canonical scratch and reproduction

Root /mnt/data/lego_recovery/. Source repo/. Full builds build-gcc/, build-clang/;
ROM-free sanitizers build-asan/. Code restored/code.bin; private AOT generated2/;
raw RomFS game/prepared-romfs/romfs.bin; parts/manifest romfs-library-roundtrip/.
New small prepared input: prepared-launch/exheader.bin and launch-manifest.json.
Current evidence launch-checkpoint/. verified-launch.* roots are owned TEST state,
not recovered NAND. Preserve unknown saves; validators remove only their own
just-created byte-verified gamecoin when pairing compilers on the same path.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/hid-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty --romfs game/prepared-romfs/romfs.bin --gpu-vram-mode reference-zero --display-clock-mode reference-idle --cfg-profile reference-stereo --cpu-mode diagnostic-dual --block-limit 100000000 --exheader prepared-launch/exheader.bin
```

Expected exit 3 at HID 0x000A0000, round 248, maximum30 and zero core1 instructions.
Use clang++ for Clang; omit LEGO_AOT_DIR for ROM-free sanitizer tests.

## Durable recovery and every-turn mandate

GitHub GTTeancum/Lego-Undercover-3DS-Recomp main. Library /LEGO-Chase-Recovery/.
Restore the newest source/evidence archive; verify CHECKPOINT-MANIFEST.json and
SOURCE-INDEX.json before use. The latter records exact Git paths/blobs/modes,
including ignored tracked reports. Reconstruct the index, not invented remote history.
The final downloadable receipt identifies the delivery commit and saved filenames.

The private checkpoint includes canonical source/evidence and prepared-launch/.
It preserves the original CORE1-BUDGET-PENDING archive/handoff unchanged, plus the
concurrent 4e6c9cd source snapshot and a separately labeled alternative-source
archive. Extract historical material separately, NEVER over current repo/.
The alternative source is alternate-reference-dual-repo/ locally; its evidence
core1-live-checkpoint/ includes the original ExHeader recovery driver/receipt and
private prefix. Its --cpu-mode reference-dual commands are NOT canonical commands.
The original pending archive retains earlier private evidence/nested history.
The concurrent commit's historical private traces were not recovered; new canonical
captures were made here. Do not cite unrecovered traces as newly checked evidence.

Large inputs have separate existing Library backups: code.bin;
LEGO-Chase-current-AOT-599pages-2026-10-03.tgz (unpacks generated2/);
Prepared-RomFS/ two uncompressed parts plus romfs-parts.json. Part sizes 402653184
and 366526464. Restore via repo/tools/restore_romfs_parts.py, which refuses overwrite.
Raw RomFS size769179648, native view offset4096/length769175552. Preserve tables.
RomFS SHA 6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf.
Code SHA 5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
AOT SHA 2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9.
CCI SHA 3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525.
Prepared launch destination: /LEGO-Chase-Recovery/Prepared-Launch/exheader.bin.

No code.bin, private AOT, raw RomFS/parts, CCI/7z, .git or compiled binaries/build
products are embedded in this source/evidence archive. Private captures/ExHeader
must stay OUT of public GitHub. Scratch may reset; Library/GitHub/checkpoints are
recovery paths, not permanent scratch. Actions artifacts expire after 30 days.
Never invent publication, CI, persistence, hardware parity, screenshots or gameplay.
Post an updated downloadable MD and recoverable checkpoint every work turn.

## Hosted confirmation

GitHub Actions run 37478931314 on implementation 274806a passed both GCC and Clang
jobs. These are ROM-free hosted suites, separate from full local original-game runs
and the locally completed Clang ASan/UBSan suites.
