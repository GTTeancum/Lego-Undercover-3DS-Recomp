# LEGO Chase Begins — canonical recovery handoff

Updated October 4, 2026. Read this first. Continue here, not on the user's PC or
via Work handoff. Historical Recovery F/J gameplay is not current recovery proof.

## Current real-game boundary

The headless LEGOChaseNative builds with all 599 private AOT pages and runs the
verified original USA executable. The observed NDM startup requests now pass:

1. ndm:u OverrideDefaultDaemons: header 0x00140040, mask 0xF.
2. ndm:u SuspendDaemons: header 0x00060040, mask 0x6 (BOSS and NIM).
3. The real program next asks APT:U to Enable: header 0x00030040, attributes 0.

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=39
last_ipc_session=APT:U requested_service= request_header=0x00030040
ipc_words=00030040 00000000 00000010 00048010 ...
host_exit_code=3
```

The pending APT request is untouched at the stop. No APT Enable response was
invented and the original Break SVC was not bypassed. No title screen, completed
initializer count, graphics, audio, controls or gameplay is established.

Base commit: 8f3fec87ca83a8cb578c72c26bb15f8934676a98.
Before editing, the restored source matched remote tree
c7b0c5f547a0629848c6c9d7e726edad6213e141 exactly. The previous missing-ndm:u Break
was reproduced first, then the service was registered with no handlers so each
actual request could be observed before implementing it.

## This checkpoint

Added src/services/ndm_service.h and .cpp, registered one shared ndm:u handler
per runner, and added tests/ctr_ndm_test.cpp as the sixth CTest suite. No existing
APT, kernel, IPC, vendor, generated-page or original executable code was changed.
CMake integration, reports and this handoff were updated.

NDM tracks default/current masks, four daemon status fields and nested suspend
counts across sessions. Masks use only the low four bits. The observed 0xF then
0x6 sequence leaves default mask 0xF, current mask 0x9, and counts [0,1,1,0].
These fields follow the pinned Azahar HLE implementation, whose operations are
labeled STUBBED; this is NOT verified firmware or network-daemon execution parity.
No network threads, online state, timers, kernel wakeups or fake events are added.

Two potentially surprising upstream policies are retained explicitly: Suspend
recomputes the current mask from the default mask rather than cumulatively from
the previous mask; Override idles selected statuses without clearing existing
suspend counts. Tests pin these choices. Do not silently treat them as proven
hardware behavior. Unknown commands and malformed header counts stop before
side effects. Resume/query commands are not guessed. Counter overflow is guarded
by a host stop, not wrapping. Response writability is checked by the existing
IPC router before the service can mutate state.

## Validation and evidence

- Full GCC and Clang builds linked all 599 private page translation units.
- Final real-game startup logs are byte-identical at APT Enable.
- GCC 6/6 CTest and Clang 6/6 CTest passed.
- Clang ASan/UBSan 6/6 ROM-free suites passed with leak checking enabled.
- Tests cover observed masks, nested counts, unselected states, high-bit masks,
  service lifetime across sessions, exact response headers, unchanged registers
  and buffers on unknown commands, read-only/partial command memory preflight,
  and absence of artificial kernel/event/time side effects.
- All 603 regular AOT archive members are unchanged; 599 are page C++ files.
  Do not manufacture a 604th file to match older historical reports.
- 545,111 raw instruction words and 111,043 registry blocks are static counts,
  not executed-block counts or proof of complete ARM semantics.
- reports/recovery-host/NDM-STARTUP-2026-10-04.json records source references,
  the observed sequence, limitations and exact log/binary hashes.
- NDM-*.txt alongside it are real run/test logs. Check GitHub Actions for the
  delivery commit separately; hosted CI uses ROM-free tests, not game bytes.

## Next exact step

Implement only the observed APT:Enable application request (0x00030040, attrs=0)
from pinned AppletManager semantics, then rerun the unchanged real game and stop
at its next unsupported request. Initialize already queued the initial Wakeup;
Enable must not manufacture another parameter or re-signal an event just to
advance the game. Preserve the previously established peek/consume semantics.

APT GlanceParameter peeks the queued empty launch Wakeup. ReceiveParameter is
component-tested consume-once behavior, not yet claimed reached by the game.
Static buffer 0 lives at TLS+0x180, separate from the command at TLS+0x80.
The prior wire policy uses Glance copy / Receive move descriptors (3dbrew), but
pads to requested size (pinned Azahar). Azahar uses move for both; 3dbrew describes
actual payload size for the static descriptor. Hardware parity remains unresolved.
See APT-PARAMETER-2026-10-04.json. Keep unimplemented roles/messages explicit.

## Working files and rebuild

Scratch root: /mnt/data/lego_recovery/.
Source: repo/; real builds: build-gcc/ and build-clang/; sanitizer: build-asan/;
private pages: generated2/; verified executable input: restored/code.bin.
This checkpoint's configure/build/test/run logs: ndm-checkpoint/.
Original six archive parts remain in /mnt/data/ and backup-verify/.
No large CCI re-extraction was needed for this executable-only startup work.

```sh
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
./build-gcc/LEGOChaseNative restored/code.bin
```

Omit LEGO_AOT_DIR for ROM-free builds; use clang++ for Clang. Launcher limits are
--block-limit and --host-event-limit. No forced successful boot. GetSystemTick
reads kernel guest time; dispatch-driven time progression remains unreconstructed.

## Durable recovery and continuing mandate

Private Library /LEGO-Chase-Recovery/ contains the six parts under Game-archive/,
code.bin, LEGO-Chase-current-AOT-599pages-2026-10-03.tgz, source checkpoints and
restore tools. Source 8f3fec8 was restored from its delivered archive this turn;
no user reupload was needed. See GAME-FILES-PERSISTENCE.md for the full procedure.
Scratch can reset. Library backups and GitHub source are the durable recovery
paths, not a claim that scratch is permanent.

Code SHA-256: 5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
CCI SHA-256: 3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525.

Use moderate tested checkpoints. Push source and reports before ending, keep all
game bytes and private AOT pages out of the public repository, update this
canonical handoff and attach a downloadable copy every turn. Check backups before
asking for another upload. Delivery commit, hosted CI confirmation and private
source/log archive are recorded in the attached handoff after the push.
