# LEGO City Undercover: The Chase Begins — 3DS Recomp

Recovery repository for an AI-assisted native recompilation effort targeting **LEGO City Undercover: The Chase Begins** (Nintendo 3DS).

## Current rebuilt state

The current source reconstructs original-game startup in a native, headless runtime and now executes bounded PICA color draws. No visible game scene, menu, or playable gameplay is demonstrated. See [the canonical continuation handoff](docs/recovery/CURRENT-REBUILD.md) for the verified state, reproduction commands, and next work.

The latest October 28 continuation adds **IA8 grayscale/alpha textures** to the existing bounded texture0 pipeline. The pending GPU command list now stages its first six draws and reaches **ETC1A4 compressed textures at byte 0x12C0**, which is the next task. This advances within the same request at dispatch round **1529**; the queue remains uncommitted when that later draw is rejected. The preceding authenticated ARM entry, depth support and whole-triangle clip rejection remain in place.

Texture0 supports tiled RGBA8 and IA8, base level zero, clamp-to-edge and matching nearest or linear min/mag filters. IA8 retains its exact two-byte encoded source span and decodes intensity and alpha independently before filtering. Depth supports D24S8 Z buffering with stencil preserved. Texture sources use bounded private-memory or ordered VRAM snapshots, and color/depth updates remain part of the complete queue transaction. Filtering and depth precision are explicitly bounded emulator approximations.

The three affected sampler, rasterizer and queue suites pass after the IA8 addition. The complete **79-suite** configuration remains unchanged; exact-commit hosted results are recorded separately. IA8 replay verifies all 65,536 original texel centers, 2,876 fractional/clamp probes and all 2,640 selected shader components against independent calculations. Its original geometry is degenerate and produces no pixels, so authored nondegenerate draws supply separate color/blending coverage. Earlier complete-VRAM depth proofs remain preserved. General clipping intersections remain unsupported.

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
