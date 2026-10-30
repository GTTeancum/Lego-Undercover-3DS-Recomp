# Latest LEGO Undercover 3DS checkpoint

Updated October 30, 2026. Read this first and resume from `main`.

## Plain-English status

Three more startup gaps are cleared. The rebuild tool now accepts a required kind of arithmetic instruction, allowing the larger helper to run. Fresh background runs get past the old stop. Menus and gameplay remain unverified.

This checkpoint was tested without a desktop window. There is no new screenshot or visual-progress claim; the last verified windowed result remains the loading image.

## Verified developer state

Base: `b9353b7`. Added 3 supplemental blocks / 72 words. Totals: **36 supplements / 525 words**, **111,079 runtime blocks / 545,636 verified words**. All 599 original pages / 603 files and all 33 preceding supplemental records are preserved.

Final headless stop: **MissingBlock `0x00150EE8`**. Relative to the saved same-mode baseline: **+487 executed core-0 instructions / +1,817 diagnostic ns**. All three fresh runs ended normally at missing code with native exit 3, no timeout and empty stderr. Complete captured audio, state and save contents match the baseline.

Enabled tests passed once per build: **83/83, 83/83, 84/84**, with zero failures/errors/skips. The final count includes the new immediate-AND suite and excludes the optional desktop smoke test. A passing diagnostic gate is not a successful game boot.

## Next exact task

Append only **`[0x00150EE8, 0x00150EF4)`**, one block / three words. Two pointer adjustments are followed by a stack-loaded PC return. Existing fast and memory handlers cover this range. The future stack value and return target are unobserved; do not infer them from live LR or add a successor body.

Authenticate the original inputs, preserve all 36 supplements, generate and verify exact words, build/test, preserve the exact native, and make one finite fresh run. No runtime decoding, forced returns, fabricated GPU completion or automatic retries.

## Continuation rules

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Publish source and metadata to `main` without force-pushing. Keep game inputs, generated game code, executables, captures, audio, saves and private recovery identifiers out of public Git. Keep user summaries short and plain.

Details: [current recovery record](docs/recovery/CURRENT-REBUILD.md).
