# Latest LEGO Undercover 3DS checkpoint

Updated October 30, 2026. **Read this first.** Resume from `main`; details are in the latest recovery section.

## Plain-English status

The rebuild now accepts the required arithmetic and larger code sections. It adds **2 missing sections of startup code**. The game gets farther through startup in both the windowed and background checks. Menus and gameplay remain unverified.

## Verified developer state

Added **2 blocks / 73 words** since 31/380. Totals: **33 supplements / 453 words**; **111,076 runtime blocks / 545,564 verified words**. All **599 original pages / 603 files** and prior supplements remain unchanged.

Both final modes stop at **MissingBlock `0x00150EC0`**. Native-child / outer-launcher exits are **3/3** headless and **3/3** desktop. Neither run timed out or wrote stderr.

From same-mode `7cf0d2a`: headless **+178 instructions / +664 ns**; desktop **+178 instructions / +664 ns**.

Each native passed **84/84 suites once**, with **0 failures/errors/skips** and no retries or permission corrections.

Final registers/time and complete audio/state/save contents agree across modes. Desktop still executes **3,883 fewer instructions**; cause untraced.

Whole touchscreen bottom-right: **Tab** enlarges it, **F11** toggles fullscreen; mouse maps to touch. See recovery for startup-image comparisons; audible sound and Windows remain unverified.

## Next exact task

Append only **`[0x00150EC0, 0x00150ED0)` — 1 block / 4 words / 16 bytes**. Four original instructions advance a working pointer, restore two stack values, adjust an argument, and call an already recorded routine. Existing fast and memory handlers cover the section; its later return remains unobserved.

Reviewed continuations: direct call after successful stack reads `0x00150DDC` is recorded original; assigned LR; eventual return remains conditional `0x00150ED0` is unrecorded. Keep unobserved successor bodies outside this change.

Preserve all 33 supplements. Authenticate, generate, verify, build/test, preserve the exact native, then use a finite fresh run. No runtime decoder, fabricated GPU completion, replay or automatic retry.

## Continuation rules

Work in standard chat and the cloud; do not use Codex services, Dot or the user’s computer. Ordinary source/metadata publication to `main` is authorized; do not force-push. Keep original inputs, generated code, executables, captures, audio, saves and private recovery identifiers out of public Git. Keep user updates short and in plain English: what improved, whether the game gets farther, and what still blocks play.

Details: [latest recovery section](docs/recovery/CURRENT-REBUILD.md).
