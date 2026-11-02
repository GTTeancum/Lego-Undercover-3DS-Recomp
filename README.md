# LEGO City Undercover: The Chase Begins — 3DS Recomp

## Current rebuilt state

The goal is a full playable native recompilation of LEGO City Undercover: The Chase Begins. This batch adds **12 reviewed logical sections containing 64 original instructions**, reaching **255 logical sections / 257 physical blocks / 3,744 supplemental instruction words**. Three fresh native startup runs advance **120,851 core0 instructions** beyond published `0e4737b` and stop at **MissingBlock `0x0014F620`**.

The final build of each candidate passed **90/90 existing tests**. All three actual native runs completed with the recorded MissingBlock exit 3, no timeout or stderr, and unchanged original inputs and AOT files. Earlier test-environment, output and evidence-permission failures were preserved and reconciled; see the checkpoint for their exact scope.

Menus, gameplay, game rendering, speaker playback and Windows execution remain unverified. No new window or screenshot was captured. Diagnostic counters and cloud elapsed times do not establish frame rate or gameplay completion.

At source preparation, hosted checks for this change had not yet run. Their actual results are attached to the published commit in [project Actions](https://github.com/GTTeancum/Lego-Undercover-3DS-Recomp/actions). Hosted ROM-free checks do not execute the original game.

The next blocker is `0x0014F620`; one logical-section slot remains. A larger reviewed group needs a separately verified capacity change before admission. See the [current checkpoint](CHECKPOINT.md) and [complete previous status](https://github.com/GTTeancum/Lego-Undercover-3DS-Recomp/blob/0e4737b5eba2d09ed6002daffd44d2650b06bf6a/README.md).

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
