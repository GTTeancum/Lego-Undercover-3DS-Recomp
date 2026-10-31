# LEGO City Undercover: The Chase Begins — 3DS Recomp

Recovery repository for an AI-assisted native recompilation effort targeting **LEGO City Undercover: The Chase Begins** (Nintendo 3DS).

## Current rebuilt state

**Resume from [CHECKPOINT.md](CHECKPOINT.md).** It contains the latest measured state and next task.

**What improved:** Six more startup gaps are filled. Three processing loops now finish their requested rows, and all three missing return steps are restored. All six fresh game runs passed their preceding stopping points. Startup reaches another missing setup step; menus and gameplay remain unverified.

**Checks:** All 6 new builds passed 89/89 enabled tests and their fresh startup runs passed the preceding stopping points. Complete audio, state and save contents match the previous same-mode checkpoint byte-for-byte.

**What still blocks play:** Startup still stops at missing game code. Menus, gameplay, speaker playback and Windows execution remain unverified. There is no new visible milestone.

The optional desktop keeps the whole touchscreen at bottom right. See [desktop build and controls](docs/DESKTOP.md). The latest headless post-stop captures are black and remain private diagnostics.

Public Git contains source and metadata only. Tested binaries, generated game instructions and full recovery evidence are preserved privately.

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
