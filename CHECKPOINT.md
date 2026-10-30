# Latest LEGO Undercover 3DS checkpoint

Updated October 30, 2026. **Read this file first.** Resume from current `main`; consult only the first recovery section for detail.

## Plain-English status

This update fills **5 missing pieces of startup code**. The game gets farther through startup in both the windowed and background checks. Menus and gameplay remain unverified.

## Verified developer state

Added **5 blocks / 16 words** since 26/364. Totals: **31 supplements / 380 words**; **111,074 runtime blocks / 545,491 verified words**. All **599 original pages / 603 files** and prior supplements remain unchanged.

Both final modes stop at **MissingBlock `0x001528A0`**. Native-child / outer-launcher exits are **3/3** headless and **3/3** desktop. Neither run timed out or wrote stderr.

From same-mode `9d3806e`: headless **+94 instructions / +350 ns**; desktop **+94 instructions / +350 ns**.

Each new native passed **84/84 suites**, once; actual JUnits have **0 failures/errors/skips**. No permission correction or retry.

Final registers/time and complete audio/state/save contents agree across modes. Desktop still executes **3,883 fewer instructions**; cause untraced.

Whole touchscreen bottom-right: **Tab** enlarges it, **F11** toggles fullscreen; mouse maps to touch. Startup-image comparison is in the latest recovery section; audible sound and Windows remain unverified.

## Next exact task

Resolve admission for **`[0x001528A0, 0x001529B8)` — 1 candidate block / 70 words / 280 bytes**. The actual stop leads into a 70-instruction section ending at its first conditional branch. Existing execution support covers its arithmetic, but nine AND or shifted SUB instructions are outside supplement admission, and the section exceeds the 256-byte size limit. Resolve both limits before adding it.

Reviewed continuations: conditional taken branch to an interior loop entry `0x0015292C` is unrecorded; conditional fallthrough `0x001529B8` is unrecorded. Keep unobserved successor bodies outside this change.

Preserve all 31 supplements. Resolve admission first; then authenticate, generate, verify, build/test, preserve the exact native and use a finite fresh run. No runtime decoder, fabricated GPU completion, replay or automatic retry.

## Continuation rules

Work in standard chat and the cloud; do not use Codex services, Dot or the user’s computer. Ordinary source/metadata publication to `main` is authorized; do not force-push. Keep original inputs, generated code, executables, captures, audio, saves and private recovery identifiers out of public Git. Keep user updates short and in plain English: what improved, whether the game gets farther, and what still blocks play.

Details: [latest recovery section](docs/recovery/CURRENT-REBUILD.md).
