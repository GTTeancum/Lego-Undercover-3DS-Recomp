# LEGO City Undercover: The Chase Begins — 3DS Recomp

Recovery repository for an AI-assisted native recompilation effort targeting **LEGO City Undercover: The Chase Begins** (Nintendo 3DS).

## Current rebuilt state

**The native SDL3 window now shows the whole second screen in the bottom-right corner.** Tab enlarges it, F11 toggles fullscreen, and mouse clicks map to the original touchscreen coordinates. The main screen keeps its proportions. See [desktop build, controls and current limits](docs/DESKTOP.md).

Startup now passes the `0x00150A54` dispatch gap. Narrow, independently tested **TST-immediate admission** enables seven authenticated blocks: **29 instruction words / 116 bytes**. Both headless and desktop advance **238 core-0 instructions** beyond their respective previous runs and stop at **MissingBlock `0x00150CE8`**. The original **599 AOT pages**, previous 11 supplement records and game inputs are unchanged; there are now **18 supplements**.

All **84 local test suites pass**, including **26 Python policy cases**, and a separate real X11/software-renderer smoke passes. Both completed game runs agree on final registers, diagnostic guest time, complete audio bytes and current filesystem state. Desktop still reports **3,883 fewer core-0 instructions**; the cause remains untraced. The first desktop attempt encountered a cloud X11 setup failure before guest execution; its outputs are retained separately from the corrected run.

The actual desktop run presents **524 frames**. Its real 1280×720 startup image is byte-identical to the previous capture, with the lower-screen loading indicator visible in the inset. Usable menus, gameplay, speaker playback and a working Windows release remain unproven.

**Next:** add the three reviewed continuation blocks at `0x00150CE8..0x00150D18` (**12 words**), using existing instruction support. The next stack-dependent branch and final return target remain unknown. See [the canonical continuation handoff](docs/recovery/CURRENT-REBUILD.md) for evidence and remaining work.

This continuation restored the complete saved `2fec57b` checkpoint after workspace cleanup. The exact tested executable was saved privately before game testing. Public Git contains source and metadata; game inputs, generated game code, executable bodies, captures and audio remain private.

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
