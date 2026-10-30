# LEGO City Undercover: The Chase Begins — 3DS Recomp

Recovery repository for an AI-assisted native recompilation effort targeting **LEGO City Undercover: The Chase Begins** (Nintendo 3DS).

## Current rebuilt state

**Resume from [CHECKPOINT.md](CHECKPOINT.md).** It contains the latest state and next task; the longer recovery document preserves older evidence.

The native SDL3 window shows the whole second screen in the bottom-right corner. Tab enlarges it, F11 toggles fullscreen, and mouse clicks map to the original touchscreen coordinates. See [desktop build and controls](docs/DESKTOP.md).

The continuation adds **4 metadata blocks / 43 instruction words** beyond the prior 22 blocks / 321 words. Totals are **26 supplements / 364 words**, with **111,069 registered blocks / 545,475 verified instruction words**. All **599 original AOT pages / 603 files** and all 22 preceding supplement records remain unchanged.

Both final modes stop at **MissingBlock `0x00150E88`**. Native-child / outer-launcher exits are **3/3** headless and **3/3** desktop. Neither run timed out or wrote stderr.

Cumulative change from the preserved same-mode `c1c85af` baseline: headless **+65 core-0 instructions / +243 diagnostic ns**; desktop **+65 core-0 instructions / +243 diagnostic ns**.

The exact final native passed **84/84 enabled CTest suites** in its single full-suite invocation. The actual final JUnit records **0 failures, 0 errors and 0 skipped cases**. No test permission correction or automatic retry was used for that final build.

Cross-mode comparison matches final registers, diagnostic time, complete audio, state contents and save contents. The phase-2 comparison still records **3,883 fewer desktop core-0 instructions** than headless; the difference remains untraced.

The desktop records **524 presentations**. The actual **1280×720 first-nonblack startup capture** has identical decoded RGBA pixels to the original baseline; complete BMP bytes are different. This records the initial startup image. Usable menus, gameplay, speaker playback and Windows execution remain unverified.

**Next:** Append only **`[0x00150E88, 0x00150E94)` — 1 block / 3 words / 12 bytes**. Three original instructions form one bounded table dispatcher using existing fast and memory evaluators. Keep its table words as data and preserve ordinary memory and target validation. See the checkpoint for reviewed continuation coverage.

Public Git contains source and metadata. The exact tested native was preserved before its finite fresh runs; original inputs, generated code, executables, captures, saves and audio remain private.

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
