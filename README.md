# LEGO City Undercover: The Chase Begins — 3DS Recomp

Recovery repository for an AI-assisted native recompilation effort targeting **LEGO City Undercover: The Chase Begins** (Nintendo 3DS).

## Current rebuilt state

**Resume from [CHECKPOINT.md](CHECKPOINT.md).** It contains the latest state and next task.

**What works:** The last verified desktop build opens a window and draws the loading image. The whole touchscreen sits in the bottom-right corner; Tab enlarges it and F11 toggles fullscreen.

**What changed:** 5 more missing startup sections are filled. Each fresh game run passed its previous stopping point. Startup now reaches another unfinished section. Menus and gameplay remain unverified.

**Checks:** All 5 builds passed **84/84 enabled tests once each**, with zero failures, errors or skips. Each exact tested executable was saved privately before its fresh game run. Save contents and captured audio remain unchanged from the previous checkpoint. These were headless runs, so there is no new screenshot or visual-progress claim.

**What still blocks play:** Startup still reaches missing game code. Usable menus, gameplay, audible playback and Windows execution remain unverified. This is not yet a ready-to-play release.

**Next:** Fill the next reviewed startup section and test the next actual stop. See [the checkpoint](CHECKPOINT.md), [desktop controls](docs/DESKTOP.md) and [the recovery record](docs/recovery/CURRENT-REBUILD.md).

Public Git contains source and metadata. Tested builds and original-game evidence are preserved privately.

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
