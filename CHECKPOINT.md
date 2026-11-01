# Latest LEGO Undercover 3DS checkpoint

Updated October 31, 2026 (Eastern). Resume from `main`.

## Plain-English status

Three more startup gaps are filled. The paired-row processing routine now completes all sixteen rows and returns to startup, and a missing call setup reaches the next routine. All three fresh game runs passed their preceding stopping points. The next routine needs additional verified instruction support; menus and gameplay remain unverified.

Menus, gameplay, speaker playback and Windows execution remain unverified. No fresh windowed run was performed. No fresh screen capture was taken in this batch; earlier black captures remain private diagnostics.

## Verified state

Base: `7fb60ad`. Runtime, generator, tests and build configuration are unchanged. All 139 earlier reviewed sections and all 599 original AOT pages / 603 files are preserved.

Totals: **142 reviewed sections / 143 page-local blocks / 2833 supplemental words**; **111,186 runtime blocks / 547,944 verified words**.

Final stop: **MissingBlock `0x0014ff08`**. Advance from the same-mode baseline: **2,717 executed instructions / 10,133 diagnostic ns**. These are not gameplay percentages or performance measurements.

All 3 final candidate builds passed **89/89 enabled tests once**, with zero failures, errors or skips. Every exact tested native was saved privately before its fresh finite game run. No game run timed out or wrote stderr. State and saves match the preceding checkpoint. Complete captured audio is unchanged.

The paired-row helper also passed 256 controlled-data cases covering 2,304 output rows, with an independent scalar reference and input, output and stack guards. A separate return check passed 64 normal-stack cases and 64 unmapped-stack cases. These do not verify real-game pixels or gameplay.

## Next exact task

Observed missing entry: **`0x14ff08`**. This section is **not ready for generation**: the existing supplement policy rejects a register-controlled shift at `0x0014FF1C`. The instruction takes its shift amount from another register rather than a fixed constant. The next task is to verify the existing runtime behavior and add narrowly checked support for this form in the rebuild tool, with controlled tests for shift-count boundaries, conditions, flags and overlapping operands. Offline inspection finds the first conditional call at 0x0014FF28, giving a provisional nine-word interval [0x0014FF08, 0x0014FF2C); it is not admitted or claimed executed. Keep the subsequent table lookup, table bytes and unobserved return continuation outside that first addition.

Preserve all 142 reviewed sections (143 page-local blocks). Add independently authored policy and runtime tests before admitting the new form; retain every existing guard. Then reauthenticate and review the complete first-exit interval. Do not append partial code, add unobserved successors, invent returns, decode at runtime or fabricate GPU completion.

## Continuation rules

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Source/metadata publication to main and ongoing private builds/backups are authorized; never force-push. Keep original inputs, generated game code, executables, captures, audio, saves and private recovery identifiers out of public Git. Keep summaries plain and omit completely black captures.

Details: [current recovery record](docs/recovery/CURRENT-REBUILD.md).
