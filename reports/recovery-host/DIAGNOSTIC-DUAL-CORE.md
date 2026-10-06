# Diagnostic dual-core scheduling and observed HID boundary

Checkpoint: October 6, 2026. Baseline commit
`095af71fa4aa1afa1fe47d6b55c9ff693ee9550f`, exact restored tree
`22ff0094627462e5a8490dd7affe02f70dbe6dc8`.
The commit containing this report is the implementation checkpoint; the downloadable
handoff receipt records its final SHA, source tree and any observed hosted CI.

## Recovery before new work

Remote main had advanced beyond the last delivered 48635d1 handoff. The interrupted
095af71 implementation already supplied private memory aliases, canonical exclusive
epochs and bounded Protect. Its source snapshot was recovered from Actions run
37463588994, artifact 11413276822. The downloaded ZIP hash, inner tar hash, source
commit and complete reconstructed Git tree were checked before editing. The stale
canonical handoff was not treated as the latest implementation state. The full GCC
baseline passed 41 tests and reproduced the processor-1 CreateThread stop at round
245. Alias implementation is inherited work, not a second implementation this turn.
The earlier 74 trace files mentioned by that commit were not recovered; this turn
produced its own final-state captures and does not claim those old traces as evidence.

## Explicit diagnostic execution model

The new `--cpu-mode diagnostic-dual` runs logical cores 0 and 1 on one host executor,
issuing at most one existing recorded A32 instruction per core per ARM11 tick, in
core-0 then core-1 order. It does not decode or replace the original instructions.
The local one-step adapter uses existing immutable PackedOps and ExecuteBlock,
including a bounded recorded suffix for callback return PCs. Vendor code, opcode
backends and private generated pages are unchanged. Thumb remains unsupported.

One instruction per tick is an explicit diagnostic cost assumption, NOT measured
ARM11 latency, accurate CPU/bus/cache timing or physical parallel host execution.
The nominal clock is 268111856 Hz; absolute tick deadlines round up to nanoseconds
without accumulating per-instruction rounding. CPU-busy execution now gives timed
platform events a chance to run; idle time jumps only to a relevant actual deadline.
Timeout expiration precedes quota and display delivery at equal timestamps, an
explicit host tie policy. Failed display preflight preserves the pending issue
slot/deadline so retry does not reissue an instruction or double-charge time.

Each core selects its own eligible priority/TLS/thread context. Alternating host
cores retains each logical core's exclusive reservation; replacing its thread,
entering a denied quota phase or completing an SVC clears that core's reservation.
This is not a complete ARM weak-memory/cache implementation or a general scheduler
parity claim. Same-priority scheduling, priority inheritance and all kernel policies
remain bounded by the inherited runtime and the new tests.

The actual application ResourceLimitObject feeds core-1 app/system windows. The
pinned HLE Multi period is floor(nsToCycles(2 ms)) = 536223 ticks. A 30-percent update
begins with a 375357-tick system window, then a 160866-tick app window. Zero disables
preemption. Active updates flip/rebase like the inspected reference ChangeState.
The pinned HLE currently aliases its unimplemented Single limiter to Multi; this
checkpoint does NOT establish the title's actual ExHeader mode or maximum. The
existing maximum 80 remains inherited. Denied app workers remain Ready but are not
eligible; no sysmodule instructions or synthetic sleep-completion replies are made.
Core 0 is not gated. Cores 2/3 remain unsupported.

Strict mode remains the default, preserving its original processor-1 stop. The
new mode is not a silent removal of the old guard. Successful original priority
setup is implemented only for a never-executed Ready worker, which cannot already
own or await a mutex. Full priority inheritance and arbitrary live-thread priority
changes are not claimed. HID is discovery-only; every HID IPC still stops untouched.

## Actual original execution, with the important limitation

Use all existing explicit profiles plus diagnostic-dual and a 100000000 instruction
budget. The original code maps/protects the stack alias, creates its actual core-1
thread and changes its priority to 49. It next reaches:

```text
cpu_ticks=4839693 core0_instructions=2180849 core1_instructions=0 quota_transitions=0
display_periods=1 guest_now_ns=18051022
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=248
last_ipc_session=hid:USER request_header=0x000a0000
```

Pinned HID names this GetIPCHandles. Its IPC remains unmodified: no input shared
page, event handles, controller values or success response is supplied.

**The original core-1 worker has executed ZERO instructions at this stop.** It is
Ready at PC 0x00104DF4, TLS 0x1FF82600, with the first eligible quota boundary still
at tick 5180070. The main thread reaches HID before that deadline. Original startup
therefore proves creation and gating, not execution of this game's second-core
code. Separate synthetic original-instruction test programs establish both-core
execution, shared flag exchange, timer progress and quota eligibility.

There are four threads and 22 open handles. CpuTime is 30; the source stack view
is inaccessible and its real alias remains writable. The two compilers produce
identical final register/page/VRAM/alias/test-file snapshots. The complete 6 MiB
VRAM bank remains zero. There is no useful screenshot, logo, rendered frame, main
menu, shader execution, audio, input support or gameplay proof. Display periods
and dispatch rounds are NOT frame counts or completion percentages.

Without the new mode, original startup still stops at processor-1 CreateThread,
PC 0x00102FCC, round 245 and time 16713681 ns. The diagnostic CLI default 1000000
budget is deliberately too small for this path and produces a BlockLimit, exit 4.
Use `--block-limit 100000000` for the documented run. In this mode that parameter
bounds successful recorded instructions between SVCs, not the strict mode's block
budget. Host-event limits independently bound SVC attempts and timed-event pumps.

## Verification actually performed

Full GCC and Clang builds link all 599 private page units. All 44 CTest suites pass
on both compilers. All 44 ROM-free Clang ASan/UBSan suites pass with leak checking
and halt-on-error. No prior suite was removed. New tests cover instruction/block
comparison, callback suffixes, exact stop behavior, rational deadlines and overflow,
quota updates and phases, per-core TLS/pseudo handles/priorities, monitor isolation,
ready-worker priority validation, cross-core flag exchange, bounded Run/resume,
CPU-busy timer progress and failed display-event repair/resume equivalence.

Twelve production scenarios match stdout, exit and newly created test-file bytes
between compilers: strict-full, dual-full, diagnostic default budget, no display,
no CFG, no VRAM, no RomFS, no PTM, invalid/missing cpu mode, and two work limits.
Each compiler has six final capture files for strict and six for diagnostic mode;
all corresponding bytes match. These are final-state inspection captures, NOT an
independent ARM emulator, complete event trace or proof of real hardware accuracy.

Original code SHA, full raw RomFS SHA and all 603 regular private AOT archive members
match their backups. Full IVFC block checking was not repeated. No Windows/macOS
build was performed. Early synthetic tests had wrong opcode metadata for branch
words, causing two new suites to fail; corrected tests and failure logs are retained.
The pending-tick resume issue was fixed and regression-tested before final builds.
Unavailable streaming execution and interrupted finite builds are recorded as setup
limitations, not suppressed successful test runs.

Private evidence: core1-checkpoint/. Public summaries: DUAL-CORE-PROOF.json,
DUAL-CORE-MATRIX.json, DUAL-CORE-FULL-GCC.txt and DUAL-CORE-STRICT-GCC.txt.
No original bytes, generated page contents, private raw captures or native binaries
are added to public GitHub. The local snapshot is not remote history.

## References and next work

Pin: azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
Kernel thread.cpp/.h: CpuLimiterMulti update/selection/phases; kernel.h: special
CpuTime constants and Single/Multi distinction; core_timing.h: ARM11 clock; svc.cpp:
SetThreadPriority validation; hid/hid_user.cpp: observed GetIPCHandles command.
Exact available blob identities are recorded in core1-checkpoint/references.json.

Next inspect and implement HID GetIPCHandles with actual shared-memory and retained
event ownership, then rerun unchanged code. Do not manufacture controller input,
periodic input timestamps or a completion event. The real worker may then become
eligible; observe it rather than claiming in advance that it executes correctly.
Keep diagnostic timing assumptions, strict mode and original-input identity explicit.
