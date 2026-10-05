# LEGO Chase Begins — canonical recovery handoff

Shared-clock checkpoint. Read this first. Continue in scratch, not on the user's
PC or through Work. Historical Recovery F/J gameplay is not current execution
proof. Keep game bytes private and checkpoint source to GitHub each turn.

## Current real-game boundary

The headless LEGOChaseNative links the unchanged 599 private AOT pages and runs
the verified USA executable. The shared-page clock read now succeeds, including
the legacy DMB barrier in the guest reader. The next observed missing service is
`fs:USER`; the game receives ServiceNotRegistered and subsequently waits:

```text
shared_clock_counter=1 snapshot_ms_since_1900=3155673600000 snapshot_tick=0 guest_now_ns=0
stop=WaitingNoRunnableThread pc=0x0024b324 detail=0x00000000 thread=1 dispatch_rounds=65
last_ipc_session=srv: requested_service=fs:USER request_header=0x00050100
ipc_words=00050040 d0406401 00000000 00000000 ...
host_exit_code=3
```

No filesystem handler, file access, save success, title screen, completed
initializer count, rendering, audio, controls or gameplay has been established.
Dispatch rounds are host dispatch calls, not frames or a completion percentage.

## Source reconciliation

Base commit: 58d3cf9cd8c07c9bd61ccba0387873c32a584081.
The restored source/index matched remote tree
5c299920dea0d6df9f7a144d3b61a8a612994970 before edits. The prior shared-page
MemoryFault at 0x00117B9C / address 0x1FF81000 was reproduced byte-for-byte.

Source commit: 3f364c925e35a504b7e671cd5a89af90a8eb8fb9.
Tested source tree: edb0e257fa69f255da19edfd26bceaced8f299fd.
Local Git is a verified snapshot/index, not the full remote-history clone.

## Shared clock implementation and limits

The dedicated page at 0x1FF81000 is read-only to the guest. Existing full/partial
mappings are rejected without adoption or overwrite. Counter is at +0; two
32-byte time records are at +0x20/+0x40. Each contains little-endian u64 values:
calendar milliseconds since 1900, update tick, ticks-per-second coefficient,
and adjustment offset. Publish the inactive record before incrementing counter.

Both snapshots and svcGetSystemTick use the same overflow-safe conversion from
kernel nanoseconds, with 268111856 ticks/second. The explicit deterministic RTC
default is **2000-01-01**, not the original console clock or today's wall time.
The launcher accepts `--rtc-ms-since-1900 N`. Reject invalid epochs, clock rebases
and time rollback rather than silently changing the time domain.

Refresh checks hourly guest-time buckets before dispatch. Missed hourly callbacks
coalesce at the supplied time; no periodic host task or guest-time advancement is
implemented. This run's kernel time remains zero. Tests explicitly advance time
to validate snapshots and SVC interpolation, but the real runner does not do so
to bypass waits. Dispatch-driven time and idle-deadline scheduling remain open.

Only clock fields are modeled. Other page bytes remain zero/unmodeled; do not
claim hardware, battery, wireless, lid or full shared-page parity. Clock offset
is zero; RTC adjustment/drift behavior is not implemented.

## Narrow archived-metadata correction

Mapping the page exposed a Fallback at guest PC 0x00117BC0, raw 0xEE074FBA.
This is the legacy CP15 DMB; the saved LLVM AOT incorrectly categorized it as
CoreAlu. The pinned native system backend already implements its memory fence.

One local patch in vendor/triaevum-a9b4477/recomp/a32_runtime.cpp routes only the
recognized DMB form to that existing backend and continues the remaining block.
Normal condition checks remain. Other unknown system/ALU operations still stop.
This is not a general fallback or a guest-PC skip. An experimental callback that
returned mid-block caused a MissingBlock at pc+4 and was rejected/removed.

The LLVM generator now classifies future instances as CoreSystem. No private
page was edited or regenerated. **The vendor slice now has a documented local
patch**; see vendor/triaevum-a9b4477/LOCAL-PATCHES.md for original/patched hashes.
Do not describe the patched vendor runtime as byte-identical to its pinned base.

## Validation

- Full GCC 599-page baseline build and final changed-source rebuild/relink.
- Fresh Clang full 599-page build and final changed-source rebuild/relink.
- Final GCC/Clang real-game startup logs are byte-identical at missing fs:USER.
- GCC 10/10 CTest; Clang 10/10 CTest.
- Clang ASan/UBSan 10/10 ROM-free CTest, leak checking and halt-on-error enabled.
- LLVM-generator Python tests: 15/15 passed locally.
- GitHub Actions 37258415292 passed GCC and Clang on source commit 3f364c9.
- Tests cover read-only access, snapshot alternation/interpolation, hourly and
  delayed refresh, epoch/overflow/rollback bounds, conflicting mappings, DMB
  Rt0-14 and conditions, continuation within a block and unknown-op strict stops.
- Alternate valid RTC is visible in diagnostics with the same next service stop;
  invalid epoch zero exits 2 before guest execution.
- All 603 regular private AOT archive members remain byte-identical; 599 are
  page C++ files. Do not manufacture a 604th artifact.
- 545111 verified raw words / 111043 registry blocks are static inventory counts,
  not executed instruction counts or proof of complete ARM semantics.
- See reports/recovery-host/SHARED-CLOCK.json and CLOCK-*.txt for actual baseline,
  intermediate/final logs, test results, binary/source fingerprints and scope.

## Next exact step

Register a strict `fs:USER` service entry, then rerun the unchanged real game to
capture its first filesystem command. Review the pinned service and available
private recovered source as evidence, not a license to copy broad success stubs.
Implement in observed order with exact IPC layouts and resource/error behavior.
Do not report archive mounting, file reads or saving until actually exercised.

Keep the current clock policy explicit. Do not add time jumps, artificial event
signals, fake resource handles or successful file opens to make startup advance.
Unknown services/commands remain explicit errors or unsupported host stops.

Primary evidence: azahar-emu/azahar commit
86a9f9236ae42bb5a2b995dbc933d599d8ea07ac,
src/core/hle/kernel/shared_page.h/.cpp, plus the pinned local
vendor/triaevum-a9b4477/recomp/a32_core.cpp system-fence implementation.
Relevant guest clock instructions were inspected privately; game disassembly is
not included in the public source checkpoint.

Earlier limits remain: application-only APT; empty launch Wakeup; unresolved
Glance/Receive descriptor/padding policy differences; utility/NotifyToWait HLE
acknowledgments rather than full physical shell/sleep/transition semantics;
NDM bookkeeping rather than networking; inherited scheduler/kernel limitations.

## Working files and rebuild

Root: /mnt/data/lego_recovery/.
Source: repo/. Private pages: generated2/. Verified code: restored/code.bin.
Builds: build-gcc/, build-clang/, build-asan/. Logs/scripts: clock-checkpoint/.
Source restored from LEGO-Chase-source-checkpoint-58d3cf9.tgz; private pages from
restored/LEGO-Chase-current-AOT-599pages-2026-10-03.tgz. No user re-upload needed.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
./build-gcc/LEGOChaseNative restored/code.bin
python -m unittest discover -s repo/tests -p test_generate_aot_pages_llvm.py -v
```

Use clang++ for Clang; omit LEGO_AOT_DIR for ROM-free tests. Launcher limits are
--block-limit and --host-event-limit; RTC option described above. No Windows
build, desktop rendering or playable release is claimed.

## Persistence and per-turn mandate

Private Library /LEGO-Chase-Recovery/ contains original six archive parts under
Game-archive/, code.bin, the unchanged 599-page archive and source checkpoints.
All six parts also remain in /mnt/data/ and backup-verify/. Scratch can reset;
Library backups and GitHub source are recovery paths, not permanent scratch.
See GAME-FILES-PERSISTENCE.md. No full CCI re-extraction was needed this turn.

Code SHA-256: 5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
CCI SHA-256: 3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525.

Use moderate tested checkpoints. Push source/reports, keep original/game-derived
bytes private, update this handoff and attach a downloadable copy every turn.
Inspect backups before requesting another upload. The delivery handoff adds the
final commit and private source/log archive after remote confirmation.
