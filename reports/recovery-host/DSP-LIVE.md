# Live DSP load and retained audio interrupt event

October 6, 2026. Tested local source, not published. Remote parent is
`50b2942120f47d221cf66e662961165cf031bd20`. The prior DSP-ICU-PENDING source is
preserved; this checkpoint adds live-device integration rather than replacing it.

## Original-game result

With explicit `--dsp-executor live-teakra`, the original LoadComponent at round252
now completes. Its four startup mailbox words come from executing the original
firmware. The same interpreter and SRAM remain alive after boot. The 16 firmware-
written pipe descriptors are validated before publishing the device and mapping its
DATA bank at 0x1FF40000. Reply is `00110082 00000000 00000001 000c234a 0037a120`.

The game creates its own event and calls RegisterInterruptEvents(type2, pipe2,
handle0x000E802C) at round254. The service retains that same EventObject without
signaling it, allocating another handle, or advancing time. Reply is `00150040 0`.

The current next request is **GetSemaphoreEventHandle, header0x00160000**, untouched
at PC0x0025947C, round255. There are four threads and30 handles. ARM time is18447726ns,
core0 has issued2287210 recorded instructions, and core1 has issued ZERO. No visible
logo, frame, title screen, main menu, audio output, or gameplay is demonstrated.
All6MiB of device VRAM remains zero under the explicit reference-reset policy.

The first scheduled DSP deadline is18508160ns, so ZERO post-load scheduled slices
have executed on this original ARM startup path. Synthetic instruction programs
verify actual scheduled continuation during CPU-busy and idle execution. A separate
original-firmware continuation experiment reaches unmodeled MMIO write0x2A0 at DSP
PC0xB36 after9734 completed Run(1) calls, then seals the device. That is NOT the
original ARM next stop, not a completed scheduled slice, and not audio output.

## Implemented behavior and limits

The live device shares its DATA bytes, known-byte provenance and reservation epochs
with ARM memory. It does not map executable program SRAM, copy a snapshot as the
live bank, initialize unknown bytes implicitly, or expose the external AHB bus.
Unknown reads fail; real firmware and ARM/pipe writes update provenance distinctly.
Generic private-memory device-write helpers reject the device bank. Mapping and
object allocation precede the success reply; protected replies or conflicting
mappings preserve the request and existing guest storage.

Pipe operations validate all16 descriptors, capacity, pointers, direction, ranges,
and overlap. They move actual bytes, update only the CPU-owned pointer and send the
actual slot through mailbox2. Full/empty/busy conditions return WouldBlock without
changing output or pointers. The device methods are tested; guest pipe IPC wrappers
remain unsupported until their exact observed requests are implemented.

Ongoing scheduling follows the pinned LLE model: first deadline +16384 ARM ticks,
then +32768 ticks, with16384 bounded Teakra Run(1) calls per slice. Boot remains
synchronous and uncharged to ARM time. Counts and this scheduling model are NOT
measured hardware latency or cycle-accurate execution. Tie order is timeouts, quota,
DSP, display. Terminal faults retain partial DSP effects and stop before further ARM
execution; they are not silently retried or rolled back.

Actual mailbox0/1 writes produce notifications. Pipe notification requires both the
actual mailbox2 slot word and semaphore bit0x8000, not a guessed pipe response.
Registered real events are signaled only for emitted notifications after a completed
slice. Multiple same-channel notifications coalesce within that synchronous slice.
Debug-pipe draining, external-memory requests and audio output fail closed. Reload,
unload, semaphore notification, other DSP IPC and broader lifecycle remain unsupported.
Registration capacity6 is guarded by an explicit host stop on overflow, rather than
claiming firmware error precedence. Null unregister and event lifetime are supported.

`guarded-teakra` stays probe-only and `live-teakra` requires diagnostic-dual CPU mode.
Known-only firmware still stops at unknown SRAM after962 calls. Reference-zero-data
and special empty-config are separate explicit compatibility policies, not recovered
hardware state. HID sampling remains unimplemented. No CPU opcode, AOT, GPU rendering,
filesystem, configuration or original game-data change was made here.

## Validation

Full GCC and Clang native builds link all599 unchanged private AOT page units.
All56 CTest suites pass under each compiler. All56 ROM-free Clang ASan/UBSan suites
pass with leak checks/halt-on-error; bounded chunks are checked against the complete
configured test-name set, without duplicates or skipped suites. Earlier54 suites
remain intact. The new tests cover shared memory, unknown-byte access, reservations,
actual firmware writes and notifications, pipe wrap/full/empty, retained events,
protected replies, allocation rollback, scheduler resume and terminal faults.

Five ordinary paired startup cases match stdout, stderr, exit and owned test-file
bytes: live-reference, probe-reference, live-known, live-bounded100, executor-disabled.
Six paired CLI rejection cases also match and create no test state. All158 final
capture files match between GCC and Clang. Independent parsing checks the exact
load/registration replies, mapped DATA identity, original retained event, empty
unsignaled events, firmware-generated pipe table and untouched next IPC. Existing
GPU words/uploads, GSP/HID state, LCD and VRAM remain unchanged across those calls.
This is not an independent CPU oracle or a complete ARM address-space dump.

Original code.bin, full RomFS, ExHeader and all603 AOT archive members retain their
verified identities. Full IVFC validation was not repeated. No Windows/macOS build
or hosted CI was performed. Source/evidence is pending because write actions were
not exposed. The attached handoff describes recovery and the exact next work.

## Primary references

Azahar pin `86a9f9236ae42bb5a2b995dbc933d599d8ea07ac`:
`src/audio_core/lle/lle.cpp`, blob388fe64ec1a5130a2c93a5dfa04ca84df109b567,
for pipe ownership, boot and scheduling; `src/core/memory.h`,
blobf7045c1716c26a32c9764eebdc6a7fe6f7e3fe2f, for DSP address layout;
`src/core/hle/service/dsp/dsp_dsp.cpp`, blobf8b23c07925c6b4a9fe36acaa7954f05126441f5,
and `dsp_dsp.h`, blobbf7e94e0846e9a038c58c9eb904b013267fd6d3c, for registration,
real semaphore-event notifier and IPC contracts. The existing MIT Teakra pin and
prior local ICU patches are unchanged. No new hardware measurement is claimed.
