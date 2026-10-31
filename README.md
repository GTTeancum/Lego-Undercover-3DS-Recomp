# LEGO City Undercover: The Chase Begins — 3DS Recomp

Recovery repository for an AI-assisted native recompilation effort targeting **LEGO City Undercover: The Chase Begins** (Nintendo 3DS).

## Current rebuilt state

Resume from [CHECKPOINT.md](CHECKPOINT.md).

**What improved:** Five more missing startup sections now run: two byte-processing helpers, an averaging helper, a row-copying loop and its return step. Together they let startup move through a substantially longer stretch of its existing code. All 5 fresh startup runs passed their previous stopping points.

**What is visible:** The earlier loading image remains the last verified windowed result. This checkpoint was tested without a game window; no visual change is claimed.

**Checks:** Each build passed 88/88 enabled tests. The exact tested builds and complete run evidence are saved privately. Tests and startup progress are not proof of playability.

**Still unfinished:** Usable menus, gameplay, speaker playback and Windows execution remain unverified. Startup currently stops at another missing game-code section.

**Next:** Add the reviewed six-instruction loop that copies two guest words per row, advances both pointers, decrements its row count and conditionally repeats. Future memory values, loop completion and the fallthrough beyond this section remain unobserved; do not add the successor body. See [the recovery record](docs/recovery/CURRENT-REBUILD.md) for measured results and the exact continuation.

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
