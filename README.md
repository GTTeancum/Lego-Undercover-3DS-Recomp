# LEGO City Undercover: The Chase Begins — 3DS Recomp

Recovery repository for an AI-assisted native recompilation effort targeting **LEGO City Undercover: The Chase Begins** (Nintendo 3DS).

## Current rebuilt state

**Resume from [CHECKPOINT.md](CHECKPOINT.md).**

**What improved:** Eight more observed startup sections now execute. Additional row-processing loops, their register and stack returns, and a caller entry resume original control flow. Startup advances another 147,594 instructions from `7f3f4c7` to MissingBlock `0x0014F608`.

**Checks:** Eight accepted headless Release candidates each passed all 90 enabled tests, with explicit successful build/test exits and complete passing JUnit reports. Four partial-state predictions matched. All accepted startup runs retain complete captures and unchanged state/save contents; the final capture preserves the entire baseline prefix and adds 16 valid zero-sample records. The checkpoint records a recovered launch-permission failure and a separate backup-integrity failure, while preserving incomplete diagnostic text logs. Published-commit hosted results are recorded separately.

**Still unfinished:** Menus, gameplay, speaker playback and Windows execution remain unverified. No new screen capture or windowed run was performed. The next reviewed two-instruction stack-save/call interval remains unadmitted.

The optional desktop keeps the whole touchscreen at bottom right. See [desktop build and controls](docs/DESKTOP.md). Black diagnostic captures remain private.

Public Git contains source and metadata; original inputs, tested natives, generated instructions and full runtime evidence remain private.

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
