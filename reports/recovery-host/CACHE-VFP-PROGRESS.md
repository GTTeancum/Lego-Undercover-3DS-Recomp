# Cache coherence and bounded VFP short vectors

October 7, 2026. Source based on b6ea4709af883666f9b0eec9cb0af74b5926f7e2.
This is native/headless startup work, not a playable port or rendered boot.
Publication and durable-backup receipts are appended to the downloadable handoff
only after actual verification.

## Original-game progress

The unchanged original game now passes eight DSP FlushDataCache requests and one
GSP FlushDataCache request, covering 86,808 bytes in total. Its memory is already
coherent/cacheless: these operations validate the process and mapped private range
and return the bounded reply without copying bytes, executing a device transaction,
invalidating reservations, charging time, or manufacturing an interrupt.

The first newly reached VFP operation occurred at 0x00258AF0 with a three-lane
FPSCR setting but a scalar-bank destination. Implementing the bounded VFPv2
single-precision short-vector rules lets that original routine and subsequent
three- and four-lane operations execute. The game issues 58 supported operations
with nonzero vector settings: 4 scalar-bank, 6 three-lane, and 48 four-lane calls,
214 individual lane operations. None of the original instruction words changed.

An independent Python rational binary32 replay matches all 32 output register words
for every captured call, including unchanged lanes. The actual operands are finite,
round-to-nearest-even; the replay rounds multiply and add separately. It also checks
that original FPSCR and CPSR values remain unchanged on this observed path. This is
not a general independent ARM emulator or proof of all VFP exception behavior.

Final English startup:

```
stop=UnsupportedIpc pc=0x0025947c detail=0x32 thread=1 dispatch_rounds=843
last_ipc_session=gsp::Gpu request_header=0x000c0000
host_ipc_error=PICA default/immediate attributes are unimplemented at list byte 0x380 register 0x232
cpu_ticks=85142780 core0_instructions=14847201 core1_instructions=99
guest_now_ns=317564398 display_periods=19 quota_transitions=299
DSP scheduled_slices=2447 notification_wait_slices=1 completed/attempted=40140800
```

The pending 1,376-byte command list first selects default/immediate attribute index 2
at byte 0x380. The ENTIRE rejected queue operation remains uncommitted: live GPU
registers, upload state, relay page and video memory are unchanged by the rejection.
No attribute, draw, P3D signal or successful list completion is supplied.

There are seven threads and 37 handles. The audio sink holds 9,794 file records,
all actual FIFO source-mask 3 and all zero startup samples, with no replacement
frames. Video memory contains only the inherited 819,200-byte depth clear at offset
0x419400, repeated little-endian 0x00FFFFFF; all other bytes are zero. No logo,
title screen, main menu, rendered frame, useful audio, playback or gameplay exists.

## Bounded implementation

DSP command 0x00130082 and GSP command 0x00080082 accept two u32 parameters and exactly
one copied process object. A current-process pseudo-handle or real duplicate resolves
to the SAME current ProcessObject; a wrong/stale handle or another object is rejected.
Nonzero targets must fit one readable private backing span, respecting alias protection.
Shared/device, unmapped, overflowing and cross-region spans remain unsupported.
Zero size performs no range work after process checks. DSP additionally requires
its healthy attached live device. Full reply preflight and private reply validation
precede response writes. InvalidateDataCache remains unsupported.

Private writes already reach their one shared backing. Successful maintenance does
not read or modify target bytes, call a device, allocate a cache, complete AHB/FCRAM
access, establish GPU ownership, clear a dirty renderer surface, or imply audio/frame
completion. This range/failure policy is narrower than the reference, not claimed
as measured firmware error precedence. Hardware cache behavior is not modeled.

Short vectors use eight-register circular banks, the FPSCR length and stride fields,
scalar destinations in S0..S7, Fm scalar-bank broadcast, and Fn advancement. Supported
operations reuse the existing scalar arithmetic, rounding and sticky exception logic.
Comparisons/conversions remain scalar. Strides 1/2 are bounded; invalid encodings,
repeated-bank vectors, shifted cross-lane overlap, double vectors, unsupported forms
and enabled exception traps stop before committing a candidate state. The PC advances
once for a vector instruction. This is deliberately not full VFPv2 coverage or
hardware vector timing. No existing scalar arithmetic algorithm was replaced.

## Verification

Full GCC and Clang executables link all 599 original AOT page units plus the same
three authenticated build-generated supplemental blocks. All 72 configured suites
pass with GCC, Clang and ROM-free Clang ASan/UBSan, with leak checking/halt-on-error.
Exact JUnit names match configured names without skips or duplicates. The previous
69 suites are unchanged; new DSP-cache, GSP-cache and short-vector suites were added.

Six ordinary startup compiler pairs match stdout, stderr, exit and their owned test
files: English, French, Spanish, language unset, sound unset, camera unset. All 69
final capture files match between compilers. Independent checks cover nine exact
cache replies and unchanged target/neighbour bytes, 58 arithmetic calls, unchanged
rejected GPU state, the actual pending list, the complete audio file and depth-only
video memory. Logging hosts reproduce ordinary CLI after normalizing only the owned
root path. This is not a complete ARM/DSP address-space dump or hardware oracle.

Original code.bin, entire raw RomFS, ExHeader and all 603 saved AOT members retain
their hashes. GCC/Clang supplement source is identical. No full IVFC recheck,
Windows/macOS build, original-hardware timing, audio playback or renderer test ran.

## References and next step

Pinned Azahar 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac:
DSP service dsp_dsp.cpp blob f8b23c07925c6b4a9fe36acaa7954f05126441f5;
GSP service gsp_gpu.cpp blob 6f915e4d6a5d27853321d0a233103afb9d877bc0.
Both are under src/core/hle/service/. Their no-copy cache handlers are read alongside
the actual host GuestMemory implementation, not used to assume external-bus support.

ARM VFP11 Technical Reference Manual DDI0274H, sections 2.7 and 3.4.2, tables 2-7,
2-8 and 3-7, supplies bank/length/stride/scalar exceptions. Official parsed document:
https://documentation-service.arm.com/static/5e8e227c88295d1e18d377ac . Requested page
screenshots and direct download failed; no page-image inspection or new hardware
measurement is claimed.

Next: implement the actually requested PICA default-attribute setup after checking
its packed attribute format/index/known-state contract. Preserve whole-list failure
atomicity and draw/chain guards. Do not inject vertices, pixels, interrupts or
completed frames merely because the original code now reaches graphics setup.
