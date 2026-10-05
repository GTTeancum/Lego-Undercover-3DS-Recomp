# LEGO Chase Begins — canonical recovery handoff

NotifyToWait checkpoint. Read this first. Continue in scratch, not on the user's
PC or through Work. Historical Recovery F/J gameplay is not current proof.

## Current verified boundary

The headless LEGOChaseNative builds with all 599 private AOT pages and runs the
verified original USA executable. It now passes the observed APT:NotifyToWait
request (0x00430040, applet 0x300). Its next explicit stop is:

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=54
last_ipc_session=APT:U requested_service= request_header=0x004b00c2
ipc_words=004b00c2 00000007 00000004 00000001 00010402 0ffff610 ...
ipc_static_buffer0=00004002 0ffff5dc
host_exit_code=3
```

This is APT:AppletUtility, utility ID 7, input size 4, output size 1. Read-only
stop inspection found input bytes `10 00 00 00` at 0x0FFFF610. The static output
slot points to 0x0FFFF5DC. No AppletUtility response has been implemented. The
request and output buffer remain unchanged at the stop.

No title screen, completed initializer count, renderer, audio, controls or
playable release is established. Dispatch rounds measure host dispatch calls,
not frames or gameplay progress.

## Source, scope, and evidence

Base main: 175e78c7801b01285d9413918edf1a3e1ca8478d.
Restored source/index exactly matched tree
2b75b10396aed1f973bb9136b46237a04cbd2d64 before editing. The existing NotifyToWait
stop was reproduced byte-for-byte against ENABLE-STARTUP-GCC.txt.

Implementation commit: 275d963c2f72a915bf1279f7d9368c0db7a9b9d9.
Tested source tree: b6f3133ce93ac15d65816e675482ea5ea58df488.

NotifyToWait is intentionally a **bounded pinned-HLE acknowledgment**, not a
claim of fully recovered hardware behavior. The pinned Azahar apt.cpp handler
reads the applet ID and replies with one ResultSuccess word; it explicitly calls
itself STUBBED. Our supported shape requires an initialized/registered application,
exact header 0x00430040, and applet ID 0x300. No events, message state, threads,
handles, or guest time are changed by the handler. Other shapes remain host stops.

Implementation changes: src/services/apt_service.cpp, the new
ctr_apt_notify_test.cpp, CMake test registration, and replacement of the now-valid
NotifyToWait negative-test case in ctr_apt_enable_test.cpp with GetSharedFont.
No kernel, dispatcher, memory, NDM, vendor or generated-page code was modified.

Behavior source, pinned to avoid drifting current upstream:
https://github.com/azahar-emu/azahar/blob/86a9f9236ae42bb5a2b995dbc933d599d8ea07ac/src/core/hle/service/apt/apt.cpp
NotifyToWait: lines 355–364. Command mapping is in apt_u.cpp. The 3dbrew NS/APT
service list confirms header 0x00430040, but this checkpoint does not establish
hardware NotifyToWait side effects beyond the HLE policy.

## Validation and delivery

- Fresh GCC baseline/full private build and final relink; fresh full Clang build.
- Both final real-game runs stop identically at AppletUtility; exit code 3.
- GCC 8/8 CTest and Clang 8/8 CTest passed.
- Clang ASan/UBSan 8/8 ROM-free CTest passed, with leak checking and halt-on-error.
- GitHub Actions 37246469038 succeeded on implementation commit 275d963, both
  GCC and Clang jobs. Later report-only changes do not change the tested runtime.
- Tests cover exact response/register preservation, repeated acknowledgment
  before/after event acquisition, after message consumption, across sessions,
  an unrelated blocked child staying blocked, malformed/premature calls, and
  read-only/partly writable response areas without partial mutation.
- All 603 regular private AOT archive members are still byte-identical; 599 are
  page C++ files. Do not manufacture a 604th file to match old reports.
- 545,111 checked raw instruction words and 111,043 registry blocks are static
  inventory, not executed counts or proof of complete ARM lowering.
- See reports/recovery-host/APT-NOTIFY.json and NOTIFY-*.txt for exact evidence.

## Next exact step

Inspect the pinned AppletUtility implementation and supporting documentation for
utility ID 7. Read the observed 4-byte input (0x10) and 1-byte output contract;
then implement only the justified request and rerun unchanged game code. Do not
blanket-acknowledge all utilities, manufacture notification completion, replace
launch messages, or signal events merely to advance startup.

Keep previous limitations explicit: application-only APT state; empty launch
Wakeup; ReceiveParameter consume-once is component-tested but not newly claimed
observed here; Glance copy/Receive move descriptors follow 3dbrew whereas requested
size padding follows pinned Azahar. Hardware wire parity is unresolved. NDM is
pinned HLE bookkeeping, not network execution. Dispatch-driven guest-time advance,
full scheduler correctness, other services, and renderer remain unreconstructed.

## Working files and commands

Scratch root: /mnt/data/lego_recovery/.
Source: repo/; private pages: generated2/; verified image: restored/code.bin.
Builds: build-gcc/, build-clang/, build-asan/. Current logs and the read-only
next-request inspection harness: notify-checkpoint/.

Restored this turn from /mnt/data/LEGO-Chase-source-checkpoint-175e78c.tgz and the
existing private AOT archive under restored/. No user upload was needed. The
source tree was verified including the tracked evidence .log ignored by default
Git rules. Local Git is a snapshot/index, not the full remote history.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
./build-gcc/LEGOChaseNative restored/code.bin
```

Use clang++ for Clang and omit LEGO_AOT_DIR for ROM-free builds. Launcher options
are --block-limit and --host-event-limit. GetSystemTick reads kernel guest time;
no new host-time policy was introduced. No Windows build is claimed.

## Durable recovery and per-turn mandate

Private Library /LEGO-Chase-Recovery/ holds the original six archive parts under
Game-archive/, code.bin, LEGO-Chase-current-AOT-599pages-2026-10-03.tgz, previous
source checkpoints, and restore tools. The original parts also remain in /mnt/data/
and backup-verify/. See GAME-FILES-PERSISTENCE.md. A full CCI re-extraction was not
necessary for this startup work. Scratch can reset: Library and GitHub are the
recovery paths, not a promise of permanent scratch storage.

Code SHA-256: 5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
CCI SHA-256: 3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525.

Work in moderate tested checkpoints, push source/reports before ending, keep
original game bytes and derived AOT pages private, update this handoff and attach
a downloadable copy every turn. Inspect/restore backups before asking for files.
The attached handoff adds the final delivery commit and private archive name.
