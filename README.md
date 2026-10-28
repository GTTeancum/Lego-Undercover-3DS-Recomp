# LEGO City Undercover: The Chase Begins — 3DS Recomp

Recovery repository for an AI-assisted native recompilation effort targeting **LEGO City Undercover: The Chase Begins** (Nintendo 3DS).

## Current rebuilt state

The current source reconstructs original-game startup in a native, headless runtime and now executes bounded PICA color draws. No visible game scene, menu, or playable gameplay is demonstrated. See [the canonical continuation handoff](docs/recovery/CURRENT-REBUILD.md) for the verified state, reproduction commands, and next work.

The latest October 28 continuation adds one authenticated six-instruction ARM block, bounded texture0 sampling, D24S8 depth testing/writes, and rejection of triangles proven wholly outside one clip plane. Original-game startup advances from dispatch round **1469 to 1529**. The pending GPU command list now gets past its first textured draw at byte **0x3F0** and the wholly off-screen triangles at **0x668**. It stops at a later texture configuration at **0xB58**. Supporting that observed configuration is the next task. The queue remains uncommitted when the later draw is rejected.

Texture0 supports tiled RGBA8, base level zero, clamp-to-edge and matching nearest or linear min/mag filters. Depth supports D24S8 Z buffering with stencil preserved. Texture sources use bounded private-memory or ordered VRAM snapshots, and color/depth updates remain part of the complete queue transaction. Filtering and depth precision are explicitly bounded emulator approximations.

All **79 local runtime suites pass**. The captured first draw and a seeded-VRAM variant match independently calculated complete 6 MiB outputs, including all depth results, stencil bytes and untouched regions. The off-screen draw now succeeds with zero pixel work, zero patches and all VRAM unchanged. This original first draw has zero final alpha, so it preserves color; authored nonuniform textures and nonzero alpha separately test filtering and visible color blending. General clipping intersections remain unsupported. Exact-commit hosted checks are recorded separately.

The earlier SaveData, memory-cleanup, color-draw and cropped-transfer milestones remain in place. Their evidence and limitations are retained in the chronological handoff. **No visible game scene, menu, or gameplay is demonstrated.**

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
