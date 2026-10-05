# LEGO Chase Begins — canonical recovery handoff

Updated October 5, 2026. Read this first and continue in scratch, not the user's
PC or Work. This is LEGO City Undercover: The Chase Begins (3DS USA), not LEGO
Batman. Historical Recovery F/J gameplay is NOT current reconstruction proof.
The current executable is a headless native startup diagnostic, not playable.

## Current source and actual stopping points

Implementation: `0c4b013b9d96d664625c6135a216874153254ff6`.
Tested/uploaded source tree: `e53a61304ccbe37b74939d34d8af7ed27655cbab`.
Base main: `eea3febef825c4e7ce5f1602ac44435dd15fcf52`.
Restored baseline tree exactly matched `ff66d935f699cbc33ab9ee5e30f5083373863e9f`.
All 261 prior checkpoint files passed their manifest verification. The previous
PTM stop was reproduced before editing; its log differed only in test-root name.
An already tracked ignored evidence .log needed forced indexing when restoring
Git. This was not a source discrepancy. Local Git is a snapshot, not full history.

NEW test archive plus `--ptm-step-mode empty` now passes these real requests:

```text
GetTotalStepCount request: 000c0000
reply: 000c0080 00000000 00000000
GetStepHistory request: 000b00c2 00000018 00000000 00000000 0000030c 0ffff5c8
reply: 000b0042 00000000 0000030c 0ffff5c8
output: 48 zero bytes, 24 hourly u16 entries
```

The next exact untouched request is FS OpenFile, NOT OpenFileDirectly:

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=73
last_ipc_session=fs:USER requested_service= request_header=0x080201c2
request=080201c2 00000000 00000001 00000000 00000004 0000001c
        00000003 00000000 00070002 0036a046
```

Archive handle 1; UTF-16LE `/gamecoin.dat` at 0x0036A046, 28 bytes including NUL;
open flags 3 (read/write), attributes 0. Host exit 3 is a diagnostic stop.
No OpenFile response or file-session handle has been invented.

Distinguish three runs:
- Fresh archive WITH explicit empty PTM mode: OpenFile above, round 73.
- Fresh archive WITHOUT that option: original GetTotalStepCount stop, round 71.
- A second process using the created file: prior FS OpenFileDirectly stop,
  header 0x08030204, round 71. It bypasses the fresh-file initialization branch.

All created gamecoin.dat files still contain only 20 zero bytes from guest
CreateFile, SHA-256 de47c9b27eb8d300dbb5f2c353e632c393262cf06340c4fa7f1b40c4cbd36f90.
They are incomplete NEW test state, not recovered NAND data or initialized saves.
No menu, graphics, audio, controls, completed initializer count or gameplay proof.

## PTM implementation policy and scope

`--ptm-step-mode empty` explicitly selects a desktop profile with no step events
and no sensor source. It is not silently enabled by a mount or guessed from a
missing file. The launcher prints this provenance. The default remains
Unconfigured, with PTM commands unsupported. Invalid mode text is rejected.

Pinned Azahar's GetTotalStepCount returns zero and is labeled STUBBED.
GetStepHistory writes one u16 per hour from its steps-per-hour setting and is
also labeled STUBBED. Our deliberate empty profile uses zero for all requested
hours/timestamps, not a claim about the user's console or default Azahar setting.
No fake coin balance, step activity, pedometer hardware, battery state or default
coin file is copied from the reference. cfg:u remains discovery-only.

PTM accepts only exact total/history headers. History supports the observed
write-only mapped descriptor with exactly hours*2 bytes. A 2048-hour/4096-byte
cap is host work policy, not a firmware limit. Complete output span, permissions,
address arithmetic and response writability are preflighted. Zero hours requires
no pointer dereference. Guest writes invalidate exclusive reservations. Output
aliasing its IPC response is an explicit host stop. Other permissions, mixed-region
buffers and general kernel mapped-buffer translation are not reconstructed.
The opaque start-time u64 causes no arithmetic overflow or clock advancement.

The total/history operations allocate no handles and change no events, threads,
service session state or kernel time. This runner's kernel time remains zero.
Other PTM requests remain stops even when empty mode is selected.

Changed files: src/services/ptm_service.h/.cpp; src/runtime/ctr_runner.h/.cpp
(mode wiring only); src/host/main.cpp (explicit CLI and provenance); CMake files;
tests/ctr_ptm_steps_test.cpp; reports and this handoff. No original game bytes,
private generated pages, vendor implementation, kernel scheduler, IPC router,
FS implementation, APT, NDM or CFG implementation was changed.

## Validation and evidence

- Full GCC and Clang builds include the unchanged 599 private page C++ files.
- GCC 15/15 and Clang 15/15 CTest suites passed.
- Clang ASan/UBSan 15/15 ROM-free suites passed; leaks/halt-on-error enabled.
- Eight real startup scenarios have byte-identical compiler logs: fresh, existing,
  mode-off, alternate RTC, no root, missing archive, invalid root, invalid mode.
- A logging-only IPC trace confirmed the actual total reply, mapped-buffer reply,
  exact 48 output bytes and following OpenFile path/flags. No game patch used.
- All 603 regular files in the private AOT backup match; 599 are page C++ files.
- 111043 registry blocks and 545111 raw words are STATIC validation counts,
  not frames, executed-instruction counts or gameplay completion.

Public report: reports/recovery-host/PTM-EMPTY-HISTORY.md.
Public real logs: PTM-FRESH-GCC.txt and PTM-EXISTING-GCC.txt in that directory.
Complete logs, validator, trace source, binary fingerprints and private wrapper
inspection: /mnt/data/lego_recovery/ptm-checkpoint/ and the private backup.
The first test compile had enum-name/type mismatches; they were corrected before
all final passing tests. No intermediate failed build is reported as successful.
The delivery receipt below/in the attachment records hosted CI and final commit.

## Next exact work

Implement the observed FS OpenFile request for the existing 20-byte file in the
explicit shared archive. Inspect pinned fs_user.cpp, archive.cpp file-session
handling, and archive_extsavedata.cpp fixed-size backend semantics first.
Return a real contained file-session handle with correct lifetime/ownership and
permission behavior, not a dummy success. Preserve existing files and the pinned
root/no-symlink containment policy from SharedArchiveMounts. Validate full UTF-16
path and static descriptor before host access; never follow guest traversal.
Then run the original code again on a NEW archive with empty PTM explicitly
selected to expose the next real file operation (do not assume it is Write).

Keep the existing-file OpenFileDirectly branch as a separate regression case.
Do not equate handling OpenFile with handling OpenFileDirectly. Do not pre-seed
or externally initialize gamecoin.dat to skip the fresh branch. File read/write,
close/flush, initialized saves, RomFS, rendering and gameplay remain open.

Primary source pin: azahar-emu/azahar
86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
This turn inspected src/core/hle/service/ptm/ptm.cpp and src/core/hle/ipc.h.
Also inspected devkitPro/libctru ptmu.c blob 74920543afee138d5571f0a80a88e1a2b26f6c3f;
its hours-versus-bytes descriptor discrepancy was NOT copied. Actual guest
wrappers at 0x0012B4BC/0x0012B508 corroborate the wire layout used here.
See PTM-EMPTY-HISTORY.md for the distinctions and reference paths.

## Working files and reproduction

Root: /mnt/data/lego_recovery/.
Source: repo/. Private AOT: generated2/. Verified code: restored/code.bin.
Builds: build-gcc/, build-clang/, build-asan/. Current logs: ptm-checkpoint/.
Prior logs: createfile-checkpoint/. Earlier source archive and AOT remain in
restored/; delivered eea3feb archive remains at /mnt/data/.

The matrix's newly owned test state is under private-state/ptm-validation.7n3glxfr/.
fresh/, mode-off/ and rtc-next-day/ subdirectories retain newly guest-created
zero files; missing/ is empty; nonexistent/ was never created. Other ptm-baseline.*,
ptm-total.*, ptm-history.* and ptm-trace.* roots are this turn's NEW test state.
Historical private-state directories restored from eea3feb were left untouched.
Do not use an unknown existing console save as a fresh test root.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
mkdir -p private-state
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/private-state/fs-open-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty
```

Expected stop is OpenFile header 0x080201C2 at round 73. For Clang set compiler
clang++; for ROM-free tests omit LEGO_AOT_DIR. Sanitizer flags and full commands
are preserved in ptm-checkpoint/build_remaining.py. The validator makes a unique
owned root each run and never removes unknown preexisting state.
No Windows build is claimed. POSIX shared file creation remains the prior backend.

## Durable recovery and every-turn mandate

GitHub: GTTeancum/Lego-Undercover-3DS-Recomp, branch main.
Private Library: /LEGO-Chase-Recovery/.
Restore the latest source/log checkpoint and handoff, plus code.bin and
LEGO-Chase-current-AOT-599pages-2026-10-03.tgz. Six original ROM parts remain under
Game-archive/; the large CCI was not needed or copied into this turn's scratch.
Source snapshots run on main pushes, but their 30-day retention is not permanent.
Use Library backups, not a promise of permanent scratch, for durable recovery.

Code SHA-256: 5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
AOT archive SHA-256: 2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9.
Historical CCI SHA-256: 3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525.

Verify the archive manifest, unpack source into /mnt/data/lego_recovery and the
AOT archive there (creates generated2/), then place code.bin in restored/.
Restore all indexed files including ignored evidence logs. Local Git is only a
snapshot; publish with verified current remote parents through GitHub connector,
never force-push this local history. Direct container GitHub DNS failed here;
connector reads/writes worked. No user-PC/Work development was used.

Work in moderate tested checkpoints. Push source/reports before ending.
Keep original code, private AOT pages, compiled game binaries and test state out
of public Git. Update this canonical handoff and POST A DOWNLOADABLE COPY EVERY
WORK TURN, plus a durable source/log checkpoint. This file must let a new chat
restore, build, reproduce the exact real boundary and continue without guessing.
Inspect existing backups before requesting another upload.

## Hosted CI confirmation

GitHub Actions run `37295448850` on implementation `0c4b013` completed successfully
for both GCC and Clang jobs. Hosted tests are ROM-free, not game-data execution.
