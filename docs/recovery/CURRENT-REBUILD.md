# LEGO Chase Begins — canonical recovery handoff

Updated October 4, 2026. Read this first. Historical Recovery F/J gameplay is not
current reconstruction proof. Work here, not on the user's PC or via Work handoff.

## Current real-game boundary

The headless `LEGOChaseNative` builds with all 599 private AOT pages and executes
the verified original USA code. `APT:GlanceParameter` now returns the queued empty
launch Wakeup without consuming it. The real program passes the prior thread-2
stop and reaches a new missing-service boundary:

```text
stop=UnsupportedSvc pc=0x0011fb80 detail=0x0000003c thread=1 dispatch_rounds=35
last_ipc_session=srv: requested_service=ndm:u request_header=0x00050100
ipc_words=00050040 d0406401 ...
host_exit_code=3
```

The game requested `ndm:u`, received ServiceNotRegistered, and subsequently called
SVC Break (0x3C). Do not bypass that break or assume startup succeeded. No title
screen, completed initializer count, graphics, sound, controls or gameplay claim.

Base commit: `2dd9b1ae694799caede9eb17daae7b6c9dd56038`.
The restored source tree matched `787bd5832f6bdc30866eb26a6a5c93dfcdbb042d` exactly
before editing. The initial real-game Glance stop was reproduced first.

## This checkpoint

Changed only `src/services/apt_service.*`, its tests, reports and this handoff.
No vendor code, generated game page or original executable was changed.

Glance peeks at the actual queued launch parameter. ReceiveParameter implements
the paired consume-once operation and returns service NoData on later reads.
Receive is component-tested; its execution by the game is NOT claimed here.
No event is re-signaled to manufacture progress and event acquisition remains
separate from message consumption. Only the existing empty Wakeup is supported.

Static output descriptor 0 is read from TLS+0x180, outside the TLS+0x80 command
buffer. Descriptor type/id, capacity, full destination writability and address
wrap are checked before any write or queue change. Requests are capped at 0x1000;
no host allocation uses a guest size. Failed output leaves the pending message
and command intact. Guest writes invalidate exclusive reservations. Zero-size
requests dereference no destination bytes.

### Explicit wire-policy boundary

3dbrew documents a copy descriptor for Glance and a move descriptor for Receive.
Pinned Azahar emits move for both. This reconstruction uses the documented split;
the launch object is null, so no real object-transfer ownership is claimed.
Output is zero-padded to the requested size, as in pinned Azahar; response payload
size remains zero. 3dbrew instead labels the static descriptor size as actual
payload size. This discrepancy is documented, not claimed resolved on hardware.
See `reports/recovery-host/APT-PARAMETER-2026-10-04.json` for primary references.

## Validation and evidence

- Fresh full GCC and Clang builds include all 599 private page translation units.
- Both real-game startup logs are byte-identical at the ndm:u/Break boundary.
- GCC 5/5 CTest suites pass; Clang 5/5 pass.
- Separate Clang ASan/UBSan: 5/5 ROM-free suites pass with leak checking enabled.
- Tests cover repeat peek, consume once, NoData, event non-resignaling, buffer
  guards, short/wrong descriptors, read-only and partly unmapped outputs, address
  wrap, absent descriptor table, zero-length and oversized requests, command-buffer
  protection, retained queue on failure, and exclusive-reservation invalidation.
- All 603 regular members of the restored AOT archive remain byte-identical.
  599 are page C++ files. Do not manufacture a 604th file to match older reports.
- 545,111 instruction words / 111,043 registry blocks are static counts, not
  executed-block counts or proof of complete ARM semantics.
- New logs are `reports/recovery-host/PARAMETER-*.txt`; the JSON report records
  actual file and executable hashes. Hosted CI must be checked after the push.

## Next exact step

Register only the observed `ndm:u` service, retaining strict unsupported-command
stops. Run the unchanged game to discover its first NDM request and implement
that request from pinned primary service code. Do not import the old private
service implementation wholesale. Leave unknown commands, applet roles, nonempty
parameters, attached objects and arbitrary missing services explicit.

## Working files / rebuild

Scratch: `/mnt/data/lego_recovery/`.
Source: `repo/`; real builds: `build-gcc/`, `build-clang/`; sanitizer components:
`build-asan/`; private pages: `generated2/`; verified input: `restored/code.bin`.
This turn's complete configure/build/test/run logs: `parameter-checkpoint/`.
Original six game archive parts remain in `/mnt/data/` and `backup-verify/`.
No CCI re-extraction was needed for this startup test; inspect before claiming
an extracted CCI is currently present.

```sh
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
./build-gcc/LEGOChaseNative restored/code.bin
```

Omit LEGO_AOT_DIR for ROM-free component tests. Use clang++ for Clang. Launcher
limits are `--block-limit` and `--host-event-limit`; no forced successful boot.
GetSystemTick uses kernel guest time; dispatch-driven time is still unreconstructed.

## Durable recovery / mandate

Private Library `/LEGO-Chase-Recovery/` holds the six parts in `Game-archive/`,
`code.bin`, `LEGO-Chase-current-AOT-599pages-2026-10-03.tgz`, prior source checkpoints
and the restore kit. See `GAME-FILES-PERSISTENCE.md`. Restore those before asking
for uploads. Scratch can reset; Library plus GitHub are the durable recovery paths.

Code SHA-256: `5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f`.
CCI SHA-256: `3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525`.

Use moderate tested checkpoints, push source and evidence before ending, keep all
game bytes/private AOT pages out of the public repository. Update this canonical
handoff and post a downloadable copy every turn. Source/log archive and delivery
commit/CI confirmation are recorded in the attached copy after the push.
