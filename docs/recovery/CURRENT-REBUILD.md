# LEGO Chase Begins — canonical recovery handoff

Updated October 5, 2026. Read this first. Continue in scratch, not the user's PC
or Work. Project: LEGO City Undercover: The Chase Begins, Nintendo 3DS USA.
This is NOT LEGO Batman Wii/Xbox. Historical Recovery F/J gameplay is not current
proof. Current executable: headless native startup diagnostic, not a playable port.

## Published source and recovery

Implementation commit: `ce38bf7c50731559bb18b92cf72f2f227a4780f5`.
Tested/uploaded implementation tree: `195e0be704b8217d55539e6af972e9bacae86a7c`.
Base main: `26aeb48cdc3c420e768d1ca129ac89b0e1d7dec4`.
Restored baseline tree: `66be217d456969583ce484109c7c29c982a4abc7`.
All 321 prior checkpoint-manifest files verified. Private code/AOT archives were
already present in restored/; no user upload, original-ROM extraction or PC access
was needed. The baseline real OpenFile stop was reproduced before editing.
The delivery receipt appended to the downloadable handoff records final commit,
CI and backup. Local Git is a verified snapshot, not full remote history.

## Actual current execution boundary

A NEW empty shared archive with explicit `--ptm-step-mode empty` passes the real
OpenFile request for UTF-16LE `/gamecoin.dat`, 28 path bytes including NUL,
archive handle 1, flags 3 (read/write), attributes 0:

```text
OpenFile request: 080201c2 00000000 00000001 00000000 00000004 0000001c
                  00000003 00000000 00070002 0036a046
reply:            08020042 00000000 00000010 00060018
GetSize request:  08040000
reply:            080400c0 00000000 00000014 00000000
```

The game receives real session handle 0x00060018 and size 20. It next stops at:

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=75
last_ipc_session=fs:File requested_service= request_header=0x08030102
request=08030102 00000000 00000000 00000014 00010001 0000014a 0ffff600
```

This is Write, offset 0, length 20, raw flags 0x10001, read-only mapped input
descriptor 0x14A, buffer 0x0FFFF600. The request and its guest-built input remain
untouched. The private logging-only trace records the exact pending input.
The host has NOT written that input or seeded a Play Coin file.

All new on-disk gamecoin.dat files remain 20 zero bytes from the guest's earlier
CreateFile, SHA-256 de47c9b27eb8d300dbb5f2c353e632c393262cf06340c4fa7f1b40c4cbd36f90.
No completed save initialization, Read/Write, main menu, renderer, audio, controls,
completed initializer count or gameplay is established. Dispatch rounds are not frames.

Keep three branches separate:
- Fresh + explicit empty PTM: the new Write stop at round 75.
- Fresh WITHOUT empty PTM: GetTotalStepCount stop at round 71, unchanged.
- Existing file: OpenFileDirectly 0x08030204 at round 71, unchanged. This branch
  skips fresh initialization and must not be used as proof that it is resolved.

## Implementation and limits

Added SharedArchiveFile ownership and SharedArchiveMounts::OpenFile; added
FsFileService, with only exact GetSize implemented. FS_USER OpenFile validates
initialization, exact header/type/static descriptor, full readable UTF-16 span,
address arithmetic and path encoding before host opens. Existing IPC response
preflight runs before file/handle effects. Unknown commands remain explicit stops.

Only known read/write/create mode bits are admitted. Empty/create requests return
pinned extdata UnsupportedOpenFlags. Accepted read, write and read/write requests
all open the host file read/write, as the pinned fixed-size extdata backend does.
Transaction ID and attributes are ignored by that path; they are not implemented.

The root stays pinned to its original directory descriptor. Archive and guest
parents are opened relative to descriptors without following symlinks. The leaf
uses no create/truncate flags and is checked as a regular file before/after open;
its device/inode must match. Nonblocking open prevents a substituted FIFO hang,
then the nonblocking flag is cleared. Multiple hard links are rejected as host
containment policy. Dot/empty/control/path-length restrictions remain stricter
than firmware. This is not protection against a malicious host administrator.

Each open gets a distinct kernel session with an owned actual descriptor. Duplicated
kernel handles share the endpoint; closing fs:USER does not close files. The final
file-session reference releases the descriptor. GetSize returns its captured u64
size without reopening the pathname. Tests cover zero and greater-than-4-GiB sizes.
Kernel CloseHandle works through reference lifetime, but the distinct file IPC
Close operation is NOT implemented. Read, Write, Flush, SetSize, subfiles, linked
file sessions and CloseArchive remain unsupported.

Handle exhaustion returns the existing kernel Result and a null moved handle,
without leaking descriptors. Unknown host errors cause an IPC host stop and do not
commit a success response. A real permission failure is covered in the regression.
Non-POSIX safe opens remain unsupported; no Windows or macOS build is claimed.

Timing is explicitly incomplete: pinned FS_USER::OpenFile sleeps for an extdata
open delay of 3085068 ns. This reconstruction implements synchronous file access
and replies only, NOT that delay, worker scheduling or timing parity. Kernel time
stays zero. No artificial advance, event signal, wakeup or Break bypass was added.
Inherited APT/NDM, shared clock/RTC, explicit empty PTM and CFG discovery limits stay.

Only CMakeLists.txt, fs_shared_archive.*, fs_user_service.*, fs_file_service.h,
and tests/ctr_fs_open_test.cpp changed for implementation. The original code,
all private AOT members, vendor, IPC router, kernel, PTM, APT, NDM and CFG are unchanged.

## Validation and evidence

Full 599-page GCC and Clang builds succeeded. Final GCC 16/16 CTest, Clang 16/16
CTest and Clang ASan/UBSan 16/16 ROM-free suites passed. Leak checking and
halt-on-error remained enabled. Eight original-game scenarios match byte-for-byte
between compilers: fresh, existing, mode-off, alternate RTC, no root, missing
archive, invalid root, invalid mode. No bad-root directory or fake save was created.

New tests cover actual descriptor access flags, exact replies, independent and
duplicated sessions, final-reference closure, FS-session independence, u64 sizes,
file preservation, UTF-16 and pointer guards, invalid modes/archives, symlinks,
hard links/FIFOs, root/leaf replacement, handle exhaustion and permission failure.
An initial test compile had a mixed-auto declaration, then was corrected. A forced
RLIMIT_NOFILE=0 test interfered with sanitizer mapping introspection; an isolated
system_error reproducer shows the same diagnostic without project code. The final
regression uses EACCES instead, not sanitizer suppression. An interrupted sanitizer
build was resumed; the final complete runs passed. Earlier failures remain in logs.

All 603 regular files in the private AOT archive match byte-for-byte; 599 are C++
pages. The 111043 blocks and 545111 raw words are STATIC registry validation counts.
Public evidence: reports/recovery-host/FS-OPENFILE.md, OPEN-FRESH-GCC.txt and
OPEN-EXISTING-GCC.txt. Full current logs/scripts: openfile-checkpoint/ in the backup.
The private trace is a logging-only replacement IPC translation unit, not a game
patch or production override. The ordinary GCC/Clang runs use unchanged production
IPC and independently reach the same Write boundary.

## Next exact work

Implement the observed fixed-size file Write, through the existing real file
session and owned descriptor, NOT an external preload of gamecoin.dat. Inspect
pinned disk_archive.cpp and archive_extsavedata.cpp write/flush/error behavior.
Validate mapped-buffer permissions and full source readability before I/O, guard
u64 offsets and lengths, preserve fixed extdata size, report actual write counts
and partial host failures honestly. Rerun original code on a NEW empty root with
explicit empty PTM; then inspect the next real request rather than guessing Close.

Important flags detail: pinned fs/file.cpp decodes flush using flags & 0xFF and
update_timestamp using flags & 0xFF00. The observed 0x10001 triggers flush, not that
timestamp mask; bit 16 is outside both. Do not label it a timestamp bit by assumption.
General IPC mapping, async FS and disk durability are not complete. Resolve policy
explicitly rather than manufacturing successful save completion or artificial wakes.

Keep the existing-file OpenFileDirectly path as a separate regression. Do not jump
to guessed RomFS, graphics or gameplay. The actual next execution determines work.

Primary source pin: azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
Inspected this turn: src/core/hle/service/fs/fs_user.cpp (OpenFile), archive.cpp
(OpenFileFromArchive), archive.h and file.cpp (Connect/GetSize/Write/Close).
Prior inspected definitions: src/core/file_sys/archive_extsavedata.cpp,
savedata_archive.cpp, path_parser.cpp, errors.h. See prior reports for PTM limits.

## Scratch locations and reproduction

Root: /mnt/data/lego_recovery/. Source: repo/. Private AOT: generated2/.
Verified input: restored/code.bin. Builds: build-gcc/, build-clang/, build-asan/.
Current logs: openfile-checkpoint/. Prior logs: ptm-checkpoint/, createfile-checkpoint/.
The attached 26aeb48 source archive remains at /mnt/data/ for baseline recovery.

NEW matrix state: private-state/open-validation.9lz42yxz/ (fresh/, mode-off/,
rtc-next-day/ contain guest-created zero files; missing/ is empty). Other current
owned roots: open-baseline.*, open-first.*, open-size.*, open-trace.*. Historical
restored test roots were left unchanged. Never use an unknown console save for a
fresh test. The validator resets only exact freshly-created zero files within its
own unique root to compare both compilers at the same pathname.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
mkdir -p private-state
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/private-state/fs-write-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty
```

Expected: Write header 0x08030102, round 75, exit 3 (diagnostic stop, not boot success).
Use clang++ for Clang; omit LEGO_AOT_DIR for ROM-free builds. Sanitizer flags and
commands are in openfile-checkpoint/build_remaining.py and finish_asan.py. The
final ctest-asan.txt supersedes the earlier failure log and asan-finished.json.

## Durable recovery and every-turn mandate

GitHub: GTTeancum/Lego-Undercover-3DS-Recomp, main.
Private Library: /LEGO-Chase-Recovery/. Restore the latest source/evidence checkpoint
and handoff, code.bin, and LEGO-Chase-current-AOT-599pages-2026-10-03.tgz. Six original
ROM archive parts remain under Game-archive/; the large CCI is not needed yet.
Source snapshots run on main pushes but expire after 30 days. Library backups,
not an assertion of permanent scratch, are the durable recovery path.

Code SHA-256: 5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
AOT archive SHA-256: 2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9.
Historical CCI SHA-256: 3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525.

Verify CHECKPOINT-MANIFEST.json, restore repo/ and logs under the root above,
unpack the AOT archive there (creates generated2/), place code.bin in restored/.
Use SOURCE-INDEX.json to restore the exact Git paths/blobs/modes, including tracked
ignored evidence logs. Local Git is a snapshot. Publish using the current remote
parent through GitHub connector with force=false; never force-push local history.
The connector worked this turn. No Work or user-PC development was used.

Work in moderate tested checkpoints. Push source and evidence before ending.
Keep game bytes, private AOT, compiled game binaries and test state OUT of public Git.
Update this canonical handoff and POST A DOWNLOADABLE COPY EVERY WORK TURN, plus a
durable source/log backup. A new chat must be able to restore files, rebuild,
reproduce the actual stop and continue from this file without unnecessary uploads.

## Hosted CI confirmation

GitHub Actions run `37297822986` on implementation `ce38bf7` completed successfully
for both GCC and Clang jobs. Hosted tests are ROM-free, not game-data execution.
