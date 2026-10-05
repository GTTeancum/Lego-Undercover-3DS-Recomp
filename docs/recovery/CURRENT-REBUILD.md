# LEGO Chase Begins — canonical recovery handoff

Utility 4 checkpoint. Read this first. Continue in scratch, not on the user's PC
or through Work. Historical Recovery F/J gameplay is not current execution proof.

## Current real-game boundary

The headless LEGOChaseNative links all 599 private AOT pages and runs the verified
USA executable. APT utility 4 now passes through the bounded reference HLE path.
The next real stop is no longer an unsupported APT request:

```text
stop=MemoryFault pc=0x00117b9c detail=0x1ff81000 thread=1 dispatch_rounds=63
r9=0x1ff81000
last_ipc_session=APT:U requested_service= request_header=0x004b00c2
ipc_words=004b0082 00000000 00000000 00004002 0ffff5dc ...
host_exit_code=3
```

The instruction is `ldr r3, [r9]`. The address is the CTR shared page; pinned
SharedPageDef puts date_time_counter at offset 0 and two DateTime snapshots at
0x20/0x40. The surrounding guest code selects a snapshot using the counter's low
bit. No shared-page mapping or time contents were fabricated to bypass the fault.
There is no title screen, completed initializer count, rendering, audio, controls
or gameplay proof. Dispatch rounds are host dispatch calls, not frames.

## Source reconciliation and implementation

Base commit: e6065d0f1dc7e0f5b6b46c20e507560f7fa3f432.
The restored source/index matched remote tree
4db5d6874eb1679b8a55bf4c68f7c9eb1bc288e7 before edits. All tracked archive paths,
including the ignored historical .log file, were staged explicitly to verify it.
The old utility 4 stop was reproduced byte-for-byte before changes.

Source commit: b69093b728e006625b1a992a641b9db1a5598b8c.
Tested source tree: 0020bfb7b49642c990ebb8449529e8307653064b.
Only apt_service.h/.cpp and the existing ctr_apt_utility_test.cpp changed.
No kernel, memory, dispatcher, IPC router, NDM, vendor or AOT bytes changed.

libctru calls utility 4 APT_SleepIfShellClosed with one input byte initialized to
zero and one output byte. Pinned Azahar AppletUtility explicitly STUBS this with
ignored input, two success words and zero output. We reproduce that bounded HLE
compatibility policy, NOT physical lid/sleep behavior or sleep completion.

The shared utility handler supports only IDs 4 and 7. Utility 4 reads exactly one
input byte; utility 7 retains its four-byte input. Both require a registered,
initialized application, exact request shape, static input slot 1, and one-byte
output. It validates readable input and output descriptor/capacity/writability
before writing. Exactly one zero byte is returned; no guest-sized allocation,
event signal, message consumption, handle creation, scheduling or time change.
Unknown utilities, including TryLockTransition (6), remain explicit host stops.

Tests now run the existing state/buffer matrix for BOTH utilities. They retain
utility 7 coverage and add valid one-byte input/output at 0xFFFFFFFF. Utility 4
never requires four readable bytes. Nonzero byte inputs follow the reference's
input-ignored policy; this is not a hardware claim. Pointer failures and response
alias protection retain the previous host safety policy.

## Validation

- Fresh GCC full baseline build, final changed-source relink, 9/9 CTest.
- Fresh Clang full 599-page build, 9/9 CTest.
- Clang ASan/UBSan 9/9 ROM-free CTest with leak checking and halt-on-error.
- Final real-game GCC and Clang startup logs are byte-identical.
- GitHub Actions 37255939684: GCC and Clang jobs succeeded on b69093b.
- All 603 regular private AOT archive members were compared byte-for-byte and
  remain unchanged; 599 are page C++ files. Do not invent a 604th artifact.
- 545,111 verified raw instruction words and 111,043 registry blocks remain
  static inventory counts, not execution counts or proof of full ARM semantics.
- Reports: reports/recovery-host/APT-UTILITY4.json and UTILITY4-*.txt.
- Source, report, executable and log fingerprints are recorded in that JSON.

## Next exact step

Restore the observed CTR shared-page mapping at 0x1FF81000, using pinned
shared_page.h/.cpp and the actual guest clock-reading code as evidence. It is a
read-only process page, not an ordinary writable heap allocation. Determine the
DateTime snapshot fields and their relationship to GetSystemTick before populating
them. Do not just add a zero-filled writable page or advance time/wake threads to
make the fault disappear. Keep unknown services and other faults explicit.

Reference paths:
- devkitPro/libctru commit 9b55eda44cf80b971503991e0c78f9bc8fe50425,
  libctru/source/services/apt.c, APT_SleepIfShellClosed.
- azahar-emu/azahar commit 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac,
  src/core/hle/service/apt/apt.cpp, AppletUtility;
  src/core/hle/kernel/shared_page.h, SharedPageDef/DateTime.
The 3dbrew AppletUtility page returned 403; no new hardware semantics are claimed.

Earlier limits remain: application-only APT, empty launch Wakeup, unresolved
Glance/Receive descriptor/padding policy differences, NDM HLE bookkeeping rather
than networking, incomplete dispatch-time progression and scheduler correctness.
The physical shell/sleep lifecycle and transition locks are not implemented.

## Working files and rebuild

Root: /mnt/data/lego_recovery/.
Source: repo/. Pages: generated2/. Verified code: restored/code.bin.
Builds: build-gcc/, build-clang/, build-asan/. New logs/scripts: utility4-checkpoint/.
Source restored from LEGO-Chase-source-checkpoint-e6065d0.tgz; pages restored
from restored/LEGO-Chase-current-AOT-599pages-2026-10-03.tgz. No new user upload.
Local Git is a verified snapshot/index, not the full remote history.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
./build-gcc/LEGOChaseNative restored/code.bin
```

Use clang++ for Clang; omit LEGO_AOT_DIR for ROM-free tests. Launcher limits are
--block-limit and --host-event-limit. No Windows build is claimed.

## Persistence and per-turn mandate

Private Library /LEGO-Chase-Recovery/ contains original six archive parts under
Game-archive/, code.bin, the unchanged 599-page archive and source checkpoints.
All six parts also remain in /mnt/data/ and backup-verify/. Scratch can reset;
Library backups and GitHub source are the recovery paths, not permanent scratch.
See GAME-FILES-PERSISTENCE.md. No full CCI re-extraction was needed this turn.

Code SHA-256: 5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
CCI SHA-256: 3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525.

Use moderate tested checkpoints. Push source/reports, keep original/game-derived
bytes private, update this handoff and attach a downloadable copy each turn.
Inspect backups before requesting another upload. The delivered handoff adds the
final commit and private source/log archive after remote confirmation.
