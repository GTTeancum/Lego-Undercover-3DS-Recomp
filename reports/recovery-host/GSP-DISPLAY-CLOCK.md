# Scheduled display notification and framebuffer latching

Checkpoint: October 5, 2026 (America/Indiana/Indianapolis; final UTC logs cross into October 6).
Implementation `2a24226c75bf6e76901493809930da224486d29d`; exact tested/uploaded tree `e64f386ae8dba547317a8cffe0321e1667562d4a`.
Baseline `0b4f9c352463197f87f606655d0e5d49a69c4415`, tree
`ae2d03b14458cb77f0170132e5e4df19935d9a94`.

## Original startup now leaves its display wait

The baseline was restored from its attached source/evidence archive. All 636 manifest
entries and 317 indexed source blobs verified, and the restored Git tree matched.
A full native GCC baseline reproduced WaitingNoRunnableThread at round 228 before edits.
The private code/AOT and prepared RomFS parts were already available; no reupload,
CCI extraction, user-PC access or Work development was required.

New explicit option: `--display-clock-mode reference-idle`. It schedules a display
period every 4481136 ARM11 ticks at 268111856 Hz, following the pinned reference.
Absolute cycle positions are rounded UP to nanosecond deadlines: the first deadline
is 16713681 ns. The second is 33427362 ns; rounding is not accumulated per period.
The default remains disabled and reproduces the original display wait.

With the new option, the original code reaches:

```text
display_periods=1 guest_now_ns=16713681
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=232
last_ipc_session=gsp::Gpu requested_service= request_header=0x000b0040
ipc_words= 000b0040 00000000 00000000 00000000 00000000 00000000 00000000 00000000
```

That is SetLcdForceBlack, argument zero, NOT another queued GPU command. It remains
unsupported and its request is untouched. There is no successful LCD-blanking change
or rendered frame in this checkpoint. Existing-gamecoin reaches the same call at
round 224, while a fresh run without the clock option retains round 228.

## Effects independently checked

The first event broadcasts PDC0 and then PDC1 through the actual registered GSP relay.
Before the event, both original counters at 0x00593C84 and 0x00593C88 are zero. They
remain zero immediately after service delivery. Only after the original relay worker
runs do both become one. The host does not write those counters or return synthetic
success from their wait.

Thread 3 moves from WaitSynchAny to Ready with pending_wake=true and Result=0. Its
retained one-shot event becomes signaled again by the second notification, and the
original worker subsequently consumes it. At the final IPC stop it is waiting again,
the event is unsignaled, the relay is drained, and the handle count remains 17.

Both pending framebuffer descriptors are latched into the real stored GPU register
image. The selected second-buffer addresses become top PA 0x18346500 and bottom PA
0x18038400; right addresses are zero, stride 720, formats 0x341/0x301, shown_fb=1.
The two dirty bits clear. This changes configuration, NOT pixels or scanout output.

Independent Python replay matches every byte of the full 4096-byte page and all
1842 register words after delivery. Page bytes changed only at offsets 1, 24, 25,
513 and 577. The final page also matches the original worker consuming both entries.
PICA upload storage and the complete device VRAM bank remain unchanged.

## Visual status

The full 6 MiB VRAM bank is still zero. Those bytes come from the inherited explicit
reference-HLE cold-bank policy, not recovered artwork. No useful game screenshot,
rendered frame, main menu, executed shader or gameplay has been demonstrated. Test
patterns are not presented as game output. The user accepts no screenshot until
there is meaningful genuine visual output.

## Timing and safety scope

ReferenceIdle is a deterministic, opt-in PLATFORM MODEL, not cycle-accurate 3DS timing.
CPU dispatches still incur no modeled cycles. Only when no thread is runnable does
the runner advance to the earliest existing thread deadline or display deadline.
No host wall clock is consulted. CPU-busy display timing, scanline timing, rendering,
frame presentation and complete asynchronous GPU timing remain unimplemented.
A caller that externally advances time may leave a past-due display event, handled
at a subsequent idle pump; this is not a complete real-time event scheduler.

The existing kernel timer expiration and wait-wakeup algorithms are unchanged.
A new read-only NextWakeDeadline accessor lets the runner respect earlier timeouts.
At equal timestamps, timeout expiration occurs before display delivery, with no
intervening guest execution. This tie order and whole-event preflight are explicit
host policies, not measured hardware ordering. Deadlines retain phase across Run
calls. Idle events are separately bounded by that call's host_event_limit, in addition
to the existing dispatch-round limit; a fully waiting/ignoring process cannot spin
forever in the host. Terminal dead processes receive no further display events.

PDC is broadcast to all registered sessions, even without GPU ownership. The service
honors ignore_pdc and the 32-pending-interrupt threshold, increments the corresponding
32-bit missed counter with wrap, and uses the 52-slot ring otherwise. It updates dirty
framebuffer state even when a PDC is ignored or counted as missed. Index/dirty fields
use their defined low bits; unrelated header bits are retained. Address conversion
supports only null, the existing VRAM selector and the original linear-heap selector.
It adds no CPU VRAM mapping or replacement pixel memory.

Invalid/failed rings or unsupported framebuffer selectors stop the complete event
before register/page/event mutation or clock advance. Prepared host-owned plans must
remain unmodified and are committed synchronously without intervening guest code;
source/generation checks reject stale plans. A hypothetical failed commit after time
advancement is reported explicitly, not described as rollback; none occurred.
Only the modified shared fields invalidate their backing reservation granules.
Existing command execution, P3D/PPF/PSC handlers and strict unsupported IPC are retained.
SaveVRAM HLE events, force-swap recovery, display-cache readers and general framebuffer
addressing are not added. No draw or blanking success is manufactured.

## Validation and recovery

Full GCC and Clang native builds link all 599 unchanged private AOT pages. All 36
CTest suites pass under each compiler. All 36 ROM-free Clang ASan/UBSan suites pass
with leak checking and halt-on-error. The new suites cover rational deadlines and
overflow, early/equal/late timers, phase retention, event-work bounds, strict default,
real retained-event wakeups, registered-session broadcast, dirty latching, ring wrap,
PDC suppression/missed counters, retired sessions and stale/invalid event rejection.
No previous CTest suite was removed. Seventeen original-startup scenarios have
byte-identical GCC/Clang logs and exits, including disabled and malformed clock modes.

The original executable, whole RomFS and all 603 AOT backup members are unchanged.
The whole RomFS SHA was rechecked; full IVFC-block verification was not repeated.
Registry 111043 blocks / 545111 raw words remains STATIC inventory, not executed CPU
instructions, frames or a completion percentage. No Windows or macOS build was run.

Setup issues are preserved in display-checkpoint/setup-notes.txt: an unsupported
streaming interface, an interrupted foreground matrix, and a duplicated Python
option-removal statement in an early matrix driver. The corrected final driver
completed all 17 scenarios on a new owned root. These were not suppressed native
test failures, and incomplete matrices are not reported as the final result.

Private evidence: display-checkpoint/ with capture/, make_trace.py, verify_display.py,
display-proof.json, validate.py, validation-summary.json, identity-proof.json,
ctest-gcc/clang/asan.txt, build_remaining.py, final_matrix.py and driver logs.
The trace uses a logging-only alternate runner; ordinary GCC/Clang runs independently
reach the same stop. Raw uploads/VRAM/page captures remain private. Public summaries
are DISPLAY-PROOF.json, DISPLAY-VALIDATION.json and DISPLAY-*-GCC.txt.

## Primary references and next work

Pin: azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
Inspected GPU constructor/VBlankCallback/SetBufferSwap in src/video_core/gpu.cpp;
FRAME_TICKS in gpu.h; BASE_CLOCK_RATE_ARM11 in src/core/core_timing.h; GSP PDC
broadcast/relay/dirty-framebuffer handling in src/core/hle/service/gsp/gsp_gpu.cpp;
FrameBufferInfo/Update layout in gsp_gpu.h; FramebufferConfig in regs_external.h.
Exact blob hashes are in the private references.json receipt.

Next implement the observed SetLcdForceBlack request from inspected LCD color-fill
semantics, without pretending that disabling a blanking flag renders a frame.
Rerun the original code with all explicit policies and a NEW empty shared archive.
Keep pursuing genuine visuals; post screenshots only once meaningful output exists.

## Hosted CI confirmation

GitHub Actions run `37391938526` on implementation `2a24226` completed successfully
for both GCC and Clang jobs. These hosted tests are ROM-free, not original-game runs.
