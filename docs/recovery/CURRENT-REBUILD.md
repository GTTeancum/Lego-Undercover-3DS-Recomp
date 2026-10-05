# LEGO Chase Begins — canonical recovery handoff

Updated October 5, 2026. Read this first. Continue in scratch, NOT on the user's
PC or Work. Project: LEGO City Undercover: The Chase Begins (Nintendo 3DS USA),
not LEGO Batman. Historical Recovery F/J gameplay is not current proof. The
current native executable is a headless startup reconstruction, not playable.

## Published implementation and restored baseline

Implementation: `a9d63b67bafbbc2687aded4af13cb0a302366dfc`.
Tested/uploaded implementation tree: `6605a090a5d701d54bd1554f46eb4bef67fe9221`.
Base main: `d6317efabe8e142bf99243285fc5c86e072c53a9`.
The restored baseline exactly matched tree
`200fa465a7008be39faeac5bbc6bb357d11d116b`. All 627 prior manifest files and 249
indexed source files verified. The former registration stop was reproduced before
editing; the only normalized log difference was the newly owned test-root path.

Source came from the attached d6317ef checkpoint. Private code/AOT and prepared
RomFS parts were already available; no user upload, original-game re-extraction,
Work or PC access was needed. Local Git is a snapshot/index, not remote history.
The final receipt appended to the downloadable handoff records delivery SHA,
hosted CI and the source/log archive. Never force-push local snapshot history.

## Actual progress and current stopping point

The original game now registers its GSP event and maps the real shared page:

```text
RegisterInterruptRelayQueue request: 00130042 00000001 00000000 0007001c
reply:                              00130082 00002a07 00000000 00000000 0007801d
MapMemoryBlock: handle=0007801d address=10000000 permissions=3 other=10000000
MapMemoryBlock result: 00000000
```

Slot 0 is allocated to the actual GSP connection. The copied event is retained,
not closed, reset or signaled. The returned handle owns the same 4096-byte shared
object used by the service. Mapping makes that backing readable/writable at
0x10000000, not a separate copy. The page is still zero and the event UNSIGNALED
at the next stop. Kernel time remains zero. No interrupt is manufactured.

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=98
last_ipc_session=gsp::Gpu requested_service= request_header=0x00010082
ipc_words=00010082 00401000 00000004 00010002 0ffff608 00000000 00000000 00000000
```

This is WriteHWRegs: relative register offset 0x00401000, length 4, static input
descriptor 0x00010002, source address 0x0FFFF608. The actual source word is zero.
The request remains untouched and unsupported. Its register's precise meaning
has NOT been implemented or inferred from the zero value. No hardware register
write, GPU command execution, framebuffer, vblank, rendered frame or main menu
is established. Audio, controls, initializer completion and gameplay remain open.
Dispatch rounds are not frames or a completion percentage.

Keep branches separate: fresh + empty PTM + verified RomFS reaches this request
at round 98; existing gamecoin reaches it at round 90, skipping initialization.
No RomFS retains OpenFileDirectly round 79. Fresh without explicit empty PTM
retains GetTotalStepCount round 71. Existing-file startup is not save readback.

The original game still writes/closes its own 20-byte gamecoin.dat. Default-clock
SHA-256: 970a8b30f57b772c2c1c5686e634b9e4ab7055b43caec90e64076f81ed08b4b6.
Seven RomFS metadata reads still total 4892 bytes and match the original image:
0/40 three times, then 40/12, 52/68, 120/212, 332/4480. These are filesystem tables,
not rendered assets. No console history, coin balance or save data was pre-seeded.

## Registration and connection ownership

GSP allocates one of four numeric relay slots per connection, first-free, following
pinned SessionData policy. These are service slot IDs, NOT newly created guest
threads. Duplicate kernel handles retain the same identity. Final session-reference
destruction frees its slot, copied event and held GPU ownership. A full slot table
returns MaxConnectionsReached 0xD0401834. Failed kernel handle insertion releases
the tentative slot, and cannot fall back to the service-registration endpoint.

Registration accepts the exact header, copy descriptor and supported flags 0/1,
and resolves an actual EventObject. Response writability and output-handle
allocation precede registration mutation. First successful registration returns
0x00002A07 (description 519, GX module 10, success summary/level), even if it is on
a nonzero slot. Subsequent registrations return zero. Re-registration preserves
shared bytes and event signal state; it may replace the retained event/flags.
Flag 1 is retained, not an implementation of VRAM backup behavior.

The IPC session factory now returns an explicit Result and optional output handler.
A successful null handler retains existing module-shared services; failure is
propagated. FS_USER received only the mechanical signature adaptation. Its file
and priority behavior is unchanged. The production IPC router DID change this turn.

## Shared mapping scope and explicit limits

ServiceSharedMemoryObject owns a fixed zero-initialized 4 KiB byte array plus
backing-relative reservation epochs. GuestMemory retains this object in mappings.
Aliases and service writes access the same bytes and invalidate the same 8-byte
exclusive granule; unrelated granules remain independent. Mapping ownership keeps
bytes alive after handles close. Private, nonshared memory retains its prior path.

The supported SVC MapMemoryBlock slice checks actual shared-object type, permission
subsets, DontCare peer permissions, address arithmetic and existing regions.
Explicit addresses must be page-aligned and >= 0x08000000, with end strictly below
0x14000000, preserving the pinned reference's exclusive end check. Range/overlap
errors use the inspected InvalidAddress/InvalidAddressState results. Address-zero
placement and unaligned requests remain host stops rather than guessed results.

This is NOT a physical BASE-region allocator, memory-budget accounting, general
shared-object implementation, guest CreateMemoryBlock or UnmapMemoryBlock. Mapping
lifetime currently ends with the memory model; explicit unmapping is not implemented.
Cross-region accesses, full mapped-buffer translation, cache effects and asynchronous
service/scheduler timing remain incomplete. No thread wakes or time advances were
added. Register/queue layout consumers, interrupts and command processing still need
implementation. Unregister, contended/repeated acquisition, TryAcquireRight and
ReleaseRight remain unsupported as before. No Windows/macOS build was performed.

Changed source: CMakeLists.txt; ctr_ipc.*, ctr_memory.*, new ctr_shared_memory.h,
ctr_svc_bridge.*, gsp_gpu_service.h, FS_USER factory signatures; new relay/map tests
and updated obsolete acquire-test expectations. Original code, private AOT, vendor,
kernel scheduler, extdata backend, RomFS backend, PTM, APT, NDM and CFG are unchanged.

## Validation and evidence

- Full GCC baseline and Clang native builds link all 599 unchanged AOT page units.
- Final GCC 23/23 CTest, Clang 23/23 and Clang ASan/UBSan 23/23 ROM-free suites pass.
  Leak checking and halt-on-error enabled; final driver reports zero for every step.
- Eleven startup scenarios have byte-identical compiler logs: fresh, existing,
  PTM-off, alternate RTC, no RomFS, no root, missing archive, invalid root, invalid
  PTM mode, missing RomFS and wrong RomFS size.
- Tests cover first/nonzero-slot registration, repeats, copied event/page lifetime,
  duplicate sessions, four-slot exhaustion, failed allocation rollback, malformed
  input/protected responses, actual shared mappings, alias coherence, permissions,
  overlap/range guards, final mapping ownership and shared exclusive reservations.
- Logging-only alternate IPC/SVC objects verify the actual registration reply,
  backing identity, successful mapping, zero page, unsignaled event and pending
  WriteHWRegs input. Ordinary production builds independently reach the same stop.
- All 603 regular AOT backup members remain byte-identical; 599 are C++ pages.
  Registry counts 111043 blocks / 545111 words are STATIC verification inventory.
  Raw RomFS SHA and all three IVFC levels (187787 blocks) verify again.

Current full evidence/scripts: relay-checkpoint/. Public report and logs:
reports/recovery-host/GSP-RELAY-MAP.md, RELAY-FRESH-GCC.txt, RELAY-EXISTING-GCC.txt,
RELAY-PROOF.json and RELAY-VALIDATION.json. Final proof is read-and-relay-proof.json;
final tests are ctest-gcc/clang/asan.txt and final-finished.json. Trace binaries and
objects are excluded from the source/log backup. The initial restore helper assumed
an incorrect manifest shape; it was corrected before extraction/verification. No
native test failure was suppressed, and all final build/test steps passed.

## Next exact work

Implement the observed GSP WriteHWRegs request at relative offset 0x00401000,
length 4, source word zero. Read the pinned GSP WriteHWRegs parser/helper and GPU
register routing/definitions first, then inspect the original caller and actual
subsequent requests. Model the correct register state/side effects, not arbitrary
success or an invented GPU completion. Validate static input descriptor, full span,
range/alignment and response writability before mutation. Do not modify the original
executable or private AOT to bypass setup. Preserve shared page/event ownership.

Rerun the original code on a NEW empty test root with explicit empty PTM and verified
RomFS. Observe the next real operation; do not assume rendering, interrupts or
vblank follow immediately. Keep the fresh/existing branches and all earlier negative
input scenarios separate. Never seed state to skip the original startup path.

Primary reference pin: azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
Inspected kernel/shared_memory.cpp (67f93be612954a8e89240c44c82839ff841282e1),
kernel/errors.h (56ee1ab9a46c7244b6b184ca29bf081cae02e88d), kernel/svc.cpp MapMemoryBlock
(14d0998548c99ca4e2484f465263deb99066cdab, lines 590-785), core/memory.h virtual
ranges (f7045c1716c26a32c9764eebdc6a7fe6f7e3fe2f, lines 190-285), and GSP session
layout in gsp_gpu.h. Source paths use src/core/hle/ for kernel/service files.
Prior pinned gsp_gpu.cpp (6f915e4d6a5d27853321d0a233103afb9d877bc0) contains the
registration, constructor, first-free slot and disconnect policies used here.

## Scratch paths and reproduction

Root: /mnt/data/lego_recovery/. Source: repo/. Private AOT: generated2/.
Code: restored/code.bin. Raw RomFS: game/prepared-romfs/romfs.bin.
Prepared parts: romfs-library-roundtrip/. Builds: build-gcc/, build-clang/, build-asan/.
Current evidence: relay-checkpoint/. Prior: priority-checkpoint/, romfs-checkpoint/,
writefile-checkpoint/, openfile-checkpoint/, ptm-checkpoint/, createfile-checkpoint/.

NEW final state: private-state/relay-validation.7hovbu58/ and relay-trace.iap08751/.
Earlier relay-baseline/register/map/trace/validation roots are this turn's NEW test
state too. Historical restored roots were preserved. These are not recovered NAND
saves. The paired validator resets only its own exact byte-verified newly created
files; never delete or reuse unknown console state for fresh tests.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
mkdir -p private-state
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/private-state/gsp-reg-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty --romfs game/prepared-romfs/romfs.bin
```

Expected: WriteHWRegs 0x00010082, round 98, diagnostic exit 3. Use clang++ for Clang;
omit LEGO_AOT_DIR for ROM-free builds. Sanitizer flags/commands are in
relay-checkpoint/build_remaining.py; final_validate.py records all final rebuilds.

## Durable recovery and every-turn mandate

GitHub: GTTeancum/Lego-Undercover-3DS-Recomp, main.
Private Library: /LEGO-Chase-Recovery/. Restore latest source/log checkpoint and
handoff, code.bin and LEGO-Chase-current-AOT-599pages-2026-10-03.tgz. Verify
CHECKPOINT-MANIFEST.json before editing. Unpack source/logs beneath the root above,
put code.bin in restored/, and unpack AOT there (creates generated2/).
SOURCE-INDEX.json retains exact tracked Git paths/blob hashes/modes, including
ignored-but-tracked logs. Local Git is not full remote history.

RomFS is already durably EXTRACTED under Prepared-RomFS/. Restore its two raw
parts (402653184 and 366526464 bytes) plus romfs-parts.json, rather than re-extracting
CCI. Restore refuses an existing output; verify rather than clobbering one.

```sh
mkdir -p game/prepared-romfs
python repo/tools/restore_romfs_parts.py game/romfs-persistence/romfs-parts.json game/prepared-romfs/romfs.bin
python repo/tools/verify_romfs.py game/prepared-romfs/romfs.bin
```

Raw RomFS: 769179648 bytes, SHA-256
6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf.
Native view offset 4096, size 769175552: do NOT include the IVFC prefix as the
filesystem header or truncate away trailing integrity tables.
code.bin SHA: 5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
AOT archive SHA: 2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9.
Historical CCI SHA: 3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525.
Six original parts remain in Game-archive/ as an independent recovery route.

Scratch may reset. Library prepared inputs/source checkpoints and GitHub are the
recovery paths; Actions source snapshots expire after 30 days. Connector access
works; direct container GitHub DNS failed. No user-PC or Work development occurred.

MANDATE: work in tested checkpoints, push source/reports before ending, keep game
bytes/private AOT/binaries/test state OUT of public Git, update this canonical
handoff and POST A DOWNLOADABLE COPY EVERY WORK TURN plus a durable source/log
backup. This file must enable a new chat to restore, build, reproduce the actual
stop and continue without guessing or unnecessary reuploads. Publish with verified
current remote parents and force=false, never force-push local snapshot history.

## Hosted CI confirmation

GitHub Actions `37323609481` on implementation `a9d63b6` completed successfully
for both GCC and Clang jobs. Hosted tests are ROM-free, not original-game execution.
