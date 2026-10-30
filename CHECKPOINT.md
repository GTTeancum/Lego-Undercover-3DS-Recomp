# Latest LEGO Undercover 3DS checkpoint

Updated October 30, 2026. Read this first and resume from `main`.

## Plain-English status

Three more missing startup sections are filled: a return step, a small byte-replication helper, and a dispatch step. Each fresh game run passed its previous stopping point. The game now reaches another unfinished helper. Menus and gameplay remain unverified.

These runs were headless. There is no new screenshot or visual-progress claim; the last verified windowed result remains the earlier loading image.

## Verified developer state

Base: `c656402`. Added 3 supplemental blocks / 24 words. Totals: **39 supplements / 549 words**, **111,082 runtime blocks / 545,660 verified words**. All 599 original pages / 603 files and all 36 preceding supplemental records are preserved. Runtime, generator, tests and build configuration are unchanged.

Final headless stop: **MissingBlock `0x00152DCC`**. Relative to the saved same-mode baseline: **+957 executed core-0 instructions / +3,569 diagnostic ns**. All three fresh runs ended at missing code with native exit 3, no timeout and empty stderr. Complete captured audio, state and save contents match the baseline.

Each of the three builds passed **84/84 enabled tests once**, with zero failures/errors/skips. Each exact tested executable was saved privately before its fresh run. Passing these checks is not evidence of playable gameplay.

## Next exact task

Append only **`[0x00152DCC, 0x00152EB4)`**, one block / 58 words / 232 bytes. Offline review of the authenticated input accepts all instructions with existing handlers. The section saves registers, combines guest byte values into several row words, restores its working pointer and returns through a stack-loaded PC. The future memory values and return target remain unobserved; do not infer the return from live LR or add a successor body.

Preserve all 39 supplements. Authenticate, generate and verify exact words, build/test, save the exact native, and make a finite fresh run. No runtime decoding, forced returns, fabricated GPU completion or automatic retries.

## Continuation rules

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Publish source and metadata to `main` without force-pushing. Keep game inputs, generated game code, executables, captures, audio, saves and private recovery identifiers out of public Git. Keep user summaries short and plain.

Details: [current recovery record](docs/recovery/CURRENT-REBUILD.md).
