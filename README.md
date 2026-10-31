# LEGO City Undercover: The Chase Begins — 3DS Recomp

Recovery repository for an AI-assisted native recompilation effort targeting **LEGO City Undercover: The Chase Begins** (Nintendo 3DS).

## Current rebuilt state

**Resume from [CHECKPOINT.md](CHECKPOINT.md).**

**What improved:** Added the missing arithmetic support and fixed a code-table lookup crash exposed by tests. Five more startup sections now run, including both processing loops and their returns.

**What was checked:** All 5 new builds passed their full test gates: 84/84 (round 01), 86/86 (round 02), 86/86 (round 03), 86/86 (round 04), 86/86 (round 05). A fresh headless game run of each exact saved executable passed its preceding startup stop. The complete prior audio capture is preserved byte-for-byte as an exact prefix; the final capture adds 4 well-formed diagnostic records. Complete state and save contents match the prior same-mode checkpoint. This is a longer diagnostic capture, not evidence of speaker playback.

**What still blocks play:** Startup stops at another missing section of original game code. Menus and gameplay remain unverified. These were headless game runs. No new screenshot or visual progress is claimed; the earlier loading image remains the last verified windowed result. Speaker playback and Windows execution remain unverified.

The whole touchscreen stays in the bottom-right corner. **Tab** enlarges it, **F11** toggles fullscreen, and mouse clicks act as touch input. See [desktop build and controls](docs/DESKTOP.md).

**Next:** Add the next 105-instruction helper, which combines neighboring byte values into row words, restores its working pointer and returns through a stack-loaded address. The future memory values and return target remain unobserved. The exact developer handoff is in [CHECKPOINT.md](CHECKPOINT.md).

Public Git contains source and metadata. Tested builds, generated game code and complete run evidence are preserved privately.

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
