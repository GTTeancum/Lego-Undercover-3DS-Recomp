# October 31, 2026 — observed startup continuation

Read [CHECKPOINT.md](../../CHECKPOINT.md) first. Base: `7fb60ad75098e15d4d0f10c12c2f0ddcd9509f31`.

## What changed

Three more startup gaps are filled. The paired-row processing routine now completes all sixteen rows and returns to startup, and a missing call setup reaches the next routine. All three fresh game runs passed their preceding stopping points. The next routine needs additional verified instruction support; menus and gameplay remain unverified.

Runtime, generator, tests and build configuration are unchanged. All 139 earlier reviewed sections and all 599 original AOT pages / 603 files are preserved. Only observed-range metadata and four status documents change. The status documents were updated after execution; all other published bytes match the final tested candidate.

## Fresh measured runs

| Round | Added section, exclusive end | Actual MissingBlock stop | Extra executed instructions |
| --- | --- | --- | ---: |
| 01 | `[0x00153B3C, 0x00153BC4)` | `0x00153bc4` | +216 |
| 02 | `[0x00153BC4, 0x00153BCC)` | `0x0014fb9c` | +2,499 |
| 03 | `[0x0014FB9C, 0x0014FBA4)` | `0x0014ff08` | +2 |

Each range followed the preceding actual missing-code stop; no future branch or return was forced. Total advance: 2,717 core-0 instructions and 10,133 diagnostic ns. This is not a gameplay percentage or performance result.

All 3 final candidate builds passed **89/89 enabled tests once**, with zero failures, errors or skips. Every exact tested native was saved privately before its fresh finite game run. All runs ended at missing original code with native exit 3, no timeout and empty stderr. Original inputs, pages, source and each selected native were checked before and after execution.

State and saves are byte-for-byte unchanged. Complete captured audio is unchanged. Captured diagnostics do not establish speaker playback.

## Additional output verification

A private diagnostic executes the exact restored paired-row operations against 256 authored input cases covering 2,304 output rows. Every output matches a separate byte-wise scalar reference; source regions remain unchanged, output guards remain untouched and the only stack writes are the two original saved registers. Its exact executable was saved privately before running. Expected instruction counts prove that its setup executes once, not once per row. These controlled-data results do not validate the real game's output words or rendered pixels.

The actual first startup pass is checked separately against its measured pointers, row count, stack adjustment and completion flags. A second private diagnostic checks the exact original two-instruction return with 64 normal-stack cases and 64 unmapped-stack cases. It verifies saved-register restoration, all other CPU state, the LR destination and strict memory/target failures. These isolated cases are not an actual-game single-step return trace.

A draft evidence description initially described the next call target as already recorded. The static coverage report identified it as missing; the draft build was stopped before its tests and game run, the description was corrected, and the corrected candidate passed its full test gate. The range and generated operations were unchanged. Both development receipts are retained privately.

## Visible status and hosted checks

Menus, gameplay, speaker playback and Windows execution remain unverified. No fresh windowed run was performed. No fresh screen capture or paired capture-on/off run was performed. Earlier black captures remain private. No visible or gameplay milestone is claimed.

The preceding checkpoint's compiler, sanitizer and packaging checks passed. Current hosted statuses are recorded in the private publication receipt; no pending job is reported as a pass. Hosted tests do not execute original game code.

## Next exact task

Observed missing entry: **`0x14ff08`**. This section is **not ready for generation**: the existing supplement policy rejects a register-controlled shift at `0x0014FF1C`. The instruction takes its shift amount from another register rather than a fixed constant. The next task is to verify the existing runtime behavior and add narrowly checked support for this form in the rebuild tool, with controlled tests for shift-count boundaries, conditions, flags and overlapping operands. Offline inspection finds the first conditional call at 0x0014FF28, giving a provisional nine-word interval [0x0014FF08, 0x0014FF2C); it is not admitted or claimed executed. Keep the subsequent table lookup, table bytes and unobserved return continuation outside that first addition.

Preserve all 142 reviewed sections (143 page-local blocks). Add independently authored policy and runtime tests before admitting the new form; retain every existing guard. Then reauthenticate and review the complete first-exit interval. Do not append partial code, add unobserved successors, invent returns, decode at runtime or fabricate GPU completion.

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Ordinary source publication and ongoing private backups are authorized. Keep original inputs, generated code, executables, captures, audio and saves private.
