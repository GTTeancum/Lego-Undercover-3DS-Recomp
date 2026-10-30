# LEGO City Undercover: The Chase Begins — 3DS Recomp

Recovery repository for an AI-assisted native recompilation effort targeting **LEGO City Undercover: The Chase Begins** (Nintendo 3DS).

## Current rebuilt state

**The native SDL3 window shows the whole second screen in the bottom-right corner.** Tab enlarges it, F11 toggles fullscreen, and mouse clicks map to the original touchscreen coordinates. The main screen keeps its proportions. See [desktop build, controls and current limits](docs/DESKTOP.md).

Startup now passes the `0x00150CE8` continuation gap. Three authenticated metadata blocks add **12 instruction words / 48 bytes**, using existing instruction support. Both headless and desktop advance **457 core-0 instructions / 1,704 diagnostic ns** beyond their respective previous runs and stop at **MissingBlock `0x00151604`**. All **599 original AOT pages / 603 files** and the previous 18 supplements remain unchanged; there are now **21 supplements / 271 words**.

The fresh Release build has passing results for all **84 test suites**: 82 passed initially, while two filesystem permission tests passed after an explicit cloud execution-context correction. No source change, rebuild or full-suite rerun was needed. The original failures and both real JUnit reports are preserved. Private generated CPP/JSON records were also checked against the original instructions.

Both fresh native children and their outer launchers report **exit 3**, with no timeout, stderr or live processes left behind. Final registers, diagnostic guest time, complete audio and actual filesystem inventories match across modes. Desktop still reports **3,883 fewer core-0 instructions**; the cause remains untraced.

The desktop presents **524 frames**. Its actual 1280×720 first-nonblack startup image has the same decoded pixels as before, including the lower-screen loading indicator in the inset. The BMP encoding changed from 32 to 24 bits; the exported PNG is identical. Usable menus, gameplay, speaker playback and a working Windows release remain unverified.

**Next:** add one reviewed block at `0x00151604..0x001516CC` (**50 words**), using existing memory, shifted-ORR and fast instruction support. Its loaded bytes are unknown, and its return to the supplied continuation remains conditional on successful memory accesses. See [the canonical continuation handoff](docs/recovery/CURRENT-REBUILD.md).

The exact new executable and raw build evidence were saved before the fresh game runs. Each closed capture, including its real state and save bodies, is saved separately. Earlier workspace-loss limitations remain recorded in the historical handoff. Public Git contains source and metadata; game inputs, generated game code, executable bodies, captures and audio stay private.

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
