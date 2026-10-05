# LEGO Chase Begins — canonical recovery handoff

Updated October 5, 2026. Read this first. Continue in scratch, NOT on the user's
PC or Work. This is LEGO City Undercover: The Chase Begins (Nintendo 3DS USA),
not LEGO Batman. Historical Recovery F/J gameplay is not current execution proof.
The current native executable is headless startup reconstruction, not playable.

## Published source and restore verification

Implementation: `ad22af5b7e2added0475de84b2963f473a6f2070`.
Tested/uploaded source tree: `270f61e7a946906356c82fa3626f741d0883d001`.
Base main: `36d8534dea59857261268e71d88a7604c197cebf`.
The restored baseline matched tree `e1309d631c377b47207a6a7a7c41eff9e5efe076`.
All 540 prior checkpoint-manifest files and 241 indexed source files verified.
The former real GetPriority stop was reproduced before edits; its log matched
apart from the newly owned test-root pathname. The final receipt in the attached
handoff records delivery SHA, hosted CI and the source/log archive.

The source was recovered from the attached 36d8534 archive, AOT/code from their
already mounted backups, and raw RomFS from the already mounted prepared parts.
No user reupload, game re-extraction or user-PC access was needed. Local Git is a
verified snapshot/index, NOT the full remote history. Publish with current remote
parents via connector and force=false; never force-push snapshot history.

## Actual current boundary: GPU interrupt relay registration

The original FS_USER GetPriority now returns its stored module priority zero:
`08630080 00000000 00000000`. The reference's UINT32_MAX branch merely logs;
no substitute default is used. The original wrapper at 0x00131254 takes IPC word 2.

The next failed lookup was gsp::Gpu. Before discovery was implemented, the guest
sent AcquireRight and RegisterInterruptRelayQueue through handle zero, eventually
stopping at SVC 0x1F / PC 0x00130910 / round 95. This is preserved in the private
before-GSP trace. Do NOT treat that failed-handle path as a valid memory-map request.

Discovery alone exposed AcquireRight at round 92. First uncontended acquisition
is now implemented and exercised by original code:

```
AcquireRight request: 00160042 00000000 00000000 ffff8001
reply:               00160040 00000000
```

The current fresh run's untouched next request is:

```
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=94
last_ipc_session=gsp::Gpu requested_service= request_header=0x00130042
ipc_words= 00130042 00000001 00000000 0007001c 00000000 00000000 00000000 00000000
```

Command: RegisterInterruptRelayQueue. Raw flags 1, copy descriptor 0, input event
handle 0x0007001C. The private trace resolves it to a genuine one-shot EventObject,
currently UNSIGNALED. Ownership is held by the session for process 1/client thread 1.
No registration result, relay-slot ID, shared-memory handle or event signal has
been returned. Rendering, GPU commands and shared-memory mapping are NOT running.

Fresh mode uses a NEW empty archive, explicit `--ptm-step-mode empty`, and the
verified `--romfs` image. Existing-file startup reaches the same request at round
86, skipping initialization. No RomFS option retains the old OpenFileDirectly
stop at round 79. Without explicit empty PTM, startup still stops at step count
round 71. Those paths remain separate; existing-file startup is not save readback.

The game still writes/closes its own 20-byte gamecoin.dat, default-clock SHA-256:
970a8b30f57b772c2c1c5686e634b9e4ab7055b43caec90e64076f81ed08b4b6.
Seven original RomFS metadata reads still total 4892 bytes and match the original
image byte-for-byte: 0/40 three times, then 40/12, 52/68, 120/212 and 332/4480.
These are filesystem tables, not rendered asset payloads. No main menu, graphics,
audio, controls, completed initializer count or gameplay is established.

## Implementation scope and limits

GetPriority accepts only exact 0x08630000 on an initialized FS session and returns
the existing shared u32 unchanged, including 0xFFFFFFFF. It does not change thread
priority or FS worker scheduling. Tests cover repeat queries, all-bit values,
separate/duplicated sessions, reopen/reinitialize, protected responses and a full
kernel handle table without allocations.

New GspGpuService uses a distinct internal identity for each connection and one
weak owning identity per module. Duplicate kernel handles share the same session.
Final session-reference destruction releases ownership; a remaining duplicate
keeps it. The service-registration endpoint itself cannot acquire ownership.
These identities are NOT numeric GSP relay-thread slots. The reference's four-slot
allocation and service-connection limit are NOT reconstructed yet.

Only first uncontended AcquireRight, flag 0, exact header and one copy descriptor
is supported. The copied handle must resolve to the actual current ProcessObject,
not a numeric PID or a same-numbered foreign object. Invalid/wrong-type handles
return transport InvalidHandle; other process objects host-stop. Repeat/contended
acquisition, nonzero flags, TryAcquireRight, ReleaseRight, queues and all remaining
GSP commands stop without modifying the request. This limited support must not be
reported as full GSP ownership/error/wait parity.

The reference's renderer/program-specific setup and shader policies are not ported
by this ownership-only slice. No GPU execution, framebuffer, vblank, interrupt or
shared-memory object is fabricated. Acquiring ownership adds no kernel handles,
threads, wakeups or guest time. Existing response preflight precedes all mutation.
Kernel time remains zero; scheduling and file timing remain incomplete.

Changed implementation files: CMakeLists.txt, ctr_runner.cpp (GSP registration only),
fs_user_service.cpp (getter), new gsp_gpu_service.h, new ctr_fs_priority_test.cpp and
ctr_gsp_acquire_test.cpp, plus one obsolete unsupported-getter test expectation.
Original game code, private AOT, vendor, kernel, production IPC, writable file
backend, RomFS reads, PTM, APT, NDM and CFG behavior remain unchanged.

## Validation and evidence

Full GCC baseline and Clang builds linked all 599 private AOT pages. Final GCC
21/21 and Clang 21/21 CTest passed; Clang ASan/UBSan 21/21 ROM-free suites passed
with leak checking and halt-on-error enabled. Eleven startup scenarios are byte-
identical between compilers: fresh, existing, PTM-off, alternate RTC, no RomFS,
no root, missing archive, invalid root, invalid PTM mode, missing RomFS and wrong
RomFS size. The guest-written coin bytes and old negative paths are preserved.

All 603 regular AOT backup members remain unchanged (599 page C++ files). The
111043 registry blocks and 545111 raw words are STATIC verification counts,
not frames, executed instructions or gameplay completion. Raw RomFS SHA matches,
and the restored image again passed all three IVFC levels (187787 hash blocks).

Public report: reports/recovery-host/FS-PRIORITY-GSP.md.
Public evidence: GSP-FRESH-GCC.txt, GSP-EXISTING-GCC.txt,
GSP-ROMFS-READ-PROOF.json, GSP-VALIDATION.json in the same directory.
Complete private evidence/scripts: priority-checkpoint/. The logging-only trace
uses an alternate IPC object; ordinary production runs use unchanged IPC code.
A test compilation initially needed an enum-to-u32 cast and was corrected before
final passing suites. No failure was suppressed. Streaming exec was unsupported;
the normal finite build drivers produced the recorded successful builds.

## Next exact work

Implement observed RegisterInterruptRelayQueue with actual state and a real shared-
memory object. Read pinned GSP constructor, SessionData allocation/destruction,
relay layout and handler, then existing kernel shared-memory object/mapping code.
Extend internal session identity to correct numeric relay-slot allocation; do not
invent a thread ID just to pass the request. Validate the copied EVENT object,
response capacity and output-handle allocation before committing registration.

Pinned handler stores the actual event and flags, marks the session registered,
returns a specific NONZERO ResultFirstInitialization on the first registration,
then returns the session's thread ID and a COPY of the shared-memory object.
Its constructor creates a 0x1000-byte shared object with read/write permissions.
This is inspected reference HLE, NOT current implemented behavior. Derive exact
result values and layouts from source; do not guess normal success or fake an event.

Then run original code against another NEW empty test root with empty PTM and
verified RomFS. Observe the next real request. SVC 0x1F may recur with an actual
shared-memory handle, but do not bypass or claim that mapping before it is seen.
No guessed frame completion, interrupts, time advances or gameplay success.

Primary source pin: azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
Inspected: src/core/hle/service/fs/fs_user.cpp GetPriority/SetPriority (blob
4f538400de9608ac92c212dc0eb8fc19c241e573, body in lines 1000-1290),
src/core/hle/service/gsp/gsp_gpu.cpp (blob 6f915e4d6a5d27853321d0a233103afb9d877bc0):
AcquireGpuRight/AcquireRight and constructor/session setup (910-1190),
ClientDisconnected/GetUnusedThreadId (1-150), RegisterInterruptRelayQueue (290-530);
gsp_gpu.h declarations (blob fe08556003db960ba22a69cb5846a2c360d4aaca).

## Working paths and reproducible continuation

Root: /mnt/data/lego_recovery/. Source: repo/. Private AOT: generated2/.
Code: restored/code.bin. Raw RomFS: game/prepared-romfs/romfs.bin.
Prepared parts currently mounted at romfs-library-roundtrip/ with romfs-parts.json.
Builds: build-gcc/, build-clang/, build-asan/. Current evidence: priority-checkpoint/.
Prior evidence: romfs-checkpoint/, writefile-checkpoint/, openfile-checkpoint/,
ptm-checkpoint/, createfile-checkpoint/. Original CCI was not re-extracted this turn.

NEW matrix state: private-state/priority-validation.0ysw5i19/.
Other new roots: priority-baseline.*, priority-first.*, priority-trace.*,
gsp-discovery.* and gsp-acquire.*. These are guest-created test state, NOT recovered
NAND saves. Historical restored state was preserved. The paired matrix resets only
its own newly created and byte-verified files; never reuse/delete an unknown save.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
mkdir -p private-state
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/private-state/gsp-queue-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty --romfs game/prepared-romfs/romfs.bin
```

Expected: RegisterInterruptRelayQueue 0x00130042, round 94, diagnostic exit 3.
Use clang++ for Clang; omit LEGO_AOT_DIR for ROM-free tests. Full sanitizer flags
and commands: priority-checkpoint/build_remaining.py. No Windows/macOS build claimed.

## Durable input recovery and every-turn mandate

GitHub: GTTeancum/Lego-Undercover-3DS-Recomp, branch main.
Private Library: /LEGO-Chase-Recovery/.
Restore latest source/log archive and handoff, code.bin, and
LEGO-Chase-current-AOT-599pages-2026-10-03.tgz. Verify CHECKPOINT-MANIFEST.json,
unpack source/logs below /mnt/data/lego_recovery, code.bin into restored/, and
unpack AOT there (creates generated2/). SOURCE-INDEX.json retains tracked paths,
blob hashes and modes, including ignored-but-tracked evidence logs.

Raw RomFS is durably EXTRACTED in Prepared-RomFS/; restore the two raw parts plus
romfs-parts.json instead of repeating CCI extraction. Part001 has 402653184 bytes,
part002 has 366526464. Restore refuses to overwrite an existing output:

```sh
mkdir -p game/prepared-romfs
python repo/tools/restore_romfs_parts.py game/romfs-persistence/romfs-parts.json game/prepared-romfs/romfs.bin
python repo/tools/verify_romfs.py game/prepared-romfs/romfs.bin
```

Raw RomFS: 769179648 bytes; native view offset 4096, size 769175552. Do NOT expose
the IVFC prefix or truncate the native view to level-3 data alone.
SHA-256: 6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf.
code.bin SHA: 5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
AOT archive SHA: 2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9.
Historical CCI SHA: 3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525.
Six original archive parts remain in Library Game-archive/ as an alternate route.
The previous RomFS handoff and persistence receipt retain per-part hashes.

Scratch can reset. Prepared Library inputs, source/log backups and GitHub are
recovery routes; Actions source snapshots expire after 30 days. Connector reads
and writes work; direct container GitHub DNS failed. No Work or user-PC development.

MANDATE: work in tested checkpoints; push source/reports before ending; keep game
bytes, private AOT/binaries/test state OUT of public Git; update this canonical
handoff and POST A DOWNLOADABLE COPY EVERY WORK TURN plus a durable source/log
backup. This file must suffice for a new chat to restore, rebuild, reproduce the
actual stop and continue without guessing. Check backups before requesting uploads.

## Hosted CI confirmation

GitHub Actions `37315649774` on implementation `ad22af5` completed successfully
for both GCC and Clang jobs. Hosted tests are ROM-free, not original-game runs.
