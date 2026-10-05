# LEGO Chase Begins — canonical recovery handoff

Utility 7 checkpoint. Read this first. Continue in scratch, not on the user's PC
or through Work. Historical Recovery F/J gameplay is not current proof.

## Current real-game boundary

The headless LEGOChaseNative builds with the 599 private AOT pages and runs the
verified original USA executable. The observed AppletUtility ID 7 request now
passes through a bounded UnlockTransition HLE compatibility path. The next stop is:

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=59
last_ipc_session=APT:U requested_service= request_header=0x004b00c2
ipc_words=004b00c2 00000004 00000001 00000001 00004402 0ffff5d8 ...
ipc_static_buffer0=00004002 0ffff5dc
host_exit_code=3
```

This next request is AppletUtility ID 4, input size 1, output size 1. A read-only
inspection of the stopped game found input byte 00 at 0x0FFFF5D8. Its output is
at 0x0FFFF5DC. Utility 4 remains unimplemented; no response or event is invented.
There is no title screen, completed initializer count, rendering, audio, controls
or gameplay proof. Dispatch rounds are host calls, not frames.

## Source and implementation boundary

Base commit: c2292df90bff00bfbff789cfa74a0d6610b89cf1.
Restored source/index matched base tree
6c6087a13d7bb010f213c77e5d9a2457ba56ba9e before editing. The original utility 7
stop was reproduced byte-for-byte against NOTIFY-STARTUP-GCC.txt.

Implementation commit: 0f0fbf2d00b72f4b65adb4e6baaae712534afd90.
Tested and pushed source tree: 891f7d3954faa5b473ff05c15f5f2df75da15d5a.
Changes: apt_service.h/.cpp, ctr_apt_utility_test.cpp and CMake test registration.
No kernel, dispatcher, memory bus, IPC router, NDM, vendor or private game-page
source changed. Later delivery changes only add this handoff and evidence.

The primary libctru source identifies utility 7 as APT_UnlockTransition, taking a
four-byte transition value and one-byte output. Pinned Azahar AppletUtility is
explicitly STUBBED: it copies but ignores that input and returns two success words
with a zero-initialized output buffer for utility 7. This checkpoint reproduces
that limited policy. It does NOT restore actual transition locks, pretend that a
lock was acquired, or implement utility 6's upstream fabricated TryLock success.

Supported shape: initialized/registered application, header 0x004B00C2, utility 7,
input size 4, output size 1, input static slot 1 descriptor 0x10402. The previously
captured real input was 0x10; other uint32 inputs receive the same input-ignored
reference HLE policy, not a claim of full hardware behavior for all masks.

The implementation reads all four input bytes before any output write. It checks
the TLS+0x180 static receive descriptor and writable one-byte destination, then
writes exactly one zero byte and a reply shaped as:

```text
004B0082 00000000 00000000 00004002 <destination>
```

The two result words are service and utility results, distinct from the SVC's
transport result. The guest write invalidates relevant exclusive reservations.
No guest-sized allocation, message change, event signal, new handle, thread wake,
registration change or guest-time advance is performed. Other utilities and
unsupported shapes still stop explicitly. Bad pointers/output descriptors and
reply/descriptor overlap are rejected before writes as host safety policy, not
assertions about hardware error behavior. Input/output overlap is safe because
the input is copied first.

## Validation and evidence

- Fresh full 599-page GCC baseline build, then final changed-source relink.
- Fresh full 599-page Clang build of the final source.
- Final GCC/Clang real-game startup logs are byte-identical at utility 4.
- GCC 9/9 CTest; Clang 9/9 CTest.
- Clang ASan/UBSan 9/9 ROM-free suites, detect_leaks=1 and halt_on_error=1.
- GitHub Actions 37248287001 passed both compiler jobs on source commit 0f0fbf2.
- Tests cover exact reply/register preservation, one-byte boundaries, retained
  parameter/event state before and after consumption, reopening sessions,
  unknown utilities, malformed/premature shapes, protected/missing buffers,
  32-bit address wrap, alias safety, and exclusive-reservation invalidation.
- Every one of the private archive's 603 regular members remains byte-identical;
  599 are page C++ files. Do not manufacture a historical 604th artifact.
- 545,111 checked raw words / 111,043 registry blocks are static inventory, not
  execution counts or proof of complete instruction lowering.
- reports/recovery-host/APT-UTILITY7.json and UTILITY7-*.txt preserve the baseline,
  final startup logs, tests, next-input inspection and fingerprints.

Behavior sources:
- azahar-emu/azahar at 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac,
  src/core/hle/service/apt/apt.cpp, AppletUtility (around lines 725–748).
- devkitPro/libctru at 9b55eda44cf80b971503991e0c78f9bc8fe50425,
  libctru/source/services/apt.c, APT_UnlockTransition wrapper.
The 3dbrew AppletUtility page could not be retrieved this turn; do not pretend
its full utility semantics were checked. Reference implementation limitations
are explicit above.

## Next exact step

Inspect the observed utility 4 command's reference semantics and its actual
one-byte input 00. Implement only the justified request, then rerun unchanged
game code to discover the next stop. Do not blanket-acknowledge all utilities or
manufacture shell/notification completion or transition-lock state. Preserve
strict unsupported IPC and Break stops.

Earlier limits remain: application-only APT state, empty launch Wakeup and
component-tested ReceiveParameter consumption; unresolved Glance copy/Receive move
versus upstream wire-policy details; NDM is HLE bookkeeping, not networking;
dispatch-driven guest time and full scheduler correctness remain incomplete.

## Working files and commands

Root: /mnt/data/lego_recovery/.
Source: repo/. Private pages: generated2/. Verified executable: restored/code.bin.
Builds: build-gcc/, build-clang/, build-asan/. New logs/scripts and read-only
next-request harness: utility-checkpoint/.

The source was restored from LEGO-Chase-source-checkpoint-c2292df.tgz; private
pages were restored from restored/LEGO-Chase-current-AOT-599pages-2026-10-03.tgz.
No user upload or full CCI extraction was needed. Local Git is a verified
snapshot/index, not a full remote-history clone.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
./build-gcc/LEGOChaseNative restored/code.bin
```

Use clang++ for Clang and omit LEGO_AOT_DIR for ROM-free tests. Launcher limits:
--block-limit and --host-event-limit. No Windows build is claimed.

## Persistence and per-turn mandate

Private Library /LEGO-Chase-Recovery/ holds code.bin, the 599-page archive, source
checkpoints, and six original archive parts under Game-archive/. The parts also
remain in /mnt/data/ and backup-verify/. See GAME-FILES-PERSISTENCE.md for recovery.
Scratch may reset; private Library backups plus GitHub source are the recovery
paths, not a promise of permanent scratch.

Code SHA-256: 5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
CCI SHA-256: 3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525.

Use moderate tested checkpoints. Push source/reports before ending, keep game
images and derived AOT pages private, update this canonical handoff and attach a
downloadable copy every turn. Inspect backups before asking for uploads. The
attached handoff adds the final delivery commit and private source/log archive.
