# LEGO City Undercover: The Chase Begins — 3DS Recomp

Recovery repository for an AI-assisted native recompilation effort targeting **LEGO City Undercover: The Chase Begins** (Nintendo 3DS).

## Current rebuilt state

The current source reconstructs original-game startup in a native, headless runtime, executes bounded PICA draws, and now supports reading the game's initial SaveData file after writing and reopening it. See [the canonical continuation handoff](docs/recovery/CURRENT-REBUILD.md) for the verified state, reproduction commands, and next work.

The latest October 28 continuation adds **bounded SaveData file Read**. The unchanged original game passes the prior file-read stop at dispatch round **8275** and reaches **round 10271**, where `gsp::Gpu` stops at a texture0 validation guard during command-queue processing. **Next: retain the pending command list and reconstruct the texture state at the rejected draw.** The current capture retains the last committed registers, which do not establish the failed draw's effective state.

Reads use the live pinned file descriptor, stage data privately, return the actual byte count and preserve bytes beyond a short read. The complete destination and reply are prepared before guest data is published. Existing SaveData ownership and lifetime rules remain intact; SharedExtData's prior operation support is unchanged. File contents and captured requests remain private.

All **79 local suites pass**. The ordinary run retains the existing **65,536 host-event budget**, original inputs and other finite limits. One selective capture matches the ordinary run in CPU/stdout, complete filesystem inventories and all **1,872,192 audio bytes**. The actual successful Read returns **42,644 bytes**; **693 semantic checks** pass, and portable integrity covers **264 files/all 216 capture members**. The destination already matched the file, so actual reply/count, frozen read implementation and separate sentinel tests establish the result. Exact-revision hosted CI results are recorded separately.

At the current stop, **both actual screen buffers are black**, with LCD force-black controls disabled. The earlier save-preparation screenshot remains historical evidence. Host-window presentation, a usable menu, audible playback and playable gameplay remain unproven.

The earlier SaveData formatting, growth, commit acknowledgement and archive closing, ETC1/ETC1A4 textures, positive-W clipping, IA8, depth, authenticated ARM entry and memory-cleanup milestones remain in place. GPU code is unchanged by this filesystem milestone. Their evidence and limitations remain in the chronological handoff.

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
