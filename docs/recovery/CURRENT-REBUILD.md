# LEGO Chase Begins — canonical recovery handoff

Updated October 4, 2026. Read this first; historical reports are not current gameplay proof.

## Current verified boundary

The headless LEGOChaseNative builds and executes the verified USA game code with
all 599 private AOT pages. This checkpoint implements the first two observed
APT:U requests, not a complete applet manager or a playable release.

Base commit: `33d84803304e9310e158cf6865daf72b65f5ca13`.
The restored source tree was verified against base tree
`a7fb9eb472d7ffb7a19bd512798850973f51b939` before editing.

The original missing APT:U stop was reproduced first. Controlled diagnostic runs
then observed this sequence directly from the game, without patching its code:

1. GetLockHandle: header `0x00010040`, application attributes `0`.
2. Initialize: header `0x00020080`, applet ID `0x300`, attributes `0`.
3. A second guest thread reaches GlanceParameter: header `0x000E0080`,
   applet ID `0x300`, requested parameter buffer size `0x1000`.

The first two requests now work. The third is intentionally an explicit host
stop with the original request/registers preserved; no invented response:

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=2 dispatch_rounds=29
last_ipc_session=APT:U request_header=0x000e0080
TLS=0x1ff82200
static_buffer_descriptor=0x04000002 static_buffer_address=0x0036c000
host_exit_code=3
```

Both GCC and Clang produce byte-identical final startup logs. No title screen,
completed initializer count, renderer, audio, input or gameplay is claimed.
The 545,111 instruction words / 111,043 blocks are static inventory counts.

## Implemented in this checkpoint

- `src/services/apt_service.*`: shared real mutex with copied handles for
  GetLockHandle; first application Initialize with two one-shot events.
- Initialization queues a Wakeup parameter (sender None, destination 0x300,
  signal 1) and signals ONLY its parameter event. Notification remains unsignaled.
  This follows the pinned manager's first-application launch behavior. The
  parameter remains queued; acquiring its event is not receiving the message.
- APT state survives closing/reopening client sessions and copied handles.
- Unknown/malformed APT requests, other applet roles and duplicate initialization
  remain unsupported host stops, not pretend successes or invented error replies.
- IPC now uses `optional<Result>`: nullopt means host implementation missing.
  The SVC bridge leaves guest state untouched and the runner reports UnsupportedIpc.
- Full command-buffer writability is checked before handlers mutate state or
  allocate handles. Partial event-handle allocation rolls back on exhaustion.

Behavior references are pinned Azahar `86a9f9236ae42bb5a2b995dbc933d599d8ea07ac`,
`apt.cpp` GetLockHandle/Initialize and `applet_manager.cpp`
GetLockHandle/Initialize/SendParameter. See the report for source links.
No generated game page, ARM lowering, guest executable or vendor file was changed.

## Validation

- Fresh full 599-page GCC and Clang native builds/run: PASS (diagnostic exit 3).
- GCC: 5/5 CTest suites pass; Clang: 5/5 pass.
- Separate Clang ASan/UBSan, leak checking enabled: 5/5 ROM-free suites pass.
- Added tests exercise mutex identity/ownership, actual queued launch-event
  behavior, unsupported-request preservation, handle-exhaustion rollback,
  read-only command-buffer rejection and runner UnsupportedIpc reporting.
- `reports/recovery-host/APT-STARTUP-2026-10-04.json` records fingerprints,
  observed requests, limits and build scope; adjacent APT-*.txt files are logs.
- Hosted CI result must be checked separately after pushing this checkpoint.

## Next exact step

Implement the observed GlanceParameter and subsequent ReceiveParameter sequence
against the real queued launch message. The static receive descriptor is at
TLS+0x180, separate from the 64-word IPC command buffer at TLS+0x80.
Inspect/validate the full destination before touching it. The pinned APT handler
pads its output static buffer to the requested size, even for an empty parameter;
3dbrew documents different details for the Glance handle descriptor. Resolve that
boundary explicitly, retain request/response logs and test message consumption.
Do not simply signal more events, always return Wakeup, or manufacture success
for later APT commands. Continue from observed guest requests only.

## Build and run

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure

cmake -S . -B build-game -DCMAKE_BUILD_TYPE=Release -DLEGO_AOT_DIR=/path/to/generated2
cmake --build build-game --parallel 4
./build-game/LEGOChaseNative /path/to/code.bin
```

Set `CMAKE_CXX_COMPILER=clang++` for Clang. CLI limits are `--block-limit N` and
`--host-event-limit N`. The executable validates code SHA-256 and every generated
instruction word; that does not prove complete/correct instruction semantics.
The private page archive has 603 regular files (599 page units), not the historic
604-file layout. Do not manufacture missing artifacts to match a count.

## Files and durable recovery

Scratch root `/mnt/data/lego_recovery/`:
- Source: `repo/`; current native builds: `build-gcc/`, `build-clang/`.
- Sanitizer components: `build-asan/`.
- Private pages: `generated2/`; executable input: `restored/code.bin`.
- Original six game archive parts: `/mnt/data/` and `backup-verify/`.
- Build/run logs are in the scratch root and copied into the source report folder.

The source/page archives already present were restored this turn; no new game
upload was required. The 1 GiB CCI was not re-extracted because code.bin suffices
for this startup boundary. Do not claim it is present without inspecting.

Private Library `/LEGO-Chase-Recovery/` retains `Game-archive/` (all six parts),
`code.bin`, `LEGO-Chase-current-AOT-599pages-2026-10-03.tgz`, earlier source
checkpoints and the restore kit. See `GAME-FILES-PERSISTENCE.md`.
Scratch may reset; Library plus GitHub source are the durable recovery paths.

Code SHA-256: `5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f`.
CCI SHA-256: `3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525`.

## Continuing mandate

Work here, not on the user's PC or through a Work handoff. Use moderate tested
checkpoints and push before ending. Keep game bytes/generated pages private.
Update this handoff and attach a downloadable copy every turn. Preserve logs and
state exact test scope. Inspect/restore backups before requesting uploads.
