# LEGO City Undercover: The Chase Begins — 3DS Recomp

Recovery repository for an AI-assisted native recompilation effort targeting **LEGO City Undercover: The Chase Begins** (Nintendo 3DS).

## Current rebuilt state

**The unchanged original game now renders its startup logos and copyright screen into the actual headless framebuffers.** The latest private lossless export shows WB Games, TT Games, LEGO and Nintendo branding on the top screen and copyright text on the bottom. See [the canonical continuation handoff](docs/recovery/CURRENT-REBUILD.md) for the verified state, reproduction commands, and next work.

The latest October 28 continuation adds **RGB8 textures and independent repeat wrapping**, with either mip-filter selector supported under the existing zero-LOD/equal-filter restriction. A bounded diagnostic first established the rejected draw's effective texture state from its real command list. With the fix, the same list and identical pre-queue GPU/GSP/VRAM/register-knownness state complete successfully: **2 draws**, **4 triangles**, **384,000 raster pixel writes**, and **103,345 changed VRAM bytes**. Captured game commands, assets and screen images remain private.

The ordinary run passes the previous texture stop at dispatch round **10,271** and reaches **round 18,082**, guest time **7.100531896 seconds**, before the configured **65,536 host-event limit**. There is no current unsupported IPC at this stop. **Next: extend bounded validation beyond the startup logos to identify the next real blocker**, allowing for the slow host runtime while retaining explicit finite limits. This budget-limited snapshot does not establish that the game is stuck at the logos.

All **79 local suites pass**. Authored tests cover repeat seams, negative and large coordinates, RGB8's exact encoded extent, opaque alpha, base-only mip-filter equivalence, queue pixel results, dependencies and rollback. The ordinary run retains the existing **100 million block limit** and **240-second timeout**. One final selective capture passes **21 strict comparison checks** against that run, including all **3,708,736 audio bytes / 231,794 records**; **13 queue-proof checks** pass and all **231 capture members** are retained. Exact-revision hosted CI results are recorded separately.

The exported top and bottom buffers contain **26,722** and **9,287 nonblack pixels**, respectively, with LCD force-black controls disabled. This establishes actual startup-screen rendering in the headless runtime. Host-window presentation, usable menus, audible playback and playable gameplay remain unproven.

The prior SaveData formatting, file growth, commit acknowledgement, archive closing and reading remain in place, alongside earlier ETC1/ETC1A4, clipping, IA8, depth and authenticated ARM-entry work. Their evidence, preserved failures and limitations remain in the chronological handoff.

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
