# LEGO City Undercover: The Chase Begins — 3DS Recomp

Recovery repository for an AI-assisted native recompilation effort targeting **LEGO City Undercover: The Chase Begins** (Nintendo 3DS).

## Current rebuilt state

**Resume from [CHECKPOINT.md](CHECKPOINT.md).**

**What improved:** Five more startup gaps are filled, including the byte-processing loop and its return. Startup now runs through that routine and reaches a larger unfinished helper.

**What was checked:** All 5 builds passed 84/84 enabled tests. A fresh headless run of each exact saved executable passed the preceding startup stop. Complete captured audio, state and saves match the prior checkpoint.

**What still blocks play:** The latest run stops at another missing section of original game code. Menus, gameplay, speaker playback and Windows execution remain unverified. No new game-window run or screenshot was taken; the earlier loading image remains the last verified visible result.

The whole touchscreen stays in the bottom-right corner. **Tab** enlarges it, **F11** toggles fullscreen, and mouse clicks act as touch input. See [desktop build and controls](docs/DESKTOP.md).

**Next:** The next helper combines guest byte values into row words and returns through the stack. It requires a tested increase to the size limit before the rebuild tool can include it. The precise developer handoff is in [CHECKPOINT.md](CHECKPOINT.md).

Public Git contains source and metadata. Tested executables, generated game code and run evidence are preserved privately.

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
