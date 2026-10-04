# CTR handle/SVC recovery checkpoint

This checkpoint reconstructs the first native host/kernel slice needed to clear
the original Stage-2 startup stop.

## Reconstructed behavior

The implementation follows the CTR handle layout used by Citra/Azahar lineage:

- Maximum process handle table size: **4096**.
- Handle slot: bits 31:15.
- Handle generation: bits 14:0.
- Generation zero is not allocated.
- Freed slots are reused through the free-slot list while a new generation keeps
  stale handles invalid.
- Current-thread pseudo handle: `0xFFFF8000`.
- Current-process pseudo handle: `0xFFFF8001`.
- Duplicating a pseudo handle creates an ordinary table handle referencing the
  same kernel object.
- Closing pseudo handles is rejected as an invalid ordinary table handle.

Result codes currently pinned:

- Success: `0x00000000`
- Invalid handle: `0xD8E007F7`
- Out of handles: `0xD8600413`

## SVC bridge

The bridge currently implements:

- `svc 0x27` — `DuplicateHandle`
- `svc 0x23` — `CloseHandle`

For `DuplicateHandle(Handle* out, Handle handle)`, CTR's modified SVC ABI uses:

- input handle in `r1`
- result in `r0`
- duplicated output handle in `r1`

A handled SVC resumes native A32 execution at `pc + 4`. Unknown SVCs remain
explicit `ExitKind::Svc` stops rather than being reported as successful.

## Stage-2 blocker reproduction

The ROM-free component test reproduces the surviving Stage-2 stop shape:

- SVC PC: `0x0013313C`
- SVC number: `0x27`
- input `r1 = 0xFFFF8000`

The new bridge returns success, places a valid regular handle to the current thread
in `r1`, and resumes at `0x00133140`.

This clears the *known host-semantic blocker*. It does **not** yet claim that the
current reconstructed game executable runs through this point end-to-end.

## Validation

Fresh local CMake builds passed with both:

- Clang
- GCC

Both run the same ROM-free handle/SVC test successfully. The tests cover pseudo
handles, object identity, generation-protected stale handles, 4096-handle
exhaustion, DuplicateHandle register ABI, CloseHandle, invalid handles, and
unsupported-SVC preservation.

Next host work should add the synchronization/event/thread SVCs needed after the
DuplicateHandle checkpoint before beginning service IPC reconstruction.
