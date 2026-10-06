# LEGO Chase Begins — canonical continuation handoff

Updated October 6, 2026. Continue in assistant scratch, **not the user's PC or Work**.
Project: LEGO City Undercover: The Chase Begins, Nintendo 3DS USA. This is a headless
native startup reconstruction, not a playable port. Post an updated downloadable
Markdown handoff and recoverable source/evidence checkpoint every work turn.
Screenshots must be meaningful genuine game output. None exist at this checkpoint.

## Source and recovery

The commit containing this handoff implements diagnostic dual-core scheduling.
Its parent is `095af71fa4aa1afa1fe47d6b55c9ff693ee9550f`, whose verified tree is
`22ff0094627462e5a8490dd7affe02f70dbe6dc8`. The downloadable receipt appended after
publication identifies the final commit, tree, archive and observed hosted CI.
Use the freshly read remote parent and force=false when publishing. Local Git is
a reconstructed snapshot/index, not remote history.

095af71 survived an interrupted turn. It already implemented real private memory
aliases, canonical reservation epochs and bounded Protect, but its handoff was stale.
Recovered its source from Actions run 37463588994, artifact 11413276822. ZIP SHA:
`186aa2bbd7df0ac0c68e46369da0c5f4aa5af5ef07868278433a63e600a3f0a9`.
The ZIP, inner tar, commit and full reconstructed tree were checked before edits.
The full GCC baseline passed 41 tests and reproduced processor-1 CreateThread at
round 245. These memory operations are inherited work, not new implementation here.
The older alias traces mentioned in that commit were not recovered. This turn made
new final-state captures; do not claim those earlier 74 traces as current evidence.

## Actual original-game boundary

With the explicit modes and command below, original startup now reaches:

```text
cpu_ticks=4839693 core0_instructions=2180849 core1_instructions=0 quota_transitions=0
display_periods=1 guest_now_ns=18051022
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=248
last_ipc_session=hid:USER request_header=0x000a0000
```

The original code maps/protects its 32 KB stack alias, creates its actual processor-1
thread and changes that worker's priority to 49. There are four threads and 22 handles.
Application CpuTime remains 30. Source 0x08045000 is protected; alias 0x0E000000 is
writable and shares its original backing, rather than a copy or replacement allocation.

**The original processor-1 worker has executed zero instructions at this stop.**
It is Ready at PC 0x00104DF4, TLS 0x1FF82600. Its first eligible app window is still
at tick 5180070. The main thread reaches HID first. Synthetic programs verify actual
execution on both logical cores, but that is not proof that this game's worker ran.

HID is discovery-only. The pinned command 0xA is GetIPCHandles. The request remains
untouched: no input shared page, event handles, controller values, sample timestamps
or successful initialization reply is supplied. The complete 6 MiB VRAM bank is
still zero. No logo, rendered frame, main menu, executed shader, audio or gameplay
has been demonstrated. Do not substitute test patterns or blank-buffer screenshots.

Strict mode remains the default. Without --cpu-mode, startup still stops at processor-1
CreateThread, PC 0x00102FCC, round 245, time 16713681 ns. Diagnostic mode's default
1,000,000 budget hits BlockLimit (exit 4) before the documented boundary. Use
--block-limit 100000000. Other absent-profile paths remain distinct diagnostic stops.

## Execution model and limitations

New opt-in: `--cpu-mode diagnostic-dual`. One host executor issues at most one immutable
recorded A32 instruction per logical core per ARM11 tick, core 0 then core 1. One
instruction per tick is an explicit diagnostic cost assumption, not measured ARM11
latency, cache/bus behavior, physical parallel execution or cycle-accurate emulation.
The nominal clock is 268111856 Hz. Absolute tick deadlines round up to nanoseconds,
without accumulating rounding per instruction. A local recorded-step adapter uses
the existing PackedOps/ExecuteBlock and bounded callback-return suffixes. Vendor,
opcode backends and private AOT pages are unchanged. Thumb remains unsupported.

The existing application ResourceLimitObject feeds core-1 eligibility. The pinned
HLE Multi limiter uses a 536223-tick period; 30% gives 375357 system ticks followed
by 160866 application ticks. Zero disables preemption. Active updates flip/rebase
as the inspected reference does. Its unimplemented Single limiter is aliased to
Multi: this checkpoint does not establish the title's actual ExHeader mode or CPU
maximum. Maximum 80 remains inherited. No sysmodule instructions are modeled.
Denied application workers stay Ready but ineligible; no sleep completion is faked.
Core 0 is not gated. Cores 2 and 3 remain unsupported.

Each logical core selects its own eligible priority/TLS/pseudo-thread context. Host
core alternation preserves reservations. Actual same-core thread replacement, a
denied quota phase or a completed SVC clears that core's reservation. No complete
ARM weak-memory model, general same-priority fairness or priority-inheritance parity
is claimed. The observed priority change is supported only for a never-executed
Ready worker; arbitrary changes to active/waiting threads remain unsupported.
HID discovery does not implement its complete session-limit behavior.

Timers, quota and display events can run during CPU-busy execution as well as idle.
Timeouts precede quota/display at equal timestamps, an explicit host tie policy.
Display preparation precedes time advance; failed preparation retains the pending
issue slot/deadline for correct retry. In diagnostic mode --block-limit bounds
successful recorded instructions between SVCs. Strict mode retains its old block
budget. SVC attempts and timed-event pumps are independently bounded by the host
work limit. Keep these assumptions explicit when extending the scheduler.

## Validation and evidence

Full GCC and Clang builds link all 599 unchanged AOT page units. All 44 CTest suites
pass with each compiler. All 44 ROM-free Clang ASan/UBSan suites pass with leak checks
and halt-on-error. No prior suite was removed. New tests cover recorded stepping,
quota phases and overflow, per-core context and monitors, cross-core flag exchange,
CPU-busy timers, work/resume limits, priority validation and failed-event retry.

Twelve paired production scenarios match stdout, exit and owned test-file bytes:
strict-full, dual-full, diagnostic default budget, no display, no CFG, no VRAM,
no RomFS, no PTM, invalid/missing CPU mode, and two work limits. Strict and diagnostic
captures each have six files per compiler; every corresponding byte matches.
These are final-state inspection snapshots, not a complete invocation trace or an
independent ARM emulator comparison. Original core-1 execution is still pending.

Code.bin and whole raw RomFS hashes match, and all 603 regular AOT backup members
are byte-identical. Full IVFC checking was not repeated. The 111043 registry blocks
and 545111 words are static inventory, not executed counts or frames. No Windows or
macOS build was performed. Initial new branch tests used wrong opcode metadata and
two suites failed; corrected tests and failure logs are retained. A pending-tick
retry issue was fixed and regression-tested before final builds. Interrupted finite
builds and unavailable streaming execution are preserved as setup limitations.

Current private evidence: `core1-checkpoint/`. Important files: build-results.json,
ctest-final-gcc/clang/asan.txt, validate_matrix.py, matrix-results.json,
inspect_original.cpp, inspect_driver.py, capture-strict-gcc/clang,
capture-dual-gcc/clang, proof.json, identity.json, references.json, restore.json and
setup-notes.txt. Inspection builds use a separate diagnostic main with the same
production libraries/AOT. Ordinary CLI runs independently reach the same stops.
Pre-adapter intermediate logs are separately named. Native binaries are not backed up.

Public report: reports/recovery-host/DIAGNOSTIC-DUAL-CORE.md. Public summaries:
DUAL-CORE-PROOF.json, DUAL-CORE-MATRIX.json, DUAL-CORE-FULL-GCC.txt and
DUAL-CORE-STRICT-GCC.txt. Raw captures stay private, never public GitHub.

## Next exact work

Inspect pinned HID GetIPCHandles and module construction. Implement its actual
shared-memory and retained-event ownership response without invented input samples
or completion signals. Rerun unchanged code in new empty test state and observe
when the original second-core worker becomes eligible and what it actually executes.
Do not blindly remove timing/safety guards. Preserve strict mode and prior GPU,
filesystem, memory-alias and CPU tests. Genuine visuals remain the goal.

Primary pin: azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
Relevant files: src/core/hle/kernel/thread.cpp/.h, kernel.h, svc.cpp, errors.h;
src/core/core_timing.h; src/core/hle/service/hid/hid_user.cpp. Exact available blob
identities are in references.json. Mutable searches were used only for navigation.

## Scratch and reproduction

Root: /mnt/data/lego_recovery/. Source: repo/. AOT: generated2/.
Code: restored/code.bin. RomFS: game/prepared-romfs/romfs.bin.
Prepared parts/manifest: romfs-library-roundtrip/. Builds: build-gcc/, build-clang/,
build-asan/. Evidence: core1-checkpoint/. Recovered snapshot: snapshot-095af71/;
original source ZIP: /mnt/data/LEGO-source-095af71.zip. Preserve unknown save roots.
The paired validator removes only its own new byte-verified gamecoin file.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
mkdir -p private-state
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/private-state/hid-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty --romfs game/prepared-romfs/romfs.bin --gpu-vram-mode reference-zero --display-clock-mode reference-idle --cfg-profile reference-stereo --cpu-mode diagnostic-dual --block-limit 100000000
```

Expected exit 3: untouched HID GetIPCHandles, round 248. Omit CPU mode for the strict
core-1 creation stop. Use clang++ for Clang. Omit LEGO_AOT_DIR for ROM-free sanitizers;
exact sanitizer commands are in core1-checkpoint/build-remaining.py.

## Durable recovery and every-turn mandate

GitHub: GTTeancum/Lego-Undercover-3DS-Recomp main. Library: /LEGO-Chase-Recovery/.
Restore the newest checkpoint, verify CHECKPOINT-MANIFEST.json before extraction,
then reconstruct SOURCE-INDEX.json paths/blobs/modes, including ignored tracked
reports. Never force-push snapshot history. Final commit/CI/archive names are appended
to the downloadable handoff after actual publication.

Private inputs have existing separate backups: code.bin;
LEGO-Chase-current-AOT-599pages-2026-10-03.tgz (unpacks generated2/);
Prepared-RomFS/ two raw parts and romfs-parts.json. Part sizes are 402653184 and
366526464 bytes. Reassemble with repo/tools/restore_romfs_parts.py after creating
the output parent. Restore refuses an existing output: verify instead of overwrite.
No original CCI extraction or user reupload is needed. Raw RomFS is 769179648 bytes;
native view offset 4096, size 769175552. Preserve the integrity tables.

RomFS SHA: 6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf.
Code SHA: 5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
AOT SHA: 2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9.
Historical CCI SHA: 3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525.

Preserve the previous 48635d1 private checkpoint/MD and 095af71 source ZIP unchanged
under historical/. Extract older source separately, never over current repo/.
The source/evidence backup excludes code, AOT, raw RomFS, CCI, .git, build products
and native executables. Raw state captures belong only in private backups. Scratch
can reset; Library/GitHub are recovery paths, not permanent scratch. Actions artifacts
expire after 30 days. Publish tested source/reports and durable backups when available.
Never invent CI, persistence, original second-core execution, visuals or gameplay.
Post updated downloadable Markdown and recoverable source/evidence every work turn.
