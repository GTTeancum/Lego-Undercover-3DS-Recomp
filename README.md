# LEGO City Undercover: The Chase Begins — 3DS Recomp

Recovery repository for an AI-assisted native recompilation effort targeting **LEGO City Undercover: The Chase Begins** (Nintendo 3DS).

## Current rebuilt state

The current source reconstructs original-game startup in a native, headless runtime and now executes bounded PICA color draws. No visible game scene, menu, or playable gameplay is demonstrated. See [the canonical continuation handoff](docs/recovery/CURRENT-REBUILD.md) for the verified state, reproduction commands, and next work.

The latest October 28 continuation adds explicit per-title SaveData opening and completes the observed private-memory cleanup sequence. The unchanged original game advances from dispatch round 1203 to **1469**, where execution reaches a missing translated ARM block at **0x00133788**. Extending authenticated build-time AOT coverage is the next task.

`--save-data-root DIR` selects an existing container, separate from shared extdata. Missing title saves return the real NotFormatted result without creating files; existing directories receive distinct u64 handles. The memory runtime now removes complete recorded aliases while restoring source read/write access, and accepts a bounded private Protect request when the entire span already has the requested permissions.

All **78 local runtime suites pass**. Private original-game checks verify the missing-save reply, the exact alias teardown with all 8192 bytes preserved, and the following Protect with all 1024 reservation-epoch tokens unchanged. Ordinary and diagnostic runs match CPU state and startup output. Full diagnostic-audio equivalence remains incomplete because two capture files were truncated; their failed artifacts and explicit scope limits are retained. Exact-commit hosted checks are recorded separately.

The preceding rendering milestone remains intact: bounded native PICA draws and cropped RGBA8-to-RGB8 transfers execute, with independent first-rectangle and seeded-VRAM checks. **No visible game scene, menu, or gameplay is demonstrated.**

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
