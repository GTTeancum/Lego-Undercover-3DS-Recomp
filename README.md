# LEGO City Undercover: The Chase Begins — 3DS Recomp

Recovery repository for an AI-assisted native recompilation effort targeting **LEGO City Undercover: The Chase Begins** (Nintendo 3DS).

## Current rebuilt state

Resume from [CHECKPOINT.md](CHECKPOINT.md).

**What improved:** Five more startup gaps are filled. A 16-row copying loop and an eight-row neighboring-data helper now complete, and their missing return steps are restored. Startup reaches the next unfinished helper. All 5 fresh runs passed their preceding stopping points.

**What is visible:** The final post-stop captures remain completely black and are retained privately. No fresh windowed run was performed and no visual milestone is claimed.

**Checks:** All 5 builds passed 89/89 enabled tests. Complete saves, state and captured audio match the previous checkpoint. Exact builds and full run evidence are preserved privately.

**Still unfinished:** Menus and gameplay remain unverified. Speaker playback and Windows execution also remain unverified. Startup still stops at missing game code; these are not playable builds.

**Next:** Restore the next reviewed neighboring-row processing helper; its instructions are already supported. See [CHECKPOINT.md](CHECKPOINT.md) and [the recovery record](docs/recovery/CURRENT-REBUILD.md).

The whole touchscreen remains at bottom right; Tab enlarges it, F11 toggles fullscreen and mouse clicks map to touch input. See [desktop controls](docs/DESKTOP.md).

Public Git contains source and metadata, not original game inputs, generated game code, executables, saves, screenshots or audio.

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
