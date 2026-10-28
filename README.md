# LEGO City Undercover: The Chase Begins — 3DS Recomp

Recovery repository for an AI-assisted native recompilation effort targeting **LEGO City Undercover: The Chase Begins** (Nintendo 3DS).

## Current rebuilt state

The current source reconstructs original-game startup in a native, headless runtime and now executes bounded PICA color draws. No visible game scene, menu, or playable gameplay is demonstrated. See [the canonical continuation handoff](docs/recovery/CURRENT-REBUILD.md) for the verified state, reproduction commands, and next work.

The latest October 28 continuation adds an explicit per-title SaveData backend. With a fresh, explicitly selected save container, the original game receives the real missing-save result and advances from dispatch round 1203 to round 1277. The next observed stop is kernel `ControlMemory` operation 5 (Unmap), for a 0x2000-byte alias; no subsequent save-format request was observed.

`--save-data-root DIR` selects an existing container, separate from shared extdata. The initialized FS session's program ID chooses `<high8>/<low8>/data/00000001`. Opening creates nothing: an absent save returns NotFormatted and a zero handle; a real existing directory receives a distinct u64 SaveData handle. No-follow directory traversal and separate handle namespaces preserve backend isolation. SaveData file operations and formatting remain unsupported.

The eight focused filesystem suites pass, including the new SaveData suite; the repository now registers 78 runtime suites. An independent private capture verifies the original missing-save response, unchanged save storage, all 67 FS request/response pairs, and ordinary/capture agreement through the next kernel stop. Exact-commit hosted checks are recorded separately.

The preceding rendering milestone remains intact: bounded native PICA draws and cropped RGBA8-to-RGB8 transfers execute, and private independent checks verify the original first rectangle and seeded VRAM updates. No visible game scene, menu, or gameplay is demonstrated.

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
