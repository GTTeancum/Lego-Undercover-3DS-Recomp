# LEGO Chase Begins — canonical recovery handoff

Updated October 4, 2026. Read this first. Continue in this environment, not on the
user's PC or through Work. Historical Recovery F/J gameplay is not current proof.

## Current real-game boundary

The headless LEGOChaseNative builds with all 599 private AOT pages and runs the
verified original USA code. This checkpoint implements the observed application
APT:Enable request: header 0x00030040, attributes 0. The real game passes it and
next issues APT:NotifyToWait: header 0x00430040, application ID 0x300.

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=44
last_ipc_session=APT:U requested_service= request_header=0x00430040
ipc_words=00430040 00000300 00000010 00048011 ...
host_exit_code=3
```

The NotifyToWait request remains untouched. No response or event is fabricated.
No title screen, completed initializer count, renderer, audio, controls or gameplay
is established. The five additional dispatch rounds are startup progress, not
five frames or a completion metric.

## This checkpoint

Base commit: 26dcd3e33eb3893eae368c9515e1d8a1b1dc4871.
The restored source/index matched remote tree
f40fde1f61a5a1de5d8cdb9fdaa85655c802fa3c before editing. The original APT:Enable
stop was reproduced byte-for-byte before changing code.

Source commit: 9176d13c7ee595c5f404f168baed821e8a37adb7.
Tested source tree: e27ffb5fb837b9dbea99d03691bc2cc839217d5c.
Changes are limited to src/services/apt_service.h/.cpp, a new focused
ctr_apt_enable_test.cpp, CMake registration, and checkpoint reports/handoff.
No kernel, IPC router, NDM, vendor or generated-page source was modified.

APT now retains explicit application registration state. First Initialize sets
registered=true only after both output event handles are allocated. Enable for
the already initialized application also sets registered=true, returning the
normal one-result reply. This follows the pinned AppletManager first-application
path; it is not a complete multi-applet manager implementation.

Repeated Enable calls do not create a new launch Wakeup, consume the pending
parameter, signal/clear an event, allocate handles, create threads or advance
guest time. Closing/reopening an APT session preserves the shared registration
and message state. After ReceiveParameter consumes the message, another Enable
leaves it absent. Other attributes, malformed headers and Enable before Initialize
remain explicit host stops, not guessed hardware error replies.

## Validation

- Full GCC and Clang builds linked all 599 private page translation units.
- Final real-game logs are byte-identical at NotifyToWait.
- GCC 7/7 CTest and Clang 7/7 CTest passed.
- Clang ASan/UBSan 7/7 ROM-free CTest passed; leak checking and halt-on-error enabled.
- GitHub Actions 37244507645 succeeded on source commit 9176d13.
- New tests cover repeated Enable before/after event acquisition, after message
  consumption, across sessions, exact replies/register preservation, malformed
  and premature requests, protected/partially writable IPC responses, unchanged
  messages, handles, thread state and guest time. Existing suites still pass.
- All 603 regular private AOT archive members remain byte-identical; 599 are page
  C++ files. Do not manufacture a 604th file to match older historical reports.
- 545,111 verified raw instruction words and 111,043 registry blocks are static
  inventory counts, not executed-block counts or proof of full ARM semantics.
- See reports/recovery-host/APT-ENABLE-2026-10-04.json and ENABLE-*.txt for
  baseline, final runs, test outputs, fingerprints and source references.

## Next exact step

Inspect and implement only the observed APT:NotifyToWait request
(0x00430040, applet 0x300), then rerun unchanged game code to identify the next
unsupported request. Review the pinned handler and any relevant applet-state
semantics first. Do not invent notification completion, queue a replacement
launch message, consume one prematurely or wake a thread merely to advance.
The command mapping is confirmed in pinned apt_u.cpp. NotifyToWait itself was
not implemented or bypassed this turn.

Behavior references: azahar-emu/azahar commit
86a9f9236ae42bb5a2b995dbc933d599d8ea07ac,
src/core/hle/service/apt/applet_manager.cpp (Initialize and Enable, lines 400–470),
and src/core/hle/service/apt/apt_u.cpp (NotifyToWait mapping).

Preserve the prior limitations: GlanceParameter peeks the empty launch Wakeup;
ReceiveParameter is component-tested consume-once behavior, not newly claimed
observed in this real run. Static output slot 0 is TLS+0x180; command is TLS+0x80.
The Glance copy / Receive move descriptor policy follows 3dbrew while requested
size padding follows pinned Azahar; hardware wire parity remains unresolved.
NDM OverrideDefaultDaemons/SuspendDaemons remain pinned HLE state bookkeeping,
not network execution. Dispatch-driven guest time and scheduler limitations remain.

## Working files and rebuild

Scratch root: /mnt/data/lego_recovery/.
Source: repo/; real builds: build-gcc/ and build-clang/; sanitizer: build-asan/;
private pages: generated2/; verified executable: restored/code.bin.
This checkpoint's complete configure/build/test/run logs: enable-checkpoint/.
Source was restored from LEGO-Chase-source-checkpoint-26dcd3e.tgz; pages and
code.bin were already present as private restored files. No user reupload needed.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
./build-gcc/LEGOChaseNative restored/code.bin
```

Omit LEGO_AOT_DIR for ROM-free builds; use clang++ for Clang. Launcher limits:
--block-limit and --host-event-limit. GetSystemTick uses kernel guest time;
dispatch-driven time progression is not reconstructed. No Windows build claimed.

## Durable recovery and mandate

Private Library /LEGO-Chase-Recovery/ contains original six parts under
Game-archive/, code.bin, LEGO-Chase-current-AOT-599pages-2026-10-03.tgz, source
checkpoints and restore tools. The six parts also remain in /mnt/data/ and
backup-verify/. No large CCI re-extraction was needed for this startup work.
See GAME-FILES-PERSISTENCE.md for recovery. Scratch may reset; Library snapshots
and GitHub source, not permanent scratch, protect the project.

Code SHA-256: 5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
CCI SHA-256: 3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525.

Use moderate tested checkpoints. Push source/reports before ending, keep game
bytes and derived AOT pages private, update this canonical handoff and attach a
downloadable copy every turn. Inspect backups before requesting another upload.
Local Git is a verified snapshot/index, not a full remote-history clone. The
attached handoff records final delivery and the private source/log archive.
