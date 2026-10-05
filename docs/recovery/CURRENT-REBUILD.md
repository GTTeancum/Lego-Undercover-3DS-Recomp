# LEGO Chase Begins — canonical recovery handoff

Read this first. Continue in scratch, not on the user's PC or through Work.
This is the native 3DS reconstruction of LEGO City Undercover: The Chase Begins
(USA), not LEGO Batman Wii/Xbox. Historical Recovery F/J gameplay is not current
proof. The recovered build remains headless startup reconstruction, not playable.

## Published checkpoint

Implementation: `654a8a2c5bcb81164e98589dd79ae6b6cf6d1102`.
Complete tested/uploaded implementation tree: `ad137053f44d5377cb6c110ffd4b7d9f6bd934ed`.
GitHub Actions run `37266660459`: GCC and Clang jobs both completed successfully.
Those hosted tests are ROM-free, distinct from the full local game-code runs.
The delivery commit adds only reports and this handoff; its SHA is recorded in
the downloadable handoff after publication.

Starting remote was `381044494ab0f8746a811bbb5ee7bf96a98b407c`. Scratch was empty.
Private Library restored code.bin and the 599-page AOT archive without user upload.
The latest source was recovered from GitHub Actions after publishing
`928c148310fa75e6a63461b7e24703e1db787247`, which makes source-only snapshots run
on EVERY main push (30-day artifact retention). Its snapshot run `37265315359`
succeeded, artifact `11325818314` was downloaded and its checksum and full source
tree `fa69221ccaefc0ff5aebe696d843e96b680d6578` verified before editing.

GitHub writes work. Use connector Git trees/commits/ref updates with the current
remote parent; never force-push a local snapshot as full history. No user-PC
access was used. Container github.com DNS was unavailable, but the connector and
artifact download worked. Prefer the latest source snapshot or Library checkpoint
instead of manually recovering dozens of individual files next turn.

## Actual current boundary: distinguish fresh and existing files

The original guest's FS CreateFile is now implemented for the supported shared
archive. Exact request at SVC PC 0x0025947C:

```text
08080202 00000000 00000001 00000000 00000004 0000001c
00000000 00000014 00000000 00070002 0036a046
```

Archive handle 1, UTF-16LE path `/gamecoin.dat` (28 bytes including NUL), attributes
0, requested size 20. Reply: header 0x08080040 and Result 0. The NEW test file is
20 zero bytes from ordinary fixed-size creation, not a seeded Play Coin balance,
save header or completed application initialization.

CreateFile initially exposed a genuine failure in the new-file initialization
path: `srv:` returned ServiceNotRegistered (0xD0406401) for `ptm:u`, then the game
called handle 0 with headers 0x000C0000 and 0x000B00C2. Those returned transport
InvalidHandle (0xD8E007F7), followed by original Break at 0x0011FB80, round 73.
A private diagnostic SVC trace proved the sequence; no Break was bypassed.
The generic last-valid-session diagnostic was stale during invalid-handle calls.

`ptm:u` is now a discoverable service endpoint, but ALL its commands remain
explicit unsupported host stops. With a fresh empty test archive, unchanged
game code creates the file and stops at the real next request:

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=71
r0=0x00058017 r14=0x0012b528
last_ipc_session=ptm:u requested_service= request_header=0x000c0000
```

Pinned PTM names 0x000C GetTotalStepCount. No count, history, battery, pedometer,
config or hardware value is invented. cfg:u remains discovery-only too.

A SECOND process using the just-created file gets FileAlreadyExists from
CreateFile and takes a different original branch:

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=71
last_ipc_session=fs:USER requested_service= request_header=0x08030204
ipc_words= 08030204 00000000 00000003 00000001 00000001 00000002 0000000c 00000001
```

That is OpenFileDirectly, NOT ordinary OpenFile. It remains unsupported. The
existing file is unchanged. Do not use this second branch to claim fresh-file
initialization is resolved; use a NEW empty private archive for PTM development.
Neither successful file opening/read/write, completed save, title screen nor
playable gameplay has been reached in this checkpoint.

## Implementation and safety scope

SharedArchiveMounts retains the shared u64 archive table and explicit existing
root policy. Mounting does not create archive directories. The mapped directory
remains ROOT/00048000/F000000B/user. The shared high ID correction is inherited.

POSIX ConfigureRoot pins a directory descriptor; CreateFile reopens numeric archive
components and guest parents relative to held descriptors, never following
symlinks. The leaf uses exclusive creation, then ftruncate to the requested size.
Existing files are never truncated. Concurrent creators return one success and
one FileAlreadyExists. No unsafe fstream fallback is supplied on other hosts:
creation remains unsupported there. No Windows build is claimed.

The handler checks initialization, exact header, UTF-16 type, static descriptor,
full input readability, address wrap, terminal/embedded NUL, surrogate pairs and
path components. Existing response-writability preflight runs before host I/O.
Strict dot/empty/control rejection, a 4096-byte path bound and a 16 MiB create
bound are HOST SAFETY POLICY, not discovered firmware capacity or full path parity.
Transactions, attributes, quotas, NAND container format, file sessions, Read,
Write, SetSize, Close and crash-durable writes remain unimplemented.

Unknown host errors request a router host stop and leave the guest IPC request
and CPU untouched. A failed resize can leave its newly-created zero-length file;
that partial host effect is reported, not silently rolled back through an unsafe
pathname. The RLIMIT_FSIZE=0 regression covers this noexcept-boundary failure.
Root path replacement stays tied to the pinned original directory; this is not
protection against a malicious host administrator changing filesystem contents.

APT/NDM behavior, FS caller/SDK/priority state, fixed RTC policy, read-only shared
clock, vendor DMB routing patch and incomplete scheduling remain inherited.
Kernel time remains zero in all current real runs; no artificial wake or time
advance was introduced to force progress.

## Tests and evidence

- Full 599-page GCC and Clang native builds succeeded; 14/14 CTest suites pass each.
- Clang ASan/UBSan: 14/14 ROM-free suites pass, leak checking/halt-on-error enabled.
- Both hosted compiler jobs passed on implementation 654a8a2.
- GCC/Clang real-run logs match byte-for-byte for fresh, existing, unconfigured,
  missing-archive and invalid-root scenarios.
- New tests cover Unicode, malformed IPC, pointers, protected responses, invalid
  archive IDs, existing-file preservation, symlinks/root replacement, concurrent
  exclusive creation, forced host resize failure and untouched PTM requests.
- All 603 regular AOT backup members match byte-for-byte; 599 are page C++ files.
- 545111 raw words and 111043 registry blocks are STATIC validation inventory,
  not executed instructions, frames, initializers or completed gameplay.

See reports/recovery-host/FS-CREATEFILE.md, CREATE-FRESH-GCC.txt and
CREATE-EXISTING-GCC.txt. Complete local configure/build/test/run logs, the trace
source and validation script are in createfile-checkpoint/ in the private backup.
An early PTM unit-test setup mistakenly connected srv: as a service; it was fixed
to use ConnectToPort before the final passing suites and publication. No failed
intermediate test result is being reported as a passing final result.

## Next exact work

Inspect pinned PTM GetTotalStepCount implementation and the original caller's
initialization flow. Define a documented, justified source of step state without
pretending to recover console history. Implement only supported/observed service
semantics, then rerun unchanged game code against a NEW empty private archive.
The earlier invalid-handle trace also showed GetStepHistory, but no successful
handler execution or output is known yet. Preserve those distinctions.

Keep the existing-file OpenFileDirectly branch as a separate negative/regression
case. Do not pre-seed gamecoin.dat or jump to guessed RomFS/graphics/save behavior.
The next game execution, not assumptions, determines subsequent work.

Reference pin: azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
Inspected: src/core/hle/service/fs/fs_user.cpp; src/core/file_sys/savedata_archive.cpp,
archive_extsavedata.cpp, path_parser.cpp, errors.h; src/core/hle/service/ptm/ptm_u.cpp.
The latter confirms command names only; PTM output policy still requires inspection.

## Scratch paths and reproducible continuation

Root: /mnt/data/lego_recovery/.
Source: repo/. Private AOT: generated2/. Verified executable input: restored/code.bin.
Builds: build-gcc/, build-clang/, build-asan/. Current logs: createfile-checkpoint/.
Initial source snapshot: snapshot-928c148/. Older Library source: source-bbc48d5/.
Downloaded source archive: /mnt/data/LEGO-source-928c148.zip.

All present private-state test roots were newly provisioned in this turn. They
are NOT recovered NAND data. paired-validation/00048000/F000000B/user/gamecoin.dat
contains 20 zero bytes from the guest's creation request. The other diagnostic
roots shared-extdata/, fresh-trace/, trace-confirm/ and ptm-first/ also contain
only newly created test state. Preserve provenance in the private archive.

File SHA-256: de47c9b27eb8d300dbb5f2c353e632c393262cf06340c4fa7f1b40c4cbd36f90.
Do not overwrite, delete or reuse an unknown existing console save for fresh tests.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
# Deliberately NEW empty test archive, not restoration of a console filesystem:
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/private-state/ptm-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT"
```

Use clang++ for Clang; omit LEGO_AOT_DIR for ROM-free builds. Other CLI limits are
--block-limit, --host-event-limit and --rtc-ms-since-1900. The private validate.py
script intentionally refuses to reuse its owned paired-validation directory;
choose new test directories for another matrix rather than deleting unknown data.

## Durable recovery and every-turn mandate

Private Library /LEGO-Chase-Recovery/ holds all six original ROM archive parts
under Game-archive/, code.bin, LEGO-Chase-current-AOT-599pages-2026-10-03.tgz,
restore tools and source checkpoints. This turn restored only code/AOT and source;
the six original parts and large CCI were NOT copied into current scratch because
startup does not yet need them. Inspect Library before asking for another upload.

Code SHA-256: 5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
AOT archive SHA-256: 2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9.
Historical CCI SHA-256: 3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525.

Scratch can reset. GitHub source, Library backups and the attached source/log
checkpoint are durable recovery paths, not a promise of permanent scratch.
The downloadable handoff records the final delivery SHA and backup filename.
Source snapshots have a 30-day retention limit; Library backups remain necessary.

Work in moderate tested checkpoints. Push source and reports before ending.
Keep original game bytes, derived AOT pages and compiled game binaries private.
Update this canonical handoff and POST A DOWNLOADABLE COPY EVERY WORK TURN.
It must be sufficient for a new chat to recover files, rebuild, reproduce the
actual stopping point and continue without guessing or unnecessary reuploads.
