# LEGO Chase Begins — canonical recovery handoff

Updated October 5, 2026. Read this first. Continue in scratch, not the user's PC
or Work. Project: LEGO City Undercover: The Chase Begins, Nintendo 3DS USA.
This is NOT LEGO Batman. Historical Recovery F/J gameplay is not current proof.
Current executable: headless native startup reconstruction, NOT a playable port.

## Published implementation and current boundary

Implementation: `2b36a39691da2f28fc64a14eac4c3c10ba17ddaa`.
Tested/uploaded implementation tree: `2156886803df7d81759bf7b01acdcd17f0e7aa72`.
Base main: `acba0105c8d4a7fd940b39d1dbd331ac985337f9`.
Restored baseline tree matched `1459853ecdbb37ca3ca635a011fade33f963e45e` exactly;
all 452 prior checkpoint-manifest files verified. The prior OpenFileDirectly stop
was reproduced before edits. Local Git is a snapshot/index, not remote history.
The attached handoff's final receipt records the delivery commit, CI and backup.

NEW empty shared archive + explicit `--ptm-step-mode empty` + verified `--romfs`
now executes the original SelfNCCH open and seven genuine RomFS metadata reads.
The fresh run's next untouched request is:

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=90
r0=0x00048015 r1=0x08630000 r14=0x00131270
last_ipc_session=fs:USER requested_service= request_header=0x08630000
ipc_words=08630000 00000000 00001180 0001180c 08000560 00000000 00000000 00000000
```

This is FS_USER GetPriority; no response has been invented. Kernel time is zero.
The seven reads total 4892 bytes: offset/length 0/40 three times, then 40/12,
52/68, 120/212 and 332/4480. Every returned byte matches the original RomFS.
These are filesystem header, directory and file tables, NOT asset rendering.
The logging-only trace records per-read hashes independently checked against input;
ordinary production GCC/Clang runs reach the same stop with unchanged IPC code.

Keep branches separate:
- Fresh + empty PTM + RomFS: original gamecoin initialization, metadata reads, round 90.
- Existing gamecoin + RomFS: same GetPriority stop at round 82; initialization skipped.
- No RomFS option: previous SelfNCCH OpenFileDirectly stop at round 79.
- Fresh without empty PTM: previous GetTotalStepCount stop at round 71.

The guest still writes/closes its own 20-byte gamecoin.dat. Default-clock SHA-256:
`970a8b30f57b772c2c1c5686e634b9e4ab7055b43caec90e64076f81ed08b4b6`.
No externally seeded coin balance or game data. The existing-file branch does not
prove gamecoin readback or gameplay save/load. No main menu, renderer, audio,
controls, completed initializer count or gameplay is established. Rounds are not frames.

## Original RomFS recovered, verified and durably prepared

All six original archive parts were restored from private Library. The extracted
CCI has 1073741824 bytes and matches the historical SHA below. The unchanged
prepare_game.py reproduced code.bin exactly and extracted raw RomFS from CCI
absolute offset 1851392. No decryption keys, downloaded replacement or patch used.

Raw RomFS: 769179648 bytes, SHA-256
`6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf`.
Native view offset: 4096; view bytes: 769175552. This follows pinned
NCCHContainer::ReadRomFS, including trailing integrity tables. Do NOT expose the
IVFC prefix as the filesystem header or truncate to level-3's 763161254 data bytes.
All three IVFC levels pass: 12 + 1456 + 186319 = 187787 full hash-checked blocks.
Complete raw SHA and native streaming SHA also match independently.

NEW durable Library folder: `/LEGO-Chase-Recovery/Prepared-RomFS/`.
It holds the EXTRACTED raw image, not the original 7z archive, split only to keep
individual Library files below the upload limit:

- LEGO-Chase-USA-romfs.bin.part001: offset 0, bytes 402653184,
  SHA `9a9786af98980cef7215c805f02ee154028aa87b21947c7846351cd575ede8f5`.
- LEGO-Chase-USA-romfs.bin.part002: offset 402653184, bytes 366526464,
  SHA `9e67e1750787b73f48a5f28247b8fe8bd3bc43b9724350b8e57621fa7e77b5ad`.
- romfs-parts.json, restore_romfs_parts.py, verify_romfs.py and profile.json.

Both uploaded parts were materialized again, reassembled and compared byte-for-byte
to game/prepared-romfs/romfs.bin. Receipt: romfs-checkpoint/romfs-persistence-receipt.json.
Use these prepared parts after reset; no repeated CCI extraction is needed.
Original six parts remain in Game-archive/ as an independent recovery route.

## Exact supported RomFS request and implementation policy

Observed SelfNCCH OpenFileDirectly request:

```text
08030204 00000000 00000003 00000001 00000001 00000002 0000000c
00000001 00000000 00004802 0ffff5e8 00030002 0ffff628
```

Archive 3, Empty type 1/one byte with descriptor 0x4802 (ID 2), binary type 2/
12 bytes with descriptor 0x30002 (ID 0), open mode 1, attributes 0. Binary path
is three zero u32s, selecting the original RomFS. The Empty byte is incidental
0xE8, NOT a filename or required NUL. Entire spans/descriptors and writable response
are validated before creating a real moved file-session handle.

RomfsFileService is a separate READ-ONLY endpoint, never the writable extdata backend.
The launcher requires the exact full raw size/SHA and prints provenance. No RomFS
option leaves the request unsupported. Missing/wrong input is a CLI error, not a
blank replacement. The image pins an O_RDONLY/O_NOFOLLOW descriptor after checking
regular-file identity; reads do not re-resolve a guest-provided host pathname.

Read accepts exact header 0x080200C2, write-only mapped output descriptor and a
bounded length. Reply 0x08020082 contains Result, actual count and original mapped
descriptor/address. Full declared output is preflighted, including a clipped EOF
tail; response aliases stop. Zero-length requires no destination dereference.
The host stages bytes before guest writes, which invalidate exclusive reservations.
Short/error host reads never substitute zero data or commit a fake success.

The 1 MiB read bound, at/beyond-EOF zero result and strict alias/span policy are
host safety choices, NOT proven firmware limits. General mapped-buffer translation,
cross-region buffers, cache/async behavior and FS timing remain unimplemented.
No clock advance, event signal, forced wakeup or Break bypass was added.

GetSize returns captured u64 view size and is component-tested, but was NOT observed
in this real startup sequence. A real Close was observed after the first Read.
Pinned IVFCFile::Close returns false without releasing its shared reader, and the
outer File::Close ignores that boolean and replies success. This narrow HLE policy
is retained, including repeat Close/reads through remaining references; it is not
writable extdata Close or proven hardware behavior. Kernel handles and module,
launcher and session image ownership remain separate; final ownership closes the fd.

Write, SetSize, Flush, other file commands, ExeFS/update RomFS and other archive
paths remain explicit stops. Extdata Read and gameplay saves are still open work.
Size/mtime checks detect ordinary external changes around actual reads; descriptor
pinning prevents path replacement redirect, but not malicious host administration.
POSIX implementation only; no Windows or macOS build was performed.

Changed: CMake files; main and runner wiring; new sha256_stream.h; FS_USER wiring;
new fs_romfs_service.h/.cpp, two tests, and Python verify/restore helpers.
No original code, private AOT page, vendor implementation, kernel, IPC router,
extdata backend, PTM, APT, NDM or CFG behavior was changed.

## Validation and evidence

Full unchanged 599-page GCC baseline build then changed-source rebuild/relink;
fresh full 599-page Clang build. Final GCC 19/19 CTest, Clang 19/19 CTest and
Clang ASan/UBSan 19/19 ROM-free suites passed, leaks/halt-on-error enabled.
Eleven scenarios have byte-identical compiler logs: fresh, existing, mode-off,
alternate RTC, no RomFS, no root, missing archive, invalid root, invalid mode,
missing RomFS and wrong RomFS size. Gamecoin behavior and older negative paths stay.

New tests exercise exact replies and guards, input identity, missing/corrupt images,
permissions/pointers/response aliases, output reservations, EOF, shared lifetime,
unsupported modifications, external truncation, pathname replacement, handle
exhaustion, streaming SHA golden vectors and padding/chunk boundaries.
The original image, code and all 603 regular AOT archive files remain unchanged;
599 are C++ pages. Registry 111043 blocks / 545111 words is STATIC verification,
not executed instructions, frames or gameplay completion.

Public: reports/recovery-host/ROMFS-READ.md, ROMFS-FRESH-GCC.txt,
ROMFS-EXISTING-GCC.txt, ROMFS-INTEGRITY.json and ROMFS-READ-PROOF.json.
Private full logs/scripts: romfs-checkpoint/. trace_romfs.py adds logging only to
an alternate IPC translation unit; the production IPC is unchanged. The private
trace binary/object are not part of public source or source/log backup.

Intermediate failures are retained: first OpenFileDirectly descriptor IDs were
wrong and were corrected from the original trace; first test enum name was fixed.
The archive extractor rejected ancillary Vimm's Lair.txt after extracting the CCI
and exited 4. Independent exact CCI SHA and successful preparation prove the game
input; this is not reported as a clean full archive extraction. All final native
builds/tests passed. Complete source tree equality was checked before publication.

## Next exact work

Implement observed FS_USER GetPriority header 0x08630000 using existing shared
priority state, after inspecting pinned fs_user.cpp and the original caller.
Do not guess a default: the reference has a special UINT32_MAX/uninitialized
priority branch that must be read in full. Current FS SetPriority already stores
module state, but GetPriority has NOT been implemented.

Then rerun unchanged game code on a NEW empty private shared archive with empty
PTM and verified RomFS selected. Observe the next request, retain the seven actual
metadata reads, and preserve separate fresh/existing regression cases. Do not jump
to assumed asset payloads, graphics or invented save success.

Primary reference pin: azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
Inspected this turn: core/file_sys/ncch_container.cpp, ivfc_archive.cpp/.h,
romfs_reader.cpp; prior archive_selfncch.cpp and core/hle/service/fs/fs_user.cpp,
file.cpp and IPC definitions. Source-root prefix is src/. A current-branch code
search located GetPriority, but its pinned full body remains next-task work.

## Working paths and commands

Root: /mnt/data/lego_recovery/. Source: repo/. Private pages: generated2/.
Code: restored/code.bin. Raw RomFS: game/prepared-romfs/romfs.bin.
CCI: game/image/LEGO-Chase-USA.cci. Original archive parts: game/archive/.
Builds: build-gcc/, build-clang/, build-asan/. Current logs: romfs-checkpoint/.
Prior logs: writefile-checkpoint/, openfile-checkpoint/, ptm-checkpoint/,
createfile-checkpoint/. Native large-input source is private, not public Git.

NEW test state: private-state/romfs-validation.jou_fe1k/, romfs-trace.n17x1up1/,
and this turn's romfs-baseline/open/read/close roots. They are guest-created test
state, NOT recovered NAND. Historical roots were preserved. Matrix resets only its
own exact expected files to compare compilers at identical paths.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
mkdir -p private-state
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/private-state/priority-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty --romfs game/prepared-romfs/romfs.bin
```

Expected diagnostic stop: GetPriority 0x08630000, round 90, exit 3. For Clang use
clang++; omit LEGO_AOT_DIR for ROM-free tests. Full sanitizer commands/flags are
in romfs-checkpoint/build_remaining.py. No promised gameplay or successful boot.

## Reset recovery and every-turn mandate

GitHub: GTTeancum/Lego-Undercover-3DS-Recomp, main.
Private Library: /LEGO-Chase-Recovery/.
Restore latest source/evidence checkpoint + code.bin + the unchanged private
LEGO-Chase-current-AOT-599pages-2026-10-03.tgz. Verify CHECKPOINT-MANIFEST.json,
restore repo/ and logs under the root above, put code.bin in restored/, and unpack
AOT there (creates generated2/). SOURCE-INDEX.json retains exact tracked paths,
blob hashes and modes, including tracked ignored evidence logs.

For RomFS, list Prepared-RomFS, materialize both raw parts and romfs-parts.json
into one directory (for example game/romfs-persistence), then run:

```sh
mkdir -p game/prepared-romfs
python repo/tools/restore_romfs_parts.py game/romfs-persistence/romfs-parts.json game/prepared-romfs/romfs.bin
python repo/tools/verify_romfs.py game/prepared-romfs/romfs.bin
```

Restore refuses an existing output; verify an already-present file rather than
clobbering it. Source/log backup deliberately excludes the large prepared parts,
which have their own independently verified Library snapshots. Six original ROM
parts in Game-archive/ plus prior recovery kit remain an alternate recovery path.

Code SHA-256: 5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
AOT archive SHA-256: 2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9.
CCI SHA-256: 3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525.

Scratch can reset. Prepared Library inputs, Library source/log backups and GitHub
are durable recovery routes; source-snapshot Actions artifacts expire after 30 days.
Never force-push local snapshot history. Publish through connector using verified
current remote parents and force=false. No user-PC/Work development was used.

MANDATE: tested checkpoints, push source/reports before ending, keep game bytes,
private AOT/binaries/test state OUT of public Git, update this canonical handoff
and POST A DOWNLOADABLE COPY EVERY WORK TURN plus a durable source/log checkpoint.
This file must suffice for a new chat to restore, build, reproduce the real stop
and continue without guesses or unnecessary user reuploads.

## Hosted CI confirmation

GitHub Actions `37312503207` on implementation `2b36a39` completed successfully
for GCC and Clang. Hosted tests are ROM-free, not original-game execution.
