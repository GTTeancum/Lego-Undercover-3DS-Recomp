# Scaled device-VRAM transfer and the display-interrupt wait

October 5, 2026. Implementation `3a403ad3e8428a070b025ee31f8983a818cfc33b`.
Exact tested/uploaded tree: `b31e9e47a7eed04f24c9fc8f19225693e5069f22`.
Base main: `0d953eccb16b094bb7a07bb9715a1bf61ceace78`.
Base tree: `02fe02f966fa987f03305fedb41a412046eef7ec`.

## Original execution

The original pending 0x01001004 DisplayTransfer now executes: tiled RGBA8 input,
horizontal pair averaging, RGB8 linear output into the existing device-owned VRAM
bank. The programmed output width is halved; its height is retained. No CPU VRAM
mapping, source substitution, reset on transfer, or renderer was added.

Six original transfers complete in this order:

| Input selector | Output selector | Programmed output | Actual output | Bytes |
|---|---|---|---|---|
| 0x1F070800 | 0x1F300000 | 480 x 400 | 240 x 400 | 288000 |
| 0x1F070800 | 0x1F38CA00 | 480 x 400 | 240 x 400 | 288000 |
| 0x1F098800 | 0x1F000000 | 480 x 320 | 240 x 320 | 230400 |
| 0x1F070800 | 0x1F346500 | 480 x 400 | 240 x 400 | 288000 |
| 0x1F070800 | 0x1F3D2F00 | 480 x 400 | 240 x 400 | 288000 |
| 0x1F098800 | 0x1F038400 | 480 x 320 | 240 x 320 | 230400 |

All inputs declare 512 x 400 pixels. Total completed output is 1612800 bytes.
Independent inverse-Morton replay matches every output, the entire device bank,
all 1842 GPU words, and the full shared page around each transfer. PICA upload state
is unchanged. Each real byte commit precedes trigger-clear and its PPF notification.
The original worker consumes these notifications through the existing event path.

IMPORTANT: the original bank and these inputs/output areas are already zero under
the explicit inherited --gpu-vram-mode reference-zero policy. The original trace
changes zero VRAM bytes and is not evidence of artwork, drawing, or nonzero pixel
conversion. Separate nonzero synthetic captures change 286189, 195422 and 295 bytes
for the original-shape, all-pairs and cropped cases; all match independent replay.
Those patterns never enter an original-game run.

## Next boundary: no pending GPU packet

```text
stop=WaitingNoRunnableThread pc=0x0024b324 detail=0x00000000 thread=1 dispatch_rounds=228
last_ipc_session=gsp::Gpu request_header=0x000c0000
ipc reply=000c0040 00000000
```

All three threads are waiting. Thread 1 is in WaitArb on word 0x00371CA4, value -1,
with wait-if-less-than type 1 and comparison 0. Thread 3 is in WaitSynchAny on real
GSP event handle 0x0007001C. Thread 2 retains its earlier three-object APT wait.
Final GSP command header is 0x00000001 (index 1, count 0), interrupt count is 0,
error is 0, and the event is unsignaled. Kernel time remains zero. No pending work
is silently discarded, and no missing interrupt is fabricated to release the wait.

Original-code disassembly and the captured stack identify the main call chain:
0x00106538 -> 0x00257A68, mode 0x402. That routine waits for BOTH counters at
0x00593C84 and 0x00593C88 to differ from their entry snapshots, yielding through
0x0024E06C / GSP handshake 0x00130244 and address-wait helper 0x00251D88.
The live GSP callback table assigns ID 2 to 0x0011F4C8 and ID 3 to 0x0011F50C;
those original callbacks increment the respective counters. The pinned interrupt
definitions name these IDs PDC0/PDC1. No PDC/vblank clock is implemented yet.
This diagnoses the missing display-interrupt progress; it does not establish an
accurate refresh period, a rendered frame, or that waking this wait completes boot.

Existing-gamecoin startup reaches the same wait at round 220. Strict no-VRAM startup
still stops at the earlier transfer, round 170. Empty PTM remains explicit. No main
menu, shader execution, rendering, audio, controls or gameplay has been demonstrated.

## Scope and safety

Only the new exact 0x01001004 slice and the inherited 0x4400 RGBA4 slice are accepted.
New input dimensions must be nonzero whole 8x8 tiles. The programmed output must
fit the input and have even nonzero width and nonzero height. Source and destination
must be aligned, bounded, nonoverlapping device spans. Declared input and output
sizes are independently capped at 1 MiB. These are host support/safety bounds, not
firmware capacity or full error precedence. Flip, other scaling modes, formats,
layouts and overlapping copies remain unsupported.

RGBA8 bytes are A,B,G,R; RGB8 bytes are B,G,R. Each channel averages the adjacent
horizontal source pair using a widened sum then integer division, so values do not
wrap and odd sums truncate. Cropping selects the left/top programmed rectangle;
no flip-plus-crop skew is claimed. GPU registers retain the programmed dimensions,
not the scaled output dimensions.

Plans now carry input_bytes separately from output bytes and distinguish device
VRAM from CPU-backed output. Dependency guards inspect the full input span, including
a tail larger than the output. A prior staged fill or VRAM transfer feeding a later
source stops the whole eligible batch. No transaction-memory forwarding was added.
Unrelated CPU mappings at identical numeric addresses are neither overwritten nor
used to validate device bytes. Private FCRAM commits keep existing reservation rules.

Existing queue/relay preflight, stop markers, owner selection and real event delivery
remain. Valid prefixes do not commit on unsupported tails. PPF is never issued for
a rejected operation. Synchronous notification is inherited host policy, not GPU
hardware timing. No scheduler, IRQ source, guest code, AOT or opcode backend changed.

## Verification

Full GCC and Clang native builds link all 599 original AOT page units. All 34 CTest
suites passed with GCC, Clang and Clang ASan/UBSan (ROM-free sanitizer suites with
leak checking and halt-on-error). Fourteen production startup scenarios match
byte-for-byte between compilers, retaining strict and invalid-input cases.

New tests exercise all 65536 pairs of byte values, odd-sum rounding, color/alpha
order, crop/tile boundaries, the 1 MiB input cap, separate span limits, overlap,
unchanged failed plans, device/CPU address independence, source dependencies,
whole-batch rollback, real waiters, closed event handles, full handle tables,
nonowner/slot selection, 15-packet wrap, stop flags, protected responses and mixed
PICA/transfer notification order. Existing tests were not removed or changed.

Code, raw RomFS and all 603 private AOT archive members remain byte-identical.
Full IVFC-block checking was not repeated; raw SHA matches the prior verified image.
Registry block/word counts are static inventory, not frames or CPU execution counts.
A streaming-container invocation was unavailable; the finite driver performed the
actual builds. No compilation or test failure occurred in this implementation.

Complete private evidence is scaled-checkpoint/: build/test logs, original and
synthetic captures, verify_scaled.py, SVC/wait trace, original-code disassembly,
identity-proof.json and validation-summary.json. Trace objects are logging-only;
ordinary GCC/Clang executables independently reach the same stop. Raw game memory,
disassembly and compiled diagnostics are not published to GitHub.

## Primary references and next work

Reference pin: azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
SwBlitter::DisplayTransfer: src/video_core/renderer_software/sw_blitter.cpp,
blob 0a68afd07c3e22fdfaaf441d4744ef79abb2a86f. Vec4 promoted arithmetic:
src/common/vector_math.h, blob 8eaae0f7b5ff9cfd95956de78359a48fd2648dbb.
Earlier color.h/utils.h/GPU::MemoryTransfer establish byte order, Morton offsets and
PPF order. gsp_interrupt.h blob 8bd37d05144a9a636b0011770a3e8c1e0571ecc1 names PDC.
Original code and live callback/stack snapshots establish the actual waiting path.

Next inspect the pinned display/vblank event source, frequency, GSP PDC handling
and guest-clock scheduling. Add a justified device-event model rather than directly
incrementing the counters, unconditionally waking threads, or inventing a completed
frame. Preserve strict input policies and continue from a NEW empty test archive.

## Hosted CI

GitHub Actions 37388750201 passed both GCC and Clang jobs on implementation
3a403ad. These hosted tests are ROM-free, not original-game execution.
