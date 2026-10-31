# LEGO City Undercover: The Chase Begins — 3DS Recomp

Recovery repository for an AI-assisted native recompilation effort targeting **LEGO City Undercover: The Chase Begins** (Nintendo 3DS).

## Current rebuilt state

**Resume from [CHECKPOINT.md](CHECKPOINT.md).** All eleven previously pending startup fixes are included. The publication resumes the two saved tested batches; it does not claim new game runs.

**What improved:** Five more startup gaps are filled. The four-row neighboring-data helper and the sixteen-row processing loop now finish, and both missing return steps are restored. All five fresh startup runs passed their previous stopping points. Startup reaches another missing helper; menus and gameplay remain unverified.

**Checks:** All 5 new builds passed 89/89 enabled tests, and all fresh runs passed their preceding stops. The six previously tested unpublished fixes are preserved, not counted as new runs. Complete captured audio, state and save contents match the recovered candidate byte-for-byte.

**What still blocks play:** Startup still stops at missing code. Menus, gameplay, speaker playback and Windows execution remain unverified. No new windowed run was performed.

The optional desktop keeps the whole touchscreen at bottom right. See [desktop build and controls](docs/DESKTOP.md). Completely black diagnostic captures stay private.

Public Git contains source and metadata only. Tested binaries, generated instructions and full recovery evidence are preserved privately. The previously rejected synthetic test upload was accepted unchanged on the requested retry.

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
