# Latest LEGO Undercover 3DS checkpoint

Updated October 30, 2026. Read this first and resume from `main`.

## Plain-English status

5 more missing startup sections are filled. Each fresh game run passed its previous stopping point. Startup now reaches another unfinished section. Menus and gameplay remain unverified.

These runs were headless. There is no new screenshot or visual-progress claim. The last verified windowed result remains the earlier loading image.

## Verified developer state

Base: `122580d`. Added 5 supplemental blocks / 112 words. Totals: **44 supplements / 661 words**, **111,087 runtime blocks / 545,772 verified words**. All 599 original pages / 603 files and all 39 preceding supplemental records are preserved. Runtime, generator, tests and build configuration are unchanged.

Final headless stop: **MissingBlock `0x001510BC`**. Relative to the saved same-mode baseline: **+817 executed core-0 instructions / +3,047 diagnostic ns**. All 5 fresh runs ended at missing code with native exit 3, no timeout and empty stderr. Complete captured audio, state and save contents match the baseline. Original inputs, code and exact native identities remained unchanged during every run.

All 5 builds passed **84/84 enabled tests once each**, with zero failures, errors or skips. Each exact tested executable was saved privately before its fresh game run. Passing these checks is not evidence of playable gameplay.

## Next exact task

Add only **`[0x001510BC, 0x001510D0)`**, one block / 5 words / 20 bytes. Five original instructions advance the working pointer, load and pop a guest-stack word, test its flag, establish LR `0x001510DC`, and conditionally branch to recorded `0x00151028`. Existing fast, memory and immediate-TST handlers cover the range. The future stack value and branch outcome remain unobserved. Preserve memory faults, stack writeback, flags and target validation; do not include the unobserved fallthrough `0x001510D0` or eventual return body.

Preserve all 44 supplements. Authenticate, generate and verify exact words, build/test, save the exact native, and make a finite fresh run. No runtime decoding, forced returns, fabricated GPU completion or automatic retries.

## Continuation rules

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Publish source and metadata to `main` without force-pushing. Keep game inputs, generated game code, executables, captures, audio, saves and private recovery identifiers out of public Git. Keep user summaries short and plain.

Details: [current recovery record](docs/recovery/CURRENT-REBUILD.md).
