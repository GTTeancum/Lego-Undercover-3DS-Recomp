# CTR guest-context switch checkpoint

This checkpoint moves the recovered kernel from thread-state modeling to actual
saved A32 guest contexts. It remains a component/runtime checkpoint; it is not
yet a complete LEGOChaseNative game runner.

## Per-thread guest state

Every `ThreadObject` now owns a full pinned-TriAevum `a32::GuestState`:

- r0-r15
- CPSR
- FPSCR
- VFP S0-S31 backing lanes
- TPIDRURW / thread pointer
- exclusive address/token/size/valid state

A context switch saves the complete live state into the outgoing thread and
restores the selected thread's saved state.

## CTR thread initialization

New SVC-created threads use the recovered CTR initialization rules:

- `r0 = argument`
- `r13 = stack_top`
- `r15 = entry_point & ~1`
- CPSR user mode = `0x10`
- CPSR T bit follows entry-point bit 0
- FPSCR = `0x03C00000`

The reconstructed main-thread baseline uses FPSCR `0x03C00010`, matching the
surviving Citra/Azahar setup path for an application main thread.

## TLS / TPIDRURW

CTR TLS geometry is now represented directly:

- TLS area base: `0x1FF82000`
- TLS entry size: `0x200`
- eight thread slots per 4-KiB page

The initial thread receives `0x1FF82000`, the next receives `0x1FF82200`,
and so on. The selected thread's TLS address is always restored into
`GuestState::thread_pointer`, which is the value consumed by the existing A32
TPIDRURW implementation.

The full guest-memory runner still needs to map and zero the corresponding TLS
pages/entries. This checkpoint establishes the addresses and CPU-visible thread
pointer, not the final memory allocator.

## Rescheduling

`Kernel::Reschedule` now:

1. saves the current live A32 state;
2. preserves the outgoing thread's own TLS pointer;
3. moves a still-running thread back to Ready;
4. selects the highest-priority ready thread;
5. changes the current-thread pseudo-handle target;
6. applies any pending synchronization result to the selected thread's saved
   r0/r1;
7. restores the selected A32 state.

The scheduler continues to use lower numeric CTR priority first with thread ID
as a deterministic tie-break.

## Blocked-SVC resume

A ROM-free test now performs the complete component sequence:

1. switch from main thread to a higher-priority child;
2. child executes `WaitSynchronization1` and receives an A32 Wait exit at
   `pc + 4`;
3. scheduler saves the blocked child's registers and restores the main thread;
4. main thread signals the child's event;
5. scheduler selects the child again;
6. the restored child context contains ResultSuccess in r0 and resumes at the
   post-SVC PC.

This tests the path the future game runner will use instead of merely testing
kernel object state.

## Context preservation test

The switch test verifies preservation/restoration of:

- general registers
- stack/program counter
- CPSR
- FPSCR
- VFP lanes
- exclusive monitor fields
- per-thread TPIDRURW/TLS pointer

It also verifies ARM/Thumb entry-point initialization and current-thread
pseudo-handle rebinding.

## Continuous integration

A repository runtime CI workflow was added at
`.github/workflows/ctr-runtime-ci.yml`.

GitHub Actions run **37165248493** built and ran the committed test tree with:

- GCC: PASS
- Clang: PASS

The CI compiles the recovered CTR runtime together with the pinned TriAevum A32
runtime/core/VFP sources, so it catches ABI and integration errors rather than
only header-level unit-test problems.

## Next boundary

The next useful runner work is:

1. map/zero CTR TLS pages in the reconstructed guest memory system;
2. add a small dispatch/scheduler loop that calls the 599-page registry,
   routes SVC exits through `SvcBridge`, and reschedules on Wait/ExitThread;
3. then reconstruct service port/session IPC.

That will be the first point where the recovered generated code and recovered
multi-thread kernel can be exercised together as a native executable skeleton.
