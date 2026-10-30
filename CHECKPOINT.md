# Latest LEGO Undercover 3DS checkpoint

Updated October 30, 2026. **Read this file first; do not load the full historical recovery log to resume.** Use the current `main` commit containing this file.

## Completed

- Added only `[0x00151604, 0x001516CC)`: **one block / 50 words / 200 bytes**. Existing instruction support handles it; no evaluator or admission changes.
- Preserved all 21 prior supplements and all 599 original AOT pages / 603 files. Totals: **22 supplements / 321 words**; runtime verifies **111,065 blocks / 545,432 words**.
- Fresh headless and desktop runs each advance **902 instructions / 3,364 diagnostic ns** from their same-mode `db19fee` baseline.
- Both stop at **MissingBlock `0x00152AFC`**, thread 7, dispatch round 21,843. Native and wrapper exits are 3; neither timed out or wrote stderr.
- **84 suites have passing results:** 83 initially; one non-launching test passed after execute permission was restored with unchanged bytes. Initial failure and both actual JUnits are retained; no full-suite rerun or rebuild.

The whole second screen remains bottom-right: **Tab** enlarges it, **F11** toggles fullscreen, and mouse input maps to the touchscreen. The 524-presentation desktop run has the same first-nonblack startup pixels. Menus, gameplay, speaker playback, and Windows execution remain unverified. Cross-mode final registers/time/audio/save contents agree; the existing 3,883-instruction difference remains unexplained.

## Next exact task

Append only **`[0x00152AFC, 0x00152B20)` — one block / 9 words / 36 bytes**, preserving the 22 entries in `config/observed_aot_supplements.json`. The reviewed forms are 7 fast, 1 memory, and 1 already-admitted shifted MOV. Keep the preceding table words as data.

If its authenticated table read succeeds with the captured registers, the computed exit selects **still-unrecorded `0x001529C4`**. Captured LR **`0x00150E6C` is also unrecorded**. Do not include unobserved target helpers or assume that return is covered. Conditional next totals: **23 supplements / 330 words**, **111,066 blocks / 545,441 words**.

Generate only from the authenticated private original input, verify the new range and preserved records, build/test in the approved cloud context, preserve the tested native, and use finite fresh runs to establish the next actual stop. No runtime decoder, fabricated GPU completion, replay, or automatic retry.

## Continuation rules

Work in standard chat and the cloud; do not use Codex services, Dot, or the user's computer. Ordinary source/metadata publication to `main` is authorized; do not force-push. Keep original inputs, generated game code, executable bodies, captures/audio, saves, and private recovery identifiers out of public Git. Keep user updates within one screen.

The private continuation receipt binds the final publication to its tested native and complete evidence. The previous private checkpoint is retained unchanged with the new evidence; earlier unavailable evidence remains unavailable. For details only when needed, read the first section of [CURRENT-REBUILD.md](docs/recovery/CURRENT-REBUILD.md).
