# Mapped guest-memory and AOT runner checkpoint

This checkpoint joins the recovered A32 execution/runtime with the reconstructed
CTR kernel for the first time. It is still an executable skeleton checkpoint,
not a claim that the full LEGO game currently boots.

## Guest memory

`src/runtime/ctr_memory.*` now implements a mapped little-endian
`a32::MemoryBus` with:

- read/write/execute permissions
- 8/16/32/64-bit accesses
- precise rejection of writes to read-only text/rodata
- exclusive-load/store reservation tracking
- atomic swap
- zeroed mapped regions

The verified LEGO executable layout is encoded directly:

- text: `0x00100000`, allocated `0x257000`
- rodata: `0x00357000`, allocated `0x14000`
- data: `0x0036B000`
- BSS begins: `0x00386670`
- BSS ends: `0x005A45D8`
- prepared code image: `0x287000` bytes
- main stack: `0x0FFF0000-0x10000000`
- TLS: `0x1FF82000`, `0x200` per thread

`LoadLegoCodeImage` maps the verified prepared `code.bin` layout, copies the
allocated text/rodata/data images, and then zeros the documented BSS range.

TLS pages are mapped on demand from the kernel's real per-thread TLS addresses.

## Native runner

`src/runtime/ctr_runner.*` now joins:

- the generated AOT `Registry`
- TriAevum `Dispatch`
- the mapped guest-memory bus
- `SvcBridge`
- the CTR thread scheduler/context switcher

The runner:

1. initializes the main guest context and stack;
2. dispatches translated A32 blocks;
3. routes `ExitKind::Svc` through the recovered CTR SVC bridge;
4. maps TLS for newly created guest threads;
5. reschedules on blocked waits and thread exit;
6. permits SVC-created/higher-priority ready threads to preempt at SVC boundaries;
7. stops explicitly on unknown SVCs, missing blocks, memory faults, unsupported
   exits, or block/host-event limits.

No unknown SVC or missing service is silently treated as success.

## End-to-end synthetic scheduler test

The runner test uses a real A32 generated-registry shape rather than directly
calling kernel methods.

The synthetic program performs:

1. main guest thread executes `WaitSynchronization1`;
2. A32 dispatch returns an SVC exit;
3. the SVC bridge blocks the main thread at the post-SVC PC;
4. the runner saves the main A32 context and schedules a child thread;
5. child executes `SignalEvent` through A32 dispatch/SVC routing;
6. child executes `ExitThread`;
7. runner restores the main thread;
8. the pending wait result is restored as ResultSuccess;
9. main executes `ExitThread`;
10. runner reports the process has no runnable/live guest threads.

This validates the dispatch -> SVC -> kernel wait -> context switch -> signal ->
context restore -> A32 resume path as one component.

A separate test verifies that an unsupported SVC remains an explicit
`UnsupportedSvc` runner stop.

## Memory validation

The test suite also verifies:

- text and rodata are read-only
- data is writable
- the BSS is zeroed even if the prepared data-allocation padding contains
  non-zero bytes
- the main stack is mapped writable
- TLS expands onto a second 4-KiB page after eight slots
- an intervening write invalidates the relevant exclusive reservation
- a clean exclusive load/store succeeds

## CI validation

GitHub Actions run **37165798433** compiled and ran the committed tree with:

- GCC: PASS
- Clang: PASS

Each compiler passed both:

- `ctr_kernel_test`
- `ctr_runner_test`

The CI build links the reconstructed runtime against the pinned TriAevum
A32 core/runtime/VFP implementation.

## Current boundary

The recovered scratch still contains the real current generated AOT tree:

- 599 pages
- 545,111 reachable slots
- 111,043 emitted blocks
- zero explicitly unsupported generated operations

Those generated game pages are not committed to the public repository. The repo
now has the memory and execution loop needed to link such a tree into a native
executable skeleton.

The next major runtime layer is CTR named ports/sessions and command-buffer IPC,
followed by the game-specific services required by startup.
