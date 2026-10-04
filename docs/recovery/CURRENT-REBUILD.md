# LEGO Chase Begins — current reconstruction handoff

Updated October 4, 2026. This replaces the accumulated, partly stale checkpoint
summary; historical milestone reports remain under `reports/recovery-host/` and
in Git history. Read this file first when continuing.

## Current verified state

A real headless `LEGOChaseNative` executable now builds from the repository host
source plus the private restored AOT pages. Fresh Linux GCC 14.2 and Clang 17
builds both linked all 599 pages and executed the verified original USA code.
This is a startup diagnostic, not a playable release or Recovery J restoration.

The launcher verifies the 2,650,112-byte code image SHA-256 and checks every
stored generated instruction word against that image before dispatch. It checked
545,111 words in 111,043 registry blocks. These are static inventory counts, not
executed-block counts or proof of correct instruction semantics.

The saved page archive has **603 regular files**, including 599 page C++ files.
Do not force the historical 604-file claim onto this archive or manufacture missing
manifest files. The new top-level CMake build accepts the actual restored layout.

## Source reconciliation and changes

Base: `d726390ce406d724f87f8feebc0e23e4b0c0e6a8`.

The earlier private source archive contains a launcher and missing SVC routing.
Only the SVC bridge changes were brought forward into current GitHub source; the
old kernel, memory, vendor and broad service implementations were not substituted
for the newer committed versions. The launcher was reconstructed with a portable
CLI, revision verification, registry checks, explicit exit codes, and diagnostics.

Newly connected SVCs: ControlMemory (0x01), CreateAddressArbiter (0x21),
ArbitrateAddress (0x22), GetSystemTick (0x28), GetProcessId (0x35), and resource
limit queries (0x38–0x3A). Kernel backends already existed in the base snapshot.
No generated page or game image was changed. IPC diagnostics retain the request
before the handler overwrites it with a response.

## Fresh observed startup boundary

Before connecting those bridges, the real program stopped at SVC 0x21,
PC `0x00101D8C`, after one dispatch round.

After the changes, both compilers produce byte-identical startup diagnostics:

```text
stop=WaitingNoRunnableThread pc=0x0024b324 thread=1 dispatch_rounds=13
last_ipc_session=srv: requested_service=APT:U request_header=0x00050100
response=00050040 d0406401
```

The program has requested `APT:U` through srv:GetServiceHandle, received
ServiceNotRegistered, and subsequently blocked. It has NOT reached the title
screen. Host exit code 3 correctly marks this as a diagnostic stop.

Next: implement the observed `APT:U` service entry and its first request using
verified service semantics, then rerun this exact startup. Review the older
private service source as evidence only; do not promote fabricated event signals,
notification success, or broad stub responses to make startup appear successful.

## Validation

- GCC: all 599 private pages built and linked; 4/4 CTest suites passed.
- Clang: all 599 private pages built and linked; 4/4 CTest suites passed.
- A same-length wrong executable was rejected by SHA-256 before mapping/dispatch.
- SHA-256 tests include empty input, abc, a padding-boundary message and a million a's.
- New tests cover recovered SVC register routing and missing-service diagnostics.
- See `reports/recovery-host/NATIVE-STARTUP-2026-10-04.json` and the adjacent
  `NATIVE-STARTUP-*.txt` logs for exact measurements and fingerprints.
- Hosted CI now uses top-level CMake to run four ROM-free suites. Do not infer a
  hosted result from the local GCC/Clang runs; check Actions separately.

## Build and run

ROM-free components:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

Real private game tree:

```sh
cmake -S . -B build-game -DCMAKE_BUILD_TYPE=Release -DLEGO_AOT_DIR=/path/to/generated2
cmake --build build-game --parallel 4
./build-game/LEGOChaseNative /path/to/code.bin
```

Set `CMAKE_CXX_COMPILER=clang++` for the Clang build. The launcher accepts
`--block-limit N` and `--host-event-limit N`. No desktop/renderer, Windows build,
controller, audio, completed initialization or gameplay result is claimed.
GetSystemTick reads kernel guest time; dispatch-driven time advancement remains
unreconstructed. Other inherited kernel/IPC limitations remain open.

## Working data and durable recovery

Current scratch root: `/mnt/data/lego_recovery/`.
Source: `repo/`; builds: `build-gcc/` and `build-clang/`; pages: `generated2/`;
verified executable input: `restored/code.bin`; older evidence source:
`source-recovered-2026-10-03/`. Fresh build/test/run logs are in the scratch root.

The six original archive parts are still in `/mnt/data/` and in
`/mnt/data/lego_recovery/backup-verify/`. The large CCI was not re-extracted in this
turn because the verified code image was sufficient for startup execution.

Durable private Library folder: `/LEGO-Chase-Recovery/`. It contains the six
parts under `Game-archive/`, `code.bin`, the 599-page AOT archive, the older source
archive and the game-files recovery kit. See `GAME-FILES-PERSISTENCE.md` for the
restore procedure. Scratch can reset; the Library snapshots and GitHub source
are the recovery paths, not a claim that scratch is permanent.

Code SHA-256: `5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f`.
CCI SHA-256: `3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525`.

## Continuing mandate

Work in moderate tested checkpoints. Push source/report changes before ending
work; keep game images and generated game bytes private. Update this handoff and
attach a downloadable copy each turn. Never report old Recovery F/J gameplay or
component tests as fresh game execution. Inspect/restore backups before asking
for another upload.
