# LEGO Chase Begins — canonical recovery handoff

Updated October 5, 2026. Read this first. Continue in scratch, not the user's PC
or Work. Project: LEGO City Undercover: The Chase Begins, Nintendo 3DS USA.
This is NOT LEGO Batman. Historical Recovery F/J gameplay is not current proof.
The current executable is a headless native startup diagnostic, not playable.

## Published implementation and recovery

Implementation: `4783f2280b68be0593d8185215589a4d683cbd31`.
Tested/uploaded implementation tree: `207e2a111d65a5180141051f3019b1319adb3c41`.
Base main: `e81c90c1538767fb44df92b245d9280e385caa92`.
Restored source tree matched `719a681ab739cc8182e84518193fe86db6e98a80` exactly.
All 388 files in the prior checkpoint manifest verified. Existing private code/AOT
backups were restored without a user upload. The previous Write stop was reproduced
before editing; its log matched after normalizing only the owned test-root name.
Local Git is a verified snapshot/index, NOT the remote commit history.
The delivery receipt in the attached handoff records final commit, CI and backup.

## Actual progress: guest Write and Close now execute

Using a NEW empty shared archive and explicit `--ptm-step-mode empty`, the original
code passes CreateFile, PTM total/history, OpenFile and GetSize, then performs:

```text
Write request: 08030102 00000000 00000000 00000014 00010001 0000014a 0ffff600
Write reply:   08030082 00000000 00000014 0000014a 0ffff600
Close request: 08080000
Close reply:   08080040 00000000
```

This is the guest's own 20-byte write at offset zero, through its real file-session
handle 0x00060018. The disk bytes match the original guest buffer byte-for-byte.
They were NOT preloaded, seeded from an emulator default or written by a helper.
The default-clock file SHA-256 is
`970a8b30f57b772c2c1c5686e634b9e4ab7055b43caec90e64076f81ed08b4b6`.
The logging-only trace also confirms Close retires the backend while the kernel
client handle still exists. The kernel's separate handle lifetime is preserved.

Write-only implementation first exposed Close at round 76; Close was implemented
only after that real request was observed. The final fresh startup stops at:

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=79
last_ipc_session=fs:USER requested_service= request_header=0x08030204
```

## Next boundary is the game's RomFS, not another gamecoin open

The complete pending OpenFileDirectly request is recorded in the private trace.
It selects archive type 3 (SelfNCCH), archive path type 1 (Empty) with declared
length 1, file path type 2 (Binary), length 12, open mode 1, attributes 0.
The file path is three zero u32 words. Pinned SelfNCCH source identifies the first
word 0 as RomFS, returning an IVFCFile backed by the application's actual RomFS.
The observed Empty archive-path byte was 0xE8; do NOT require it to be a NUL or
interpret it as a filesystem name. The path type, not that incidental byte, matters.

No OpenFileDirectly response, RomFS file handle or game asset read is implemented.
No main menu, renderer, audio, controls, completed initializer count or gameplay
is established. The gamecoin write/close sequence is demonstrated, but gameplay
save/load, gamecoin re-reading and full console-format/timing parity are NOT.

Keep branches separate:
- Fresh + explicit empty PTM: guest Write/Close, then RomFS request at round 79.
- Fresh without empty PTM: prior GetTotalStepCount stop at round 71; disk is zero.
- Existing file: OpenFileDirectly at round 71, skipping initialization. The guest-
  created file remains unchanged; this is not evidence of successful file reading.
- Alternate RTC: same final boundary; the original guest changes its stored date.

## Implementation scope and safety policy

Changed fs_file_service.h, added fs_file_service.cpp, extended SharedArchiveFile's
API, wired CMake, added ctr_fs_write_test and adjusted two obsolete unsupported
expectations in ctr_fs_open_test to malformed Write/Close cases. New tests now
cover the valid operations. No original game bytes, private AOT pages, vendor,
IPC router, scheduler, FS_USER, archive path code, PTM, APT, NDM or CFG was changed.

Write validates the exact header, read-only mapped descriptor and the entire
source span before host access, including bytes beyond the part clipped by EOF.
Zero-length input dereferences no source pointer. Source/reply aliasing remains
an explicit host stop. The 1 MiB transfer cap is host work policy, not a firmware
limit. General mapped-buffer translation and cross-region spans are not modeled.
The service copies only bounded input and never modifies the guest source.

Writes use the already-owned descriptor, not the pathname. Beyond fixed EOF gives
ResultWriteBeyondEnd; exactly at EOF succeeds with count zero; crossing EOF clips
using subtraction without u64 overflow. Positive short writes are completed in a
loop, EINTR is retried, and actual partial counts survive host errors. A pre-write
fstat refuses external extent changes or new hard links. These checks do not
protect against a malicious host administrator racing filesystem changes.

Flush uses flags & 0xFF, matching pinned file.cpp. Bit 16 in actual 0x10001 is
NOT labeled a timestamp bit. The next byte is the reference timestamp mask, which
DiskFile ignores. This host uses pwrite and fsync on a flush request: fsync is an
explicit stronger host policy than the reference's fflush, not hardware durability
or timing parity. No guest timestamp updates, artificial clock advancement or
wakeups were introduced. The current runner's kernel time remains zero.

Unknown write/sync errors stop without committing a guest reply and report the
actual bytes already written. Partial host changes are retained, never claimed
rolled back. Tests force 0-byte/8-byte write failures and a post-write sync error.
Close retires descriptor ownership before the host close call to avoid double
closure. Duplicate kernel handles share backend closure; independent opens do not.
Repeated Close returns success and GetSize retains its captured value, following
reference HLE policy, not hardware verification. Write on a closed endpoint stops.
Close errors remain host stops; no fake guest Result is invented.

Read, OpenFileDirectly, standalone Flush, SetSize, CloseArchive, subfiles, linked
file sessions, quotas, transactions, attributes, NAND containers and asynchronous
FS scheduling remain unsupported. POSIX support only; no Windows/macOS build or
full file API, power-loss durability, crash recovery or gameplay save claim.

## Validation and evidence

- GCC full unchanged 599-page baseline build, then changed-source rebuild/relink.
- Clang fresh full 599-page build.
- GCC 17/17 and Clang 17/17 CTest passed.
- Clang ASan/UBSan 17/17 ROM-free suites passed, leak checking/halt-on-error enabled.
- Eight original-game scenarios have byte-identical GCC/Clang logs: fresh,
  existing, PTM mode-off, alternate RTC, no root, missing archive, invalid root,
  invalid PTM mode. Invalid roots stay absent; no-root/missing behavior is unchanged.
- Tests cover exact replies, EOF/zero/64-bit offsets, a greater-than-4-GiB sparse
  file, complete input/response preflight, source immutability/reservation retention,
  aliases, duplicate/independent close ownership, path replacement, external extent
  changes/hard links, partial host failures and flush selection/error propagation.
- Linux fsync interception is linked ONLY into ctr_fs_write_test; real game builds
  and the private logging-only trace use actual host fsync, without fault injection.
- All 603 regular AOT backup members remain byte-identical; 599 are C++ pages.
  111043 registry blocks and 545111 raw words are STATIC verification counts,
  not executed instructions, frames or completed gameplay.

Public report: reports/recovery-host/FS-WRITE-CLOSE.md. Public final startup logs:
WRITE-FRESH-GCC.txt and WRITE-EXISTING-GCC.txt. Full builds/tests/trace/validator:
writefile-checkpoint/ in the private source backup. The private trace only adds
logging to IPC; ordinary production runs independently reach the same boundary.
All final build/test steps passed; no failing test was suppressed or marked passed.
The first attempted streaming container invocation was unsupported; the recorded
normal build driver performed the actual baseline build successfully.

## Next exact work

Implement the observed SelfNCCH RomFS OpenFileDirectly path using the ORIGINAL
game data. Restore the original six archive parts from Library (see persistence
handoff/recovery kit), extract the CCI and verify its recorded hash. Inspect the
actual NCCH/RomFS offsets/format and pinned SelfNCCH/IVFC loader semantics before
returning a real read-only file endpoint. Do not create an empty replacement RomFS,
reuse the writable extdata backend without permission changes, or synthesize data.
Keep extracted game input private and persist it when prepared; don't rely on scratch.

Inspect pinned fs_user.cpp OpenFileDirectly, archive_selfncch.cpp, ivfc_archive.*
and loader setup, then implement only the observed archive/path/file operations.
Validate descriptor sizes, paths and response/handle ownership. Rerun unchanged
code on a NEW empty test root with empty PTM explicitly selected; observe the next
real request before guessing reads, graphics or gameplay. Keep existing-file tests
as a separate branch, never as a bypass of the fresh initialization sequence.

Reference pin: azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
This checkpoint inspected src/core/file_sys/disk_archive.cpp (blob
7125bd3974c0b0cf248b35175b433e1faba7cc43), src/common/file_util.cpp (blob
3168a26224bd79b80623034c900074812036aeda), and archive_selfncch.cpp (blob
6d41020efc3a69676a6ae092833707b599fbf5f9). Earlier exact File Write/Close,
fixed-size extdata and IPC definitions remain in prior reports.

## Scratch, reproducible continuation and durable recovery

Scratch: /mnt/data/lego_recovery/. Source: repo/. Private pages: generated2/.
Verified code: restored/code.bin. Builds: build-gcc/, build-clang/, build-asan/.
Current evidence: writefile-checkpoint/. Prior evidence: openfile-checkpoint/,
ptm-checkpoint/, createfile-checkpoint/. Previous source archives remain /mnt/data/.
NEW matrix state: private-state/write-validation.trppwghk/. Other new roots:
write-baseline.*, write-first.*, write-close.*, write-trace.*. Historical restored
state was left untouched. Fresh and alternate-date matrix files now contain the
guest's own output; mode-off/baseline files remain zeros. Never treat these as
recovered NAND saves. The validator resets only its own exact freshly-created
files to compare compilers at identical paths, never unknown or older state.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
mkdir -p private-state
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/private-state/romfs-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty
```

Expected current stop: OpenFileDirectly 0x08030204, round 79, exit 3. For Clang use
clang++; omit LEGO_AOT_DIR for ROM-free tests. Sanitizer settings and commands are
in writefile-checkpoint/build_remaining.py. Original game data is not in public Git.

GitHub: GTTeancum/Lego-Undercover-3DS-Recomp, main.
Private Library: /LEGO-Chase-Recovery/. Restore the latest source/log checkpoint,
code.bin and LEGO-Chase-current-AOT-599pages-2026-10-03.tgz. Six original ROM parts
are under Game-archive/. The CCI/RomFS was NOT copied into this turn's scratch.
The game-files recovery kit and GAME-FILES-PERSISTENCE.md retain restore details.
GitHub source-snapshot artifacts expire after 30 days; Library backups, not a
promise of permanent scratch, are the durable recovery path.

code.bin SHA-256: 5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
AOT archive SHA-256: 2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9.
Historical CCI SHA-256: 3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525.

Verify CHECKPOINT-MANIFEST.json; restore source/logs beneath the root above and
unpack AOT there (creates generated2/). Put code.bin in restored/. SOURCE-INDEX.json
preserves exact Git paths/blob hashes/modes, including tracked ignored logs.
Publish using the current remote parent and force=false. Never force-push the
local snapshot history. Connector reads/writes worked; no user-PC development.

MANDATE: work in tested checkpoints, push source/reports before ending, keep game
bytes/private AOT/binaries/test state out of public Git, update this canonical
handoff and POST A DOWNLOADABLE COPY EVERY WORK TURN plus a durable source/log
backup. This file must suffice to restore, build, reproduce and continue in a new
chat. Inspect Library backups before asking the user for another upload.

## Hosted CI confirmation

GitHub Actions `37308386943` on implementation `4783f22` completed successfully
for both GCC and Clang jobs. These hosted tests are ROM-free, not game-data runs.
