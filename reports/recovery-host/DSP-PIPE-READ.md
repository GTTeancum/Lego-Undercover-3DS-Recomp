# Original DSP response reads and DATA address conversion

October 6, 2026 (user-local date). This work extends published 5e7b03e. The baseline
already booted the DSP, consumed the original startup message and produced a real
32-byte response. This checkpoint implements reading that response, not generating it.

## Actual original-game result

ReadPipeIfPossible reads two bytes at round 261 and the remaining 30 at round 262.
Every returned byte matches the live firmware response, SHA-256
4cb606067ddd1281cd0f379eb999f1c0904a427848b9547c23d72123f8f134bc. The CPU-owned read
pointer changes 0 -> 2 -> 32. Neighbouring guest output bytes remain unchanged.
No fixed count or address table is returned. The second read waits through one real
16384-call firmware quantum for the command mailbox to clear, then sends the actual
read-notification slot. It does not forcibly empty the mailbox.

The unchanged game then makes 30 ConvertProcessAddressFromDspDram calls: 15 returned
word locations and their alternate-bank locations. Each returns 0x1FF40000 + 2*word
within the retained DATA mapping. Translation neither initializes nor reads SRAM.
The game sets the actual semaphore at round 293 and signals its exported real event
at round 294. This original event signal is now observed, not only component-tested.

Next untouched request, round 295:

```
stop=UnsupportedIpc pc=0x0025947c detail=0x32 thread=1 dispatch_rounds=295
last_ipc_session=cfg:u request_header=0x00010082
ipc_words=00010082 00000001 00070001 0000001c 0ffff658 ...
```

This is the one-byte SoundOutputMode configuration block. No setting is supplied.
Final state: 2 scheduled DSP slices, 1 separate synchronous notification-wait slice,
81920 completed/attempted Teakra Run(1) calls; ARM 2289950 recorded core0 instructions,
zero core1 instructions; guest time 18639948 ns, four threads, 31 handles.
Fourteen captured stereo pairs have actual FIFO provenance and are zero startup
silence. No fallback samples were added on this path. All 6 MiB VRAM remains zero.
No music, playback, HID sampling, rendered logo/frame, title screen, menu or gameplay.

## Contracts and bounded host policies

Only channel 2, peer 0, low-u16 lengths 0..128 are enabled for ReadPipeIfPossible.
The pinned service returns all requested bytes or zero, not a partial available read.
The upper half of the size word is ignored, as in Pop<u16>. Static output buffer 0
is at TLS+0x180 (command buffer+0x100), NOT TLS+0x100. The response contains the actual
byte count and static-buffer descriptor. Invalid shapes remain unsupported.

Before consumption, the handler validates reply/receive-table accessibility, static
buffer type/capacity, writable output and overflow. Output aliasing the reply or
any static receive descriptor is rejected, including aliases of underlying backing.
The bounded output must use private writable storage; device/shared output is not
enabled. Existing preparation reserves write metadata before consuming bytes, and
commit updates real exclusive-reservation epochs. Invalid output cannot drain the pipe.

When sufficient payload exists, the live reader stages actual bytes, updates ONLY
the CPU-owned read pointer, then waits for an empty incoming command mailbox. This
ordering follows the pinned LLE. At most four complete firmware quanta are allowed
per read (explicit host containment bound). A firmware fault or exhausted bound AFTER
pointer commit retains partial DSP effects, reports a terminal device failure and
returns no successful guest read. It is not called transaction rollback. A repeated
request cannot consume more from that faulted device. The older nonblocking device
method remains the default for existing callers/tests.

Synchronous waits leave ARM time and scheduled DSP deadlines unchanged, following
the reference convention rather than asserting hardware timing accuracy. They have
a separate counter. Actual emitted DSP notifications use the same retained-event
path as scheduled execution; no completion signal is invented by a read. Event or
copyout invariant failure after commit also does not pretend to roll back effects.

Address translation supports only the mapped 17-bit DATA word space and a healthy,
attached live device. Broader address/resource behavior remains unsupported. Original
special/reset/transmit/audio/CPU/CFG/PTM/display/ExHeader policies remain explicit.
No vendor instruction/peripheral, AOT, ARM scheduler, GPU renderer, HID producer,
filesystem or configuration-content change was made.

## Completed validation

Full GCC and Clang native executables link all 599 unchanged private AOT page units.
All 63 CTest suites pass under each compiler and all 63 ROM-free Clang ASan/UBSan
suites pass with leak checks/halt-on-error. Exact configured/JUnit name sets agree,
with no skipped/duplicated suites. All 61 inherited suites remain.

New tests cover lengths, all byte values, ring wrap, all-or-zero reads, real synthetic
firmware mailbox progress, event delivery, bounded waits and actual unknown-memory
faults, retry containment, protected/aliased outputs, allocation failure, reservation
coherence and all 131072 bounded DATA word addresses. Synthetic firmware is not
presented as original game execution or an independent CPU oracle.

Six ordinary original-startup pairs match stdout/stderr/exit/owned test-file bytes:
full, immediate boot, known-only, noCFG, guarded probe and strict CPU. All 291 final
capture files match across compilers. Independent parsing verifies the two outputs,
30 translations, real event identity/signal, untouched next CFG request/output and
unchanged captured GPU/uploads/GSP/HID/LCD/VRAM. Captures are logging-only snapshots,
not an independent CPU oracle or a full ARM address-space dump.

Original code, full raw RomFS, ExHeader and all 603 AOT archive members retain their
verified identities. Full IVFC checking was not repeated. No Windows/macOS build or
playback test. Hosted checks/publication are recorded only after observed completion.
Private evidence: pipe-read-checkpoint/. See the canonical handoff for recovery.

## Primary sources

Azahar pin 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac:
- src/core/hle/service/dsp/dsp_dsp.cpp, blob f8b23c07925c6b4a9fe36acaa7954f05126441f5:
  ReadPipeIfPossible reply and u16/all-or-zero semantics; DATA address conversion.
- src/audio_core/lle/lle.cpp, blob 388fe64ec1a5130a2c93a5dfa04ca84df109b567:
  read-pointer ownership, real synchronous mailbox waits and notification order.
- src/core/hle/service/cfg/cfg.h, blob 4c5275a343d62005d305b55668fbede285607132:
  next block 0x70001 is SoundOutputMode, mono=0/stereo=1/surround=2.
These are inspected reference implementations, not new hardware measurements.
