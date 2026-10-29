# LEGO City Undercover: The Chase Begins — 3DS Recomp

Recovery repository for an AI-assisted native recompilation effort targeting **LEGO City Undercover: The Chase Begins** (Nintendo 3DS).

## Current rebuilt state

**The unchanged original game now passes its next startup thread-priority blocker.** An already-executed thread waiting on an address arbiter changes priority successfully without being woken early. The game advances another **113,624 instructions** and reaches a concrete **MissingBlock at `0x00151B48`**. See [the canonical continuation handoff](docs/recovery/CURRENT-REBUILD.md) for the verified state and precise next task.

The October 29 continuation adds bounded **SetThreadPriority** support for running, ready and waiting threads through the shared SVC bridge. It preserves waits, pending wake results and normal scheduling boundaries. Mutex ownership and mutex wait dependencies remain explicitly unsupported until priority inheritance is implemented.

All **79 local suites pass on a fresh recovery build**. One new ordinary run and one observational capture pass **21 strict comparison checks** and a **24-check evidence closure**, including complete matching audio and filesystem state. All **five actual priority calls** succeed; the previously blocked call changes thread **12** from priority **48 to 47** while it remains in `WaitArb`. Hosted CI for code commit `598fe33` passes **3 workflows and 7 jobs**, including **five full 79-suite runs**.

**Recovery provenance:** A workspace reset after code commit [`598fe33`](https://github.com/GTTeancum/Lego-Undercover-3DS-Recomp/commit/598fe338f41d5af57a21e622ca1566bfc5909b59) lost the original milestone logs, raw run/capture evidence and both executables. Those original files remain lost. The new measurements are explicitly labeled recovery reproductions, with different executable hashes and two separately saved private executable archives. They reproduce the recorded stop, complete audio and headless frame export; the tested runtime source is unchanged.

The bounded run uses **131,072 host events**, **`--block-limit 100000000`**, and a **600-second wall timeout**. In diagnostic mode, the block-limit setting bounds instructions between SVC boundaries. It stops at guest time **8.748407941 seconds**, thread **7**, round **21,843**, before the event cap. The last `fs:RomFS` request is historical, not the cause of this missing-code stop.

Actual startup logos and copyright text were rendered in the prior checkpoint. At this later stop, both active headless framebuffers are black after the logos cleared, with LCD force-black disabled. Host-window presentation, usable menus, audible playback and playable gameplay remain unproven.

**Next: verify and translate the missing A32 code region.** Static inspection identifies a shifted flag-setting instruction that the supplement generator currently rejects and a four-way dispatch to further unrecorded entries. Review the instruction-admission policy and complete block graph before adding authenticated supplements. The original **599 AOT pages**, existing **four supplements**, game inputs and prior rendering/save support are unchanged.

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
