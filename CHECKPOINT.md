# Latest LEGO Undercover 3DS checkpoint

Updated October 30, 2026. Read this first and resume from `main`.

## Plain-English status

The rebuild tool now accepts larger startup routines. The previously blocked routine and four more missing startup sections now run. Each of the 5 fresh game runs passed its preceding stopping point. Menus and gameplay remain unverified.

These were headless runs. SDL3 was unavailable in this workspace, so there is no new screenshot or visual-progress claim; the earlier loading image remains the last verified windowed result.

## Verified developer state

Base: `14b9f92`. Added 5 supplemental blocks / 245 words. Totals: **54 supplements / 944 words**, **111,097 runtime blocks / 546,055 verified words**. All 599 original pages / 603 files and all 49 preceding supplemental records are preserved. The generator now admits same-page blocks up to 1024 bytes; the previous limit was 512 bytes. Python policy tests cover the extended bounds and tail validation. Synthetic C++ tests exercise 96-, 160- and 256-word blocks, including interior-loop dispatch. Runtime evaluators and build configuration are unchanged.

Final headless stop: **MissingBlock `0x001515B8`**. Relative to the same-mode baseline: **+9,035 executed core-0 instructions / +33,698 diagnostic ns**. Every run ended at missing code with native exit 3, no timeout and empty stderr. Complete captured audio, state and save contents match the baseline.

Each new build passed **84/84 enabled tests once**, with zero failures/errors/skips. Its exact executable was saved privately before its fresh run. These checks do not establish a successful game boot.

## Next exact task

Next candidate: **`[0x001515B8, 0x00151604)`**, one block / 19 words / 76 bytes. The next helper copies a pair of guest words to eight rows and returns through LR. Its memory values and eventual return destination remain unobserved. Existing handlers accept the reviewed instructions. Successor execution remains unobserved; do not add unobserved bodies or manufacture a return target.

Preserve all 54 supplements. Authenticate, generate and verify exact words, build/test, preserve the exact native, and make a finite fresh run. No runtime decoding, forced returns, fabricated GPU completion or automatic retries.

## Continuation rules

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Publish source and metadata to `main` without force-pushing. Keep game inputs, generated game code, executables, captures, audio, saves and private recovery identifiers out of public Git. The user authorized ongoing private builds and recovery backups. Keep user summaries short and plain.

Details: [current recovery record](docs/recovery/CURRENT-REBUILD.md).
