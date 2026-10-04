# CTR synchronization/thread recovery checkpoint

This checkpoint extends the reconstructed CTR host layer beyond the historical
DuplicateHandle blocker. It is a ROM-free component checkpoint, not yet an
end-to-end game-run claim.

## Implemented kernel objects

### Events

Reset values follow the CTR/Citra/Azahar ordering:

- `0` — OneShot
- `1` — Sticky
- `2` — Pulse

Behavior reproduced:

- OneShot clears when acquired.
- Sticky remains signaled until explicitly cleared.
- Pulse wakes currently eligible waiters, then clears.

### Mutexes

- Recursive acquisition by the owning thread.
- Other threads wait while the mutex is owned.
- Final release makes the mutex available to the highest-priority eligible waiter.
- Release from a non-owning thread returns `0xD8E0041F`.

### Semaphores

- Available/max counts are retained.
- Acquisition consumes one available slot.
- Release reports the previous available count.
- A release that would exceed the maximum returns `0xD8E007FD`.

## Thread state

The runtime now models:

- Running
- Ready
- WaitSleep
- WaitSynchAny
- WaitSynchAll
- Dormant
- Dead

Created threads retain entry point, argument, stack top, priority, processor ID,
and a wait result/index. The current recovery scheduler chooses ready threads by
CTR priority order (lower numeric priority first) with thread ID as a deterministic
tie-break.

This does **not** yet switch complete A32 guest register/TLS contexts between
threads. It reconstructs the kernel state/scheduling decisions needed for that
next integration step.

## Wait behavior

`WaitSynchronization1` and `WaitSynchronizationN` now support:

- immediate acquisition
- zero-time timeout
- finite timeout
- infinite wait (negative timeout)
- wait-any index reporting
- wait-all acquisition
- signal-driven wakeup
- timeout-driven wakeup

A suspended SVC returns an A32 `ExitKind::Wait` with the guest PC already moved
to `pc + 4`. When the kernel wakes the thread, `SvcBridge::ApplyPendingWake`
updates the saved guest result register; wait-any also writes the ready index.

## SVCs now bridged

- `0x08` CreateThread
- `0x09` ExitThread
- `0x0A` SleepThread
- `0x13` CreateMutex
- `0x14` ReleaseMutex
- `0x15` CreateSemaphore
- `0x16` ReleaseSemaphore
- `0x17` CreateEvent
- `0x18` SignalEvent
- `0x19` ClearEvent
- `0x23` CloseHandle
- `0x24` WaitSynchronization1
- `0x25` WaitSynchronizationN
- `0x27` DuplicateHandle

The unusual modified CTR SVC ABI is preserved. In particular,
`WaitSynchronizationN` uses timeout low/high in `r0/r4`, handles address in
`r1`, count in `r2`, wait-all in `r3`, and returns the wait-any index in
`r1`.

## Result codes pinned in this slice

- Success: `0x00000000`
- Timeout: `0x09401BFE`
- Invalid pointer: `0xD8E007F6`
- Invalid handle: `0xD8E007F7`
- Out of handles: `0xD8600413`
- Out of range: `0xE0E01BFD`
- Kernel out of range: `0xD8E007FD`
- Invalid combination (kernel): `0xD90007EE`
- Wrong locking thread: `0xD8E0041F`

## Validation

Fresh local builds were performed from the reconstructed source through both
direct compiler invocation and CMake/CTest.

- Clang: PASS
- GCC: PASS
- CTest: 1/1 PASS on each compiler

The ROM-free suite covers handle generation, pseudo handles, all three event reset
modes, event signal/clear, wait-any/wait-all, finite timeouts, recursive mutexes,
wrong-owner mutex release, semaphore limits, basic thread creation/sleep,
WaitSynchronizationN guest-memory ABI, SVC wake-result application, and the
original Stage-2 DuplicateHandle shape.

## Next boundary

The next runtime checkpoint should attach a full A32 guest context/TLS record to
each ThreadObject and implement actual context selection/resume. After that, the
runner can execute generated pages across multiple guest threads instead of only
modeling their kernel states. Service-port/session IPC comes immediately after
that scheduler integration.
