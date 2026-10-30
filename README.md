# LEGO City Undercover: The Chase Begins — 3DS Recomp

Recovery repository for an AI-assisted native recompilation effort targeting **LEGO City Undercover: The Chase Begins** (Nintendo 3DS).

## Current rebuilt state

**Resume from [CHECKPOINT.md](CHECKPOINT.md).** It contains only the latest state and next task; the longer recovery document preserves older evidence.

The native SDL3 window shows the whole second screen in the bottom-right corner. Tab enlarges it, F11 toggles fullscreen, and mouse clicks map to the original touchscreen coordinates. See [desktop build and controls](docs/DESKTOP.md).

Startup now passes the `0x00151604` gap. One authenticated metadata block adds **50 instruction words / 200 bytes**, using existing instruction support. Fresh headless and desktop runs each advance **902 core-0 instructions / 3,364 diagnostic ns** beyond their respective `db19fee` runs and stop at **MissingBlock `0x00152AFC`**. All **599 original AOT pages / 603 files** and the previous 21 supplements remain unchanged. Current totals are **22 supplements / 321 words** and **111,065 blocks / 545,432 verified instruction words**.

All **84 enabled test suites have passing results**: 83 passed in the initial full run; one test could not launch because its executable lacked execute permission. Restoring that file's mode without changing its bytes allowed exactly that test to pass. The original failed result and both actual JUnits are preserved. No rebuild or full-suite rerun was used. Private generated CPP/JSON words were checked against the original input, and all previous supplemental records remain identical.

Both native children and outer launchers exited **3** at the diagnostic missing block, with no timeout or stderr. Final registers, diagnostic time, complete audio, and state/save contents agree across modes. The existing **3,883-instruction** desktop/headless difference remains untraced.

The desktop presents **524 frames**. The actual 1280×720 first-nonblack startup capture and its decoded pixels match the preceding capture, including the lower-screen loading indicator. Usable menus, gameplay, speaker playback, and Windows execution remain unverified.

**Next:** add the reviewed nine-instruction dispatcher at `0x00152AFC..0x00152B20`. Existing fast, memory and shifted-MOV support suffices. Its conditional selected target and the captured return are still unrecorded; the next checkpoint gives the exact scope.

Public Git contains source and metadata. Original inputs, generated game code, executables, captures, saves and audio stay private. The exact tested native was preserved before both finite fresh runs.

## Historical recovery records

The original local working tree was lost. Historical **Recovery J** and **Focus K01** documents describe that earlier tree; their renderer and gameplay claims are not evidence that those systems exist in the current rebuilt source.

Those historical records described:

- 604 generated AOT translation files.
- Native runtime, 3DS service support, renderer, audio, desktop/input integration and tests.
- Native Linux execution reaching interactive Cherry Tree Hills.
- 296/296 initializers completed.
- Save/exit/reload coverage in-process.
- Known unresolved first-load real-time intro rendering defect.
- Focus K01 narrowed the invalid shadow projection data flow.

## Repository layout

- `docs/recovery/` — recovered checkpoint/status documentation and provenance.
- `src/generated/` — recovered/generated AOT game-code translation pages.
- `src/runtime/` — native runtime and memory/kernel implementation.
- `src/services/` — 3DS service implementations.
- `src/renderer/` — graphics translation/presentation.
- `src/audio/` — audio implementation.
- `src/frontend/` — desktop/input/window integration.
- `tests/` — native and Python regression tests.
- `tools/` — preparation, translation, extraction and diagnostic tools.
- `diagnostics/` — focused diagnostic fixtures such as Focus K01.
- `reports/` — reproducibility/results material safe to publish.
- `game/` — local user-supplied game data; intentionally ignored by Git.

## Game data

No copyrighted game image, RomFS, extracted assets, `code.bin`, saves, or captured game audio should be committed. Users must supply their own legally obtained game data locally.

## Current goal

Advance the unchanged original game toward genuine rendering, usable menus, and gameplay. Continue from verified surviving source and private input backups, preserve reproducible checkpoints, and implement missing behavior without fabricating GPU completion or game output.
