# Latest LEGO Undercover 3DS checkpoint

Updated October 30, 2026 (Eastern). Read this first and resume from `main`.

## Plain-English status

Added the missing arithmetic support and fixed a code-table lookup crash exposed by tests. Five more startup sections now run, including both processing loops and their returns. All 5 fresh game runs passed their previous stopping points. Menus and gameplay remain unverified.

These were headless game runs. No new screenshot or visual progress is claimed; the earlier loading image remains the last verified windowed result. Speaker playback and Windows execution remain unverified.

## Verified developer state

Base: `28b8c7d`. Added 5 supplemental blocks / 128 instruction words. Totals: **59 supplements / 1072 words**, **111,102 runtime blocks / 546,183 verified words**. All 599 original AOT pages / 603 files and 54 preceding supplemental records remain unchanged. The generator now admits canonical immediate RSB with no PC operands. Its arithmetic still uses the existing evaluator. The runtime lookup cache now stores validated indices rather than borrowed block pointers, checking current entries and sorted insertion gaps on each hit. Two new native suites cover arithmetic and reused-registry lifetimes; Python policy tests cover the new admission. The same-page 1024-byte limit and all original-input, extent, overlap and first-exit guards remain unchanged.

Final headless stop: **MissingBlock `0x00151B6C`**. The run executed **61,411,758 core-0 instructions**, an advance of **23,764** over the same-mode baseline. Native exit 3, no timeout, empty stderr. The complete prior audio capture is preserved byte-for-byte as an exact prefix; the final capture adds 4 well-formed diagnostic records. Complete state and save contents match the prior same-mode checkpoint. This is a longer diagnostic capture, not evidence of speaker playback.

The full enabled-test gates passed once per build: **84/84 (round 01), 86/86 (round 02), 86/86 (round 03), 86/86 (round 04), 86/86 (round 05)**, with no failures/errors/skips. Each exact tested executable was saved privately before its fresh run. Passing these checks does not establish playable gameplay.

## Next exact task

Next interval: **`[0x00151B6C, 0x00151D10)`**. Add the next 105-instruction helper, which combines neighboring byte values into row words, restores its working pointer and returns through a stack-loaded address. The future memory values and return target remain unobserved. Existing handlers accept this reviewed interval. Future successor execution remains unobserved. Do not invent return targets or add unobserved successor bodies.

Preserve all 59 supplements. Authenticate original inputs, generate and verify exact instructions, build/test, preserve the exact native, then make one finite fresh run. No runtime decoding, forced returns, fabricated GPU completion or automatic retries.

## Continuation rules

Use ordinary tools in this chat's cloud workspace, not Codex services, Dot or the user's computer. Publish ordinary source and metadata to `main` without force-pushing. Original inputs, generated game code, executables, captures, audio, saves and private recovery identifiers stay private. Ongoing private LEGO builds and recovery backups are authorized. Keep user summaries short and plain.

Details: [current recovery record](docs/recovery/CURRENT-REBUILD.md).
