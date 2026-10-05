# LEGO Chase Begins — canonical recovery handoff

Read this first. Continue in scratch, not on the user's PC or through Work.
Historical Recovery F/J gameplay is not current proof. This is a headless native
startup reconstruction, not a playable release.

## Published work and recovered pending checkpoint

The previously pending filesystem implementation is now published as
ed036aae5499ce39b58328071356a7511fee1b24, based on remote
bbc48d52afce5cded88907967a678988f04a349c. The uploaded tree matched the prior
verified implementation tree e687a42e1e07c474e255aa2724ae05e6733ac918 exactly.
GitHub Actions 37262006851 passed. No local snapshot history was force-pushed.
The complete pending snapshot was restored before editing, and its cfg:u missing
service baseline was reproduced byte-for-byte with a fresh 599-page GCC build.

Current source commit: 5df8d42164d26e6e4c49b5965fa84ff11161904d.
Tested/uploaded source tree: be320b9cc6e51a97dd378b47317d545f744268ff.
GitHub Actions 37262887897 succeeded on this implementation. The delivery commit
adds reports and this handoff; consult the attached copy for its final SHA.

GitHub write actions are available in this turn. The update_ref wrapper rejected
its advertised expected_sha argument at binding, but the documented branch_name,
sha and force=false call succeeded. This is not a permissions failure. Never use
force to push the local snapshot as full remote history.

## Observed sequence and actual current boundary

Registering cfg:u did NOT expose a configuration query. The game connects to it,
then immediately requests FS OpenArchive. cfg:u is therefore only a discoverable
endpoint: every command remains an explicit UnsupportedIpc stop. No language,
region, hardware identifier, country or other configuration value is invented.

Observed FS OpenArchive:
- Header 0x080C00C2, archive type 7 (SharedExtSaveData), binary path type 2.
- Exactly 12 input bytes: 00000000 0B0000F0 00000000.
- Little-endian fields: media 0, low ID F000000B, caller high ID 0.
- Static descriptor 0x00030002, input address 0x0FFFF610.

A bounded host-backed archive mount is now implemented. It requires an explicit
existing root selected with --shared-extdata-root. The actual mapped directory is
ROOT/00048000/F000000B/user. Pinned Azahar replaces the high ID with 0x48000 and
ignores the media field for this shared archive. The implementation follows that
HLE path policy; this is not a raw NAND container or full FS-format implementation.

For the real-game test only, an empty private host directory was explicitly
provisioned. It is NEW test state, not recovered console data. OpenArchive itself
does not create directories or seed any files. With that directory present:

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=69
last_ipc_session=fs:USER request_header=0x08080202
next command=CreateFile path=/gamecoin.dat requested size=20 bytes
```

The path is UTF-16LE, 28 bytes including NUL, at 0x0036A046, with static descriptor
0x00070002. The file does NOT exist and the CreateFile request remains unsupported.
No file read/write, saved data, Play Coin balance, title screen or gameplay is
claimed. No game instruction or generated-page byte was changed.

Without --shared-extdata-root, startup stops at the untouched OpenArchive request.
An existing root lacking the requested archive returns reference-HLE NotFormatted
0xC8A04554 and the original game then calls Break at 0x0011FB80 (round 70).
An invalid/nonexistent root is rejected by the CLI with exit 2 without creating it.
These cases must not be collapsed into an artificial successful mount.

## Implementation and safety boundaries

src/services/fs_shared_archive.* provides a module-shared u64 archive table,
separate from kernel handles. Opening an existing directory assigns a distinct
mount ID; sessions share the table. The host canonicalizes its selected root,
uses only numeric components, rejects symlinks beneath the root and refuses root
rebasing. It never writes a game file. Bad/missing directory outcomes are explicit.
Symlink rejection and host error guards are containment policy, not exact hardware
error parity. An open directory descriptor is not retained yet: future file writes
must revalidate and use race-resistant, contained opens rather than trusting a
previously checked path if host files can change concurrently.

FS requires initialization, exact OpenArchive header/type/12-byte size/descriptor,
and a configured root. It validates the complete readable input and arithmetic
before reading. Existing IPC response-writability preflight runs before mounting.
Other archive kinds, malformed requests and all file/close operations stay stops.
Archive IDs are not session/kernel handles and consume no HandleTable slot.

Inherited FS startup behavior remains: per-connection kernel-caller/program binding,
non-spoofable CallingPid, shared priority and SDK diagnostics. APT and NDM behavior
is unchanged. Kernel time remains zero in these real runs. The fixed RTC policy,
read-only shared clock, documented local vendor DMB routing patch and incomplete
scheduler are retained; none is represented as finished hardware emulation.

## Validation

- GCC: full unchanged 599-page baseline build, changed-source rebuild/relink,
  12/12 CTest suites passed.
- Clang: fresh full 599-page build and 12/12 CTest suites passed.
- Clang ASan/UBSan: 12/12 ROM-free suites passed, leak checking/halt-on-error enabled.
- GCC and Clang real startup logs match exactly with the same explicit test root.
- Hosted CI passed both compiler jobs for both published implementation commits.
- Tests cover absent/configured roots, missing mounts, high-ID correction, shared
  u64 mount identity, request guards, short/wrapping input pointers, protected
  responses, root reconfiguration, symlink containment and unsupported CFG calls.
- All 603 regular AOT archive members remain byte-identical; 599 are C++ pages.
  545111 raw words and 111043 registry blocks are static verification counts,
  not executed instructions, frames or gameplay completion.
- See reports/recovery-host/FS-SHARED-ARCHIVE.json and CFG-*.txt for fresh logs,
  original path inspection, negative runs, tests, source and binary fingerprints.
  FS-STARTUP.json and FS-*.txt retain the recovered earlier checkpoint evidence.

## Next exact work

Restore the observed FS CreateFile request for /gamecoin.dat (20 bytes), using
the explicit private shared archive root. Inspect pinned CreateFile and extdata
fixed-size semantics first. Validate the actual guest UTF-16 path and avoid host
path traversal or symlink races. Preserve existing files; do not overwrite them
or synthesize Play Coin content. Let the real guest create/initialize its own file
through implemented operations. Then rerun unchanged game code to find the next
request. cfg:u configuration commands have not been exercised and need no values
yet. Do not jump ahead to guesses for saves, RomFS, graphics or gameplay.

Reference source pin: azahar-emu/azahar
86a9f9236ae42bb5a2b995dbc933d599d8ea07ac:
src/core/hle/service/fs/fs_user.cpp (OpenArchive/CreateFile),
src/core/hle/service/fs/archive.h,
src/core/file_sys/archive_extsavedata.cpp (corrected ID/path and Open),
src/core/file_sys/errors.h. References are also in the checkpoint JSON.

## Working files and reproducible commands

Scratch root: /mnt/data/lego_recovery/.
Source: repo/. Private pages: generated2/. Verified code: restored/code.bin.
Builds: build-gcc/, build-clang/, build-asan/. Current logs: cfg-checkpoint/.
Earlier pending FS logs/patch/metadata remain in fs-checkpoint/ and the root.
The private inspectors in cfg-checkpoint/ read request paths from the executed
process; they are not production overrides or replacements for game execution.

Explicit empty test archive:
/mnt/data/lego_recovery/private-state/shared-extdata/00048000/F000000B/user
The directory exists and is empty at handoff. Do not call this a recovered NAND
save. The attached checkpoint preserves the empty directory structure.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root /mnt/data/lego_recovery/private-state/shared-extdata
```

Use clang++ for Clang; omit LEGO_AOT_DIR for ROM-free builds. Other CLI limits are
--block-limit, --host-event-limit and --rtc-ms-since-1900. No Windows build claimed.

## Durable recovery and mandate

Private Library /LEGO-Chase-Recovery/ holds all six original ROM archive parts
under Game-archive/, code.bin, the unchanged 599-page archive and source checkpoints.
This turn restored the attached FS-STARTUP-PENDING snapshot and already-present
private code/pages without a new upload. Six ROM parts remain in /mnt/data/ and
backup-verify/. Scratch can reset; GitHub source, Library backups and the attached
source/log checkpoint are the recovery paths, not a promise of permanent scratch.

Code SHA-256: 5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
CCI SHA-256: 3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525.

Use moderate tested checkpoints. Push source/reports, keep original game bytes and
derived pages private, update this canonical handoff and attach a downloadable
copy every work turn. Inspect backups before requesting uploads. Local Git is a
snapshot/index, not full remote history; never force-push it.
