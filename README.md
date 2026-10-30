# LEGO City Undercover: The Chase Begins — 3DS Recomp

Recovery repository for an AI-assisted native recompilation effort targeting **LEGO City Undercover: The Chase Begins** (Nintendo 3DS).

## Current rebuilt state

**The native SDL3 window now shows the whole second screen in the bottom-right corner.** Tab enlarges it, F11 toggles fullscreen, and mouse clicks map to the original touchscreen coordinates. The main screen keeps its proportions. See [desktop build, controls and current limits](docs/DESKTOP.md).

The actual original-game desktop run presents **524 frames** and saves a real 1280×720 startup image, with the lower-screen loading indicator visible in the inset. This establishes live host-window presentation. Usable menus, gameplay, speaker playback and a working Windows release remain unproven.

The next two A32 blocks are also translated: **44 instruction words / 176 bytes**, using the existing admission policy. The headless game advances **282 core-0 instructions** through the loop and stack return to a new **MissingBlock at `0x00150A54`**. The original **599 AOT pages**, original nine supplement records and game inputs are unchanged; there are now **11 supplements**.

All **84 local test suites pass**, and a separate real X11/software-renderer smoke passes. Headless and desktop reach the same stop, final registers, diagnostic guest time, complete audio bytes and filesystem state. Desktop enables real HID sampling and reports **3,883 fewer core-0 instructions**; the cause has not been traced, and instruction-for-instruction equivalence is not claimed.

**Next:** review the narrow TST-immediate admission needed at `0x00150A54`, add independent policy/semantic tests, then translate its bounded dispatch region. See [the canonical continuation handoff](docs/recovery/CURRENT-REBUILD.md) for evidence and remaining work.

This continuation uses the intact `8fd18d4` checkpoint. The exact tested executable was saved privately before both game runs. Public Git contains source and metadata; game inputs, generated game code, executable bodies, captures and audio remain private.

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
