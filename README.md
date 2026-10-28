# LEGO City Undercover: The Chase Begins — 3DS Recomp

Recovery repository for an AI-assisted native recompilation effort targeting **LEGO City Undercover: The Chase Begins** (Nintendo 3DS).

## Current rebuilt state

The current source reconstructs original-game startup in a native, headless runtime and now executes bounded PICA color draws. No visible game scene, menu, or playable gameplay is demonstrated. See [the canonical continuation handoff](docs/recovery/CURRENT-REBUILD.md) for the verified state, reproduction commands, and next work.

The latest October 28 continuation completes the original GPU draw at dispatch round 843 and the unscaled framebuffer transfer reached at round 1109. With an explicitly larger diagnostic event budget, the unchanged original game advances to round 1203, where `fs:USER` requests `OpenArchive` for save data (archive ID 4). That filesystem operation is the next unsupported behavior.

The new renderer connects the existing native vertex loader and shader executor to triangle assembly, perspective color interpolation, texture-disabled TEV, alpha blending, and masked writes into tiled RGBA8 VRAM. Draw bytes, registers, queue retirement, and interrupts remain part of one staged queue transaction. DisplayTransfer `0x1004` now converts a cropped tiled RGBA8 buffer to linear RGB8 without horizontal averaging.

Independent private checks verify every pixel of the original 192,000-pixel first draw and all 6 MiB of VRAM on both captured and arbitrary seeded backgrounds. The original target was already black; the seeded checks establish actual blend and mask behavior. All 77 local runtime suites pass. Hosted checks and private original-game evidence are recorded separately in the continuation receipt.

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
