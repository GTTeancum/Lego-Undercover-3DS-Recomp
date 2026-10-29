# LEGO City Undercover: The Chase Begins — 3DS Recomp

Recovery repository for an AI-assisted native recompilation effort targeting **LEGO City Undercover: The Chase Begins** (Nintendo 3DS).

## Current rebuilt state

**The next five missing A32 blocks are translated, and the original game passes the dispatch at `0x00151B48`.** Its actual path executes all **30 instructions** through the selected leaf and returns normally. Startup advances another **96 core-0 instructions**, reaching a new **MissingBlock at `0x001535E0`**. See [the canonical continuation handoff](docs/recovery/CURRENT-REBUILD.md) for current evidence and the next two mapped blocks.

The October 29 continuation adds a narrow build-time admission rule for existing **MOV, ADD and ORR register operations with immediate shifts**, plus **five authenticated supplements / 167 instruction words**. The literal jump table remains data. The original **599 AOT pages**, original four supplement instruction records, runtime semantics and game inputs are unchanged; there are now **nine supplements**.

All **80 local test suites pass**, including a new Python policy suite with **17 cases** and expanded independent C++ shift/flag/dispatch fixtures. One ordinary game run and one read-only capture pass **21 strict comparison checks**, including complete audio and filesystem state. A separate verifier checks every first-visit PC/result transition and serialized CPU-state boundary, the actual table selection and return, and all three observed newly admitted operations against independent arithmetic expectations. The other three dispatch arms have static and synthetic-test coverage; their execution is not claimed.

The bounded runs use **131,072 host events**, **`--block-limit 100000000`** and a **600-second wall timeout**, with fresh owned SaveData and SharedExtData. The current stop is guest time **8.748408299 seconds**, thread **7**, round **21,843**. These are diagnostic scheduling measurements, not hardware timing. The last `fs:RomFS` request is historical.

Actual startup logos and copyright text were rendered earlier. At this stop, both active headless framebuffers remain black after the logos cleared, with LCD force-black disabled. Host-window presentation, usable menus, audible playback and playable gameplay remain unproven.

**Next: translate the two mapped blocks at `0x001535E0..0x00153690`.** They fit the current opcode policy and contain a bounded loop followed by POP-to-PC. The loaded return address remains unknown; no forced return or substitution from live LR is justified. This is a static follow-up plan, not an executed translation.

This continuation used the intact checkpoint at `5b4933e`; both new executables were saved privately before their game runs. The earlier priority milestone's lost original evidence remains a historical limitation, recorded in the handoff. Public Git contains source and metadata only; game-derived code, executables, captures and audio remain private.

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
