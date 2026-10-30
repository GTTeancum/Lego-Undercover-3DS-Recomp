# LEGO City Undercover: The Chase Begins — 3DS Recomp

Recovery repository for an AI-assisted native recompilation effort targeting **LEGO City Undercover: The Chase Begins** (Nintendo 3DS).

## Current rebuilt state

Resume from [CHECKPOINT.md](CHECKPOINT.md).

**What improved:** Three more missing startup sections are included, and the rebuild tool now handles the arithmetic form needed by a larger helper. Fresh background runs get past the old stopping point.

**What still blocks play:** Execution still reaches missing game code. Menus, gameplay, speaker playback and a working Windows release remain unverified.

**Visual status:** No new windowed game run or screenshot was taken for this checkpoint. The last verified desktop result remains the loading image; this update does not establish a visible change.

**Checks:** The three builds passed all enabled tests: 83, 83 and 84 respectively. The final build adds a dedicated arithmetic test. These were headless builds; the optional desktop smoke test was not enabled locally. Each exact tested native was preserved before its fresh game run.

The existing desktop layout keeps the whole touchscreen at bottom right. Tab enlarges it, F11 toggles fullscreen, and mouse clicks map to touch. See [desktop build and controls](docs/DESKTOP.md).

**Next:** Add the reviewed return section and check the next actual stop. Public Git contains source and metadata; builds, original inputs and complete run evidence are preserved privately.

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
