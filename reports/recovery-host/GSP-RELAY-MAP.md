# GSP relay registration and service-backed shared mapping

Checkpoint: October 5, 2026. Implementation
`a9d63b67bafbbc2687aded4af13cb0a302366dfc`, tested/uploaded tree
`6605a090a5d701d54bd1554f46eb4bef67fe9221`.

## Observed original-game sequence

The restored d6317ef source tree matched 200fa465a7008be39faeac5bbc6bb357d11d116b;
627 private checkpoint files and 249 source-index entries verified. The baseline
RegisterInterruptRelayQueue stop was reproduced before edits. No game code or
private AOT changes were made.

Registration now returns header 0x00130082, first result 0x00002A07, allocated slot 0,
copy descriptor zero and actual shared-object handle 0x0007801D. It retains the
actual input EventObject 0x0007001C without signaling or resetting it. Registration
alone first exposed SVC MapMemoryBlock at round 95. The original request selected
address 0x10000000, read/write permissions 3 and peer DontCare 0x10000000.

After implementing that observed SVC, the original code maps the service's same
4096-byte object, then stops at round 98:

```text
last_ipc_session=gsp::Gpu request_header=0x00010082
request=00010082 00401000 00000004 00010002 0ffff608
```

WriteHWRegs requests four bytes at relative register offset 0x00401000. The source
word at 0x0FFFF608 is zero. This request remains untouched. Neither the register's
purpose nor hardware side effects have been implemented. No GPU command, interrupt,
vblank, rendering, main menu or gameplay is claimed.

The logging-only trace confirms actual backing identity, successful map, byte
coherence and an unsignaled event. The page remains zero at the stop; SHA-256 is
ad7facb2586fc6e966c004d7d1d16b024f5805ff7cb47c7a85dabd8b48892ca7. Kernel time is zero.
Production GCC/Clang binaries use their normal IPC/SVC objects and independently
reach the same boundary; alternative trace objects only add diagnostics.

All seven original RomFS metadata reads still match the verified image: 4892 bytes
at offsets/sizes 0/40 three times, 40/12, 52/68, 120/212 and 332/4480. Guest-written
20-byte gamecoin initialization remains intact. Existing-file startup reaches the
new stop at round 90 but skips that initialization; it is not gamecoin readback or
gameplay save/load. Prior missing-input and unconfigured-PTM stops remain separate.

## Supported state and lifetime

GSP has four connection-owned numeric slots, allocated first-free. Duplicated kernel
handles retain the same session identity; final identity destruction releases the
slot, stored event and held ownership. Exhaustion returns 0xD0401834. Failed kernel
handle insertion releases the tentative slot. The generic IPC session factory now
has an explicit Result, preventing errors from falling back to the root endpoint.
FS_USER changes are only the matching factory signature, not file behavior.

Registration validates exact request shape, flags 0/1, copy descriptor, actual event
and writable response. Kernel output-handle allocation precedes state changes.
First successful registration, on any slot, returns 0x2A07; later calls return zero.
Re-registration does not zero shared bytes or change event signal state. Flags are
retained; VRAM-save functionality is not supplied by accepting flag 1.

The shared object owns its bytes and reservation epochs. Mappings retain the object
and refer to those same bytes; no detached zero-filled copies are installed. Writes
via aliases, host service or load helpers invalidate the backing's same 8-byte
reservation granule. Unrelated granules are independent. Mapping ownership outlives
closed handles. These operations are used by a single-host-thread runner; no new
host-thread synchronization, emulated GPU work or guest-time advancement is implied.

The MapMemoryBlock slice accepts only actual bounded service pages, explicit aligned
addresses and nonzero subsets of read/write permissions, with peer DontCare. It
checks address ranges and refuses overlapping mappings without changing existing
bytes. The pinned exclusive end condition is retained: end must be below 0x14000000.
Address zero, misalignment and unsupported object models are not silently adopted.
The fixed 4 KiB service model does not implement physical BASE allocations, resource
accounting, automatic placement, guest CreateMemoryBlock, Unmap, generalized shared
objects or cache/timing parity. No Windows/macOS build is claimed.

## Validation

Full GCC and Clang builds link all 599 private AOT page units. Final CTest results:
GCC 23/23, Clang 23/23, Clang ASan/UBSan 23/23 ROM-free suites, leak checking and
halt-on-error enabled. Final validation driver records zero for every build/test/
scenario/trace step. New tests cover nonzero first slot, repeated registration,
copy lifetimes, slot/handle exhaustion and rollback, malformed/protected requests,
shared alias coherence, permissions, overlaps, bounds, final ownership and exclusive
epochs. No native failure was suppressed.

Eleven production scenarios have byte-identical compiler logs: fresh, existing,
PTM-off, alternate RTC, no RomFS, no root, missing archive, invalid root, invalid
mode, missing RomFS, wrong RomFS size. Each matrix uses its own newly created state;
only exact expected files in that root are reset for a same-path compiler comparison.
Historical state and unknown console saves are never deleted or overwritten.

All 603 regular AOT archive members match; 599 are page C++ files. Static inventory
111043 registry blocks / 545111 raw words is not an execution/frame count. Raw
RomFS again passes its full SHA and three IVFC levels (187787 hash blocks).

RELAY-PROOF.json contains original-read hashes and actual registration/map/input
diagnostics. RELAY-VALIDATION.json contains the scenario list and binary fingerprints.
RELAY-FRESH-GCC.txt / RELAY-EXISTING-GCC.txt are ordinary production logs. Complete
private scripts, trace source and tests are in relay-checkpoint/ of the private
backup; trace binaries/objects and game bytes are excluded from public source.

## Primary inspected reference

azahar-emu/azahar pinned to 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac:
- src/core/hle/service/gsp/gsp_gpu.cpp: first initialization, four slots,
  registration, copied event/page, constructor and session release.
- src/core/hle/service/gsp/gsp_gpu.h: session fields and flag definitions.
- src/core/hle/kernel/shared_memory.cpp: real backing, initialization, Map rules.
- src/core/hle/kernel/svc.cpp: MapMemoryBlock handler (lines 590-785).
- src/core/hle/kernel/errors.h: MaxConnectionsReached and mapping errors.
- src/core/memory.h: heap/shared ranges (lines 190-285).

These are reference-HLE behaviors and bounded host policies, not complete hardware
verification. Next work is the actual WriteHWRegs parser and target register state/
side effects. Do not return dummy success or invent interrupts to force progress.

## Hosted CI confirmation

GitHub Actions `37323609481` on implementation `a9d63b6` completed successfully
for both GCC and Clang jobs. Hosted tests are ROM-free, not original-game execution.
