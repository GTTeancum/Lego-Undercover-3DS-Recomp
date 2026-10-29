# LEGO City Undercover: The Chase Begins — 3DS Recomp

Recovery repository for an AI-assisted native recompilation effort targeting **LEGO City Undercover: The Chase Begins** (Nintendo 3DS).

## Current rebuilt state

The current source reconstructs original-game startup in a native, headless runtime, executes bounded PICA draws, and now writes, acknowledges, closes and reopens the game's initial SaveData archive. See [the canonical continuation handoff](docs/recovery/CURRENT-REBUILD.md) for the verified state, reproduction commands, and next work.

The latest October 28 continuation adds the observed **ControlArchive commit acknowledgement and CloseArchive**. The unchanged original game preserves its **42,644-byte application file**, retires the first archive mount, opens a fresh mount and reopens the existing file. It advances from dispatch round **5349 to 8275**, where **fs:File Read**, header `0x080200C2`, is unsupported. **The next task is that file read; it has not executed.** Captured filenames, transport and game bytes remain private.

ControlArchive follows the pinned local SaveData reference behavior: return success and a zero output byte. It adds no filesystem write, journal, flush or power-loss guarantee. CloseArchive retires only the initialized title's mount; file contents, metadata and independent file sessions survive. Both commands prepare their private guest replies before any state change.

All **79 local suites pass** for the combined change. One ordinary run and one selective capture agree in CPU/stdout, complete filesystem inventories and all **1,417,152 audio bytes**. The actual FS sequence passes **596 proof checks**; portable integrity covers **230 files and all 188 capture members**. Earlier Control-only build interruption and executable-relink records remain intact; the final combined gate passes. Exact-revision hosted CI results are recorded separately.

At this later stop, **both actual screen framebuffers are black**, with LCD force-black controls disabled. The earlier save-preparation screenshot belongs to the preceding milestone. Host-window presentation, a usable menu, audible playback and playable gameplay remain unproven.

The earlier SaveData formatting and file growth, ETC1/ETC1A4 textures, positive-W clipping, IA8, depth, authenticated ARM entry and memory-cleanup milestones remain in place. GPU code and original game inputs are unchanged by this filesystem milestone. Their evidence and limitations remain in the chronological handoff.

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
