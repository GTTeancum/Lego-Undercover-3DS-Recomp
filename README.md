# LEGO City Undercover: The Chase Begins — 3DS Recomp

Recovery repository for an AI-assisted native recompilation effort targeting **LEGO City Undercover: The Chase Begins** (Nintendo 3DS).

## Current rebuilt state

Resume from [CHECKPOINT.md](CHECKPOINT.md).

**What improved:** Five more startup gaps are filled: a copying loop, its return, a missing call, a stack return and the next call setup. The program can also save both current screen images after execution stops, without SDL. All 5 fresh startup runs passed their previous stopping points.

**What is visible:** A fresh headless post-stop capture shows both screens completely black: 400x240 top and 320x240 bottom. This is the current stopped scanout, not a windowed screenshot or the earlier first-nonblack loading capture. Those different capture points are not a regression comparison. No fresh windowed run was possible because the pinned SDL3 download failed. Menus, gameplay, speaker playback and Windows execution remain unverified.

**Checks:** The first four builds each passed 88/88 enabled tests once; the final build passed 89/89 once, including the new snapshot suite. The exact tested builds and complete run evidence are saved privately. Tests and startup progress are not proof of playability.

**Still unfinished:** Usable menus, gameplay, speaker playback and Windows execution remain unverified. Startup currently stops at another missing game-code section.

**Next:** Add the reviewed two-instruction entry that saves six registers on the guest stack and calls the already recorded routine at 0x0015117C. Its later continuation at 0x0014F44C remains unobserved; do not include that successor or invent a return target. See [the recovery record](docs/recovery/CURRENT-REBUILD.md) for measured results and the exact continuation.

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
