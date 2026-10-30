# Latest LEGO Undercover 3DS checkpoint

Updated October 30, 2026. Read this first and resume from `main`.

## Plain-English status

Five more startup gaps are filled, including the byte-processing loop and its return. Startup now runs through that routine and reaches a larger unfinished helper. Each of the 5 fresh game runs passed its preceding stopping point. Menus and gameplay remain unverified.

These were headless runs. There is no new screenshot or visual-progress claim; the earlier loading image remains the last verified windowed result.

## Verified developer state

Base: `fb309a5`. Added 5 supplemental blocks / 38 words. Totals: **49 supplements / 699 words**, **111,092 runtime blocks / 545,810 verified words**. All 599 original pages / 603 files and all 44 preceding supplemental records are preserved. Runtime, generator, tests and build configuration are unchanged.

Final headless stop: **MissingBlock `0x00151D10`**. Relative to the same-mode baseline: **+4,443 executed core-0 instructions / +16,572 diagnostic ns**. Every run ended at missing code with native exit 3, no timeout and empty stderr. Complete captured audio, state and save contents match the baseline.

Each new build passed **84/84 enabled tests once**, with zero failures/errors/skips. Its exact executable was saved privately before its fresh run. These checks do not establish a successful game boot.

## Next exact task

Next candidate: **`[0x00151D10, 0x00151F68)`**, one block / 150 words / 600 bytes. The next helper combines guest byte values into row words and returns through the stack. It requires a tested increase to the size limit before the rebuild tool can include it. All instruction forms are individually supported, but the complete interval exceeds the current 512-byte generation limit. Add a tested size-policy extension first; do not bypass its bounds or fabricate a branch to split the routine. Successor execution remains unobserved; do not add unobserved bodies or manufacture a return target.

Preserve all 49 supplements. Authenticate, generate and verify exact words, build/test, preserve the exact native, and make a finite fresh run. No runtime decoding, forced returns, fabricated GPU completion or automatic retries.

## Continuation rules

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Publish source and metadata to `main` without force-pushing. Keep game inputs, generated game code, executables, captures, audio, saves and private recovery identifiers out of public Git. The user authorized ongoing private builds and recovery backups. Keep user summaries short and plain.

Details: [current recovery record](docs/recovery/CURRENT-REBUILD.md).
