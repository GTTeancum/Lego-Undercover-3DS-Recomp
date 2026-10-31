# LEGO City Undercover: The Chase Begins — 3DS Recomp

Recovery repository for an AI-assisted native recompilation effort targeting **LEGO City Undercover: The Chase Begins** (Nintendo 3DS).

## Current rebuilt state

**Resume from [CHECKPOINT.md](CHECKPOINT.md).**

**What improved:** Five more startup sections now run, including the recovered helper from the interrupted session. The rebuild tool also handles a larger routine that crosses a code-page boundary, without changing its original instructions or adding a jump.

**What was checked:** All 5 builds passed their enabled tests; the final build passed 88/88. Fresh headless runs passed their preceding stops. Complete state and save contents match the previous same-mode checkpoint. The entire prior audio capture is preserved byte-for-byte, with four valid silent diagnostic records appended; this is not evidence of speaker playback.

**What still blocks play:** Startup reaches more missing original game code. Menus and gameplay remain unverified. These were headless runs. No fresh game-window run or screenshot was taken. The earlier loading image remains the last verified visible result. Speaker playback and Windows execution remain unverified.

The whole touchscreen stays in the bottom-right corner. **Tab** enlarges it, **F11** toggles fullscreen, and mouse clicks act as touch input. See [desktop build and controls](docs/DESKTOP.md).

**Next:** Add the next 208-instruction helper, which combines guest byte values using stack temporaries, writes row words, restores its working pointer and returns through a stack-loaded PC. Its future memory values and return destination remain unobserved. The exact developer handoff is in [CHECKPOINT.md](CHECKPOINT.md).

Public Git contains source and metadata only. Tested builds, generated code and complete run evidence are preserved privately.

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
