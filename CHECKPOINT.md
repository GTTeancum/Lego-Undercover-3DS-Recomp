# Latest LEGO Undercover 3DS checkpoint

Updated October 30, 2026. **Read this file first.** Resume from current `main`; consult only the first recovery section when more detail is needed.

## Completed

Added **4 blocks / 43 words** since 22/321. Totals: **26 supplements / 364 words**; **111,069 runtime blocks / 545,475 verified words**. All **599 original pages / 603 files** and prior supplements remain unchanged.

Both final modes stop at **MissingBlock `0x00150E88`**. Native-child / outer-launcher exits are **3/3** headless and **3/3** desktop. Neither run timed out or wrote stderr.

From the same-mode `c1c85af` baseline: headless **+65 instructions / +243 ns**; desktop **+65 instructions / +243 ns**.

Final native: **84/84 enabled suites**, one full CTest invocation; actual JUnit **0 failures/errors/skips**. No permission correction or retry.

Final registers/time and complete audio/state/save contents agree across modes. Phase-2 still has **3,883 fewer desktop instructions**; untraced.

Desktop: **524 presentations**; actual **1280×720** first-nonblack startup pixels match the baseline. Menus, gameplay, speaker playback and Windows remain unverified.

Whole touchscreen bottom-right: **Tab** enlarges it, **F11** toggles fullscreen; mouse maps to touch.

## Next exact task

Append only **`[0x00150E88, 0x00150E94)` — 1 block / 3 words / 12 bytes**. Three original instructions form one bounded table dispatcher using existing fast and memory evaluators. Keep its table words as data and preserve ordinary memory and target validation.

Reviewed continuations: computed target if authenticated table read succeeds `0x00152AFC` is recorded supplement; captured unchanged LR; eventual return remains conditional `0x00150E94` is unrecorded. Keep unobserved successor bodies outside this change.

Preserve all 26 supplements. Authenticate, generate, verify, build/test, preserve the exact native, then use a finite fresh run. No runtime decoder, fabricated GPU completion, replay or automatic retry.

## Continuation rules

Work in standard chat and the cloud; do not use Codex services, Dot or the user’s computer. Ordinary source/metadata publication to `main` is authorized; do not force-push. Keep original inputs, generated code, executables, captures, audio, saves and private recovery identifiers out of public Git.

The four existing status documents keep the tracked file count at **522**. Details: [latest recovery section](docs/recovery/CURRENT-REBUILD.md).
