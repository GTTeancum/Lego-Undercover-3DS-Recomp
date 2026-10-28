# LEGO City Undercover: The Chase Begins — 3DS Recomp

Recovery repository for an AI-assisted native recompilation effort targeting **LEGO City Undercover: The Chase Begins** (Nintendo 3DS).

## Current rebuilt state

The current source reconstructs original-game startup in a native, headless runtime and now executes bounded PICA color draws. No visible game scene, menu, or playable gameplay is demonstrated. See [the canonical continuation handoff](docs/recovery/CURRENT-REBUILD.md) for the verified state, reproduction commands, and next work.

The latest October 28 continuation adds **ETC1/ETC1A4 compressed textures and positive-W homogeneous triangle clipping**. The unchanged original game now passes the GPU request that blocked dispatch round **1529** and reaches round **1626**. Its next unsupported operation is **fs:USER FormatSaveData**, request header `0x084C0242`, which is the next implementation task. The preceding IA8, authenticated ARM entry, depth and whole-triangle rejection milestones remain in place.

Texture0 supports tiled RGBA8, IA8, ETC1 and ETC1A4, base level zero, clamp-to-edge and matching nearest or linear min/mag filters. Each format retains its actual encoded source span for memory and feedback checks. Clipping interpolates position, color and enabled UV before perspective division, preserves visible portions of crossing triangles, and bounds polygon growth and raster work. Depth supports D24S8 Z buffering with stencil preserved. Texture reads and color/depth writes remain part of the complete queue transaction. Clipping, filtering and depth precision are explicitly bounded emulator models.

All **79 local suites pass** after the combined change. Independent ETC1A4 sampling checks cover 4,505 queries and every byte of the retained compressed texture. A separate original-draw replay clips two input triangles into three fan triangles and verifies all **6 MiB of resulting VRAM** against an independent arithmetic oracle: 3,852 pixel writes change 2,110 RGB bytes while preserving depth, stencil, destination alpha and uncovered bytes. The selective original capture verifies all **eight draws in accepted queue 250**, its complete committed GPU/VRAM result, and full CPU/audio equivalence with ordinary execution. Exact-commit hosted results are recorded separately. These checks establish bounded execution and memory results, without demonstrating a visible frame.

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
