# DSP semaphore notification and observed audio-startup pipe

October 6, 2026. This checkpoint continues the exact DSP-LIVE-PENDING source tree
`d3b4ec4a1d9ede5f72aba7ba812c66b439b20e5f` over published parent
`50b2942120f47d221cf66e662961165cf031bd20`. It preserves the pending ICU and live
DSP work. Publication and hosted checks are recorded in the delivery receipt.

## Original-game progress

The real DSP firmware still generates its boot replies and pipe descriptors. The
original ARM code now obtains a retained OneShot semaphore event at dispatch255,
presets it to0x2000 at256, writes its four-byte audio-startup message to pipe2 at257,
and calls direct SetSemaphore(0x4000) at258. The successful replies are0x00160042,
0x00170040,0x000D0040 and0x00070040, all with Result0. Export allocates one copied
handle,0x000F002D. No signal is generated merely by export or preset.

The pipe write uses the firmware-created slot5 and existing payload storage. It
normalizes the last two bytes only in the service-local copy, as the pinned DSP
service does; the guest source remains untouched. The CPU-owned write pointer
advances from0 to4, and the real mailbox2 receives slot5. Independent parsing
reproduces every SRAM and provenance change. Nonzero synthetic tests verify byte
movement independently of the original startup message's normalized zero payload.

The game then waits on its audio interrupt event. Its first scheduled DSP slice
actually begins on this original ARM path, unlike the predecessor's isolated
continuation experiment. It stops at an unsupported MMIO WRITE0x2A0, DSP PC0xB36.
The firmware's descriptor read pointer remains0: the four bytes are queued, not
acknowledged or consumed at this stop. There is no completed scheduled slice.

```
stop=UnsupportedDspEvent pc=0x002594c4 thread=1 dispatch_rounds=259
guest_now_ns=18508160 cpu_ticks=4962257
core0_instructions=2287384 core1_instructions=0
DSP completed Run(1) calls=10215 attempted=10216 firmware word writes=9216
```

Both audio and semaphore events remain unsignaled. The original code has NOT yet
signaled the exported semaphore-event handle; actual notifier-to-DSP delivery is
verified using synthetic executable DSP programs. The direct semaphore request
and the new scheduled attempt are observed original-game operations.
No rendered logo, frame, title screen, main menu, audio output or gameplay exists.
The 6MiB VRAM bank remains zero. Timing is the inherited explicit diagnostic model,
not measured hardware latency; Run(1) counts are not claimed as retired instructions.

## Implementation and safeguards

Event notification follows the pinned WaitObject order: set signal, process ordinary
waiters, invoke service notifier, then clear Pulse state. A OneShot waiter can consume
before notification. Repeated signals still notify; clearing/exporting do not. The
DSP binding uses a weak reference to the same live interpreter, avoiding a dangling
service capture or an ownership cycle. Faulted/expired targets stop explicitly.

Notifier failure may occur after ordinary event/waiter effects. Those effects are
not rolled back. The SVC preserves CPU registers and returns an explicit host stop,
not an invented success/error Result. GSP command/display and DSP interrupt paths
propagate notifier failure as well rather than silently accepting it.

GetSemaphoreEventHandle lazily allocates retained objects transactionally. Export
failure preserves handle-table generation/free-list state and publishes no partial
object; repeated calls and reconnects export the same event. SetSemaphoreMask stores
the low16bits for future signaling. Direct SetSemaphore and event notification OR
bits into the same ARM-to-DSP APBP peripheral, not the DSP-to-ARM completion state.
They do not execute firmware or advance ARM time synchronously.

WriteProcessPipe is deliberately bounded to the observed pipe2/four-byte/static-
buffer shape. Larger payloads, other pipes and blocking behavior remain unsupported.
The existing device method stages actual bytes, validates descriptors, updates only
the CPU-owned pointer and sends the real mailbox word. A full pipe/busy mailbox
preserves the request/storage. Reload, unload, external AHB, audio output, read-pipe
IPC, input sampling and broader device lifecycle remain unsupported.

## Validation

Both GCC and Clang full native builds link599 unchanged private AOT page units.
All58 CTest suites pass under each compiler; all58 ROM-free Clang ASan/UBSan suites
pass at-O1 with leak checking and halt-on-error. Complete configured test-name sets
match with no skips or duplicates. The earlier56 suites remain, with one obsolete
unsupported GetSemaphoreEventHandle fixture redirected to still-unsupported unload.

Two new suites cover notifier ordering, repeated signals, waiter consumption, weak
lifetime, actual DSP semaphore read/clear instructions, direct/event OR combination,
low16-bit presets, protected replies, allocation failure and handle-table exhaustion,
malformed IPC, pipe input normalization, memory permissions and busy-mailbox rollback.
They are synthetic component coverage, not proof of original audio output.

Five ordinary original-startup cases match stdout/stderr/exit/test-file bytes between
compilers: live-reference, probe-reference, known-only, bounded100 and disabled.
226 corresponding final capture/data/log files also match (host run timing receipts
excluded). Logging-only captures reproduce the ordinary executable's final stop.
Code, full raw RomFS, original ExHeader and all603 AOT archive members retain their
verified identities. Full IVFC checking was not repeated. No Windows/macOS build.

Initial new-test include/known-byte expectation errors were corrected before final
runs. Unoptimized sanitizer and combined capture runs exceeded bounded tool windows;
those incomplete logs remain distinct. The initial capture verifier incorrectly
expected consumption; the final proof explicitly requires read0/write4. No failed or
incomplete attempt is counted as passing. Current private evidence:semaphore-checkpoint/.

## References and next work

Pinned Azahar86a9f9236ae42bb5a2b995dbc933d599d8ea07ac: kernel/event.cpp,
kernel/wait_object.cpp/.h and service/dsp/dsp_dsp.cpp under src/core/hle/.
These establish event/notifier ordering, preset semantics, direct semaphore behavior
and audio-message normalization. The unchanged Teakra3d697a1 APBP implementation
provides the real peripheral. No hardware measurement was performed.

Next inspect MMIO0x2A0 in the BTDMP region and the original firmware access before
adding peripheral behavior. Do not substitute generic register storage or claim a
completed slice/consumed command merely because a timer fired. Keep all explicit
CPU/VRAM/CFG/special-data/reset policies and stop on unsupported hardware semantics.
