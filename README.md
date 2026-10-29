# LEGO City Undercover: The Chase Begins — 3DS Recomp

Recovery repository for an AI-assisted native recompilation effort targeting **LEGO City Undercover: The Chase Begins** (Nintendo 3DS).

## Current rebuilt state

The current source reconstructs original-game startup in a native, headless runtime, executes bounded PICA draws, and now creates and writes the game's initial SaveData file. See [the canonical continuation handoff](docs/recovery/CURRENT-REBUILD.md) for the verified state, reproduction commands, and next work.

The latest October 28 continuation adds **SaveData file opening and growing writes**. The unchanged original game opens a real empty application file, writes **42,644 bytes**, and closes it. It passes the previous OpenFile stop at dispatch round **5118** and reaches **fs:USER ControlArchive**, header `0x080D0144`, at round **5349**. The next task is the observed archive-commit acknowledgement; the captured request, filename and save contents remain private.

SaveData read/write/create opening preserves existing bytes and publishes a missing file atomically without replacement after its title binding, path, endpoint, handle capacity and reply are prepared. File size follows the real descriptor, and writes can extend it within an explicit host bound. SharedExtData retains its existing fixed-extent behavior. These bounded host-file semantics do not claim a console journal, hardware quota or power-loss durability.

All **79 local suites pass**. The first ordinary run wrote the file before reaching the existing **16,384 host-event limit**. A separately recorded run raised only that finite budget to **65,536** and reached the next unsupported command with the same native binary, original inputs and other limits. One selective capture matches that second run in CPU/stdout, complete filesystem inventories, and all **970,944 audio bytes**. A separate read-only correction resolves inventory ordering in the initial proof; the corrected proof passes **276 checks**, with the original failure retained. Exact-revision hosted CI results are recorded separately.

The latest private export of the actual framebuffer still shows a **save-preparation notice on the lower screen**; the top screen is black. This establishes headless framebuffer contents. Host-window presentation, a usable menu and playable gameplay remain unproven.

The earlier first-time format, ETC1/ETC1A4 textures, positive-W clipping, IA8, depth, authenticated ARM entry and memory-cleanup milestones remain in place. GPU code and original game inputs are unchanged by this filesystem milestone. Their evidence and limitations remain in the chronological handoff.

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
