# Latest LEGO Undercover 3DS checkpoint

Updated October 31, 2026 (Eastern). Read this first and resume from `main`.

## Plain-English status

Five more startup gaps are filled. A four-row copying loop and a 16-row neighboring-data pass now complete, and their missing return steps are restored. Startup reaches the next unfinished helper. All 5 fresh game runs passed their preceding stopping points. Menus and gameplay remain unverified. Speaker playback and Windows execution also remain unverified.

The final post-stop captures remain completely black and are retained privately. No fresh windowed run was performed and no visual milestone is claimed.

## Verified developer state

Base: `8f1d469`. Added 5 reviewed sections / 5 physical blocks / 52 words. Totals: **102 reviewed sections / 103 physical blocks / 2295 words**, **111,146 runtime blocks / 547,406 verified words**. All 599 original AOT pages / 603 files and all 97 preceding sections are preserved. Runtime, generator, tests and build configuration are unchanged. Only observed-range metadata is appended.

Final stop: **MissingBlock `0x00154c84`**. Same-mode advance: **+11,125 core-0 instructions / +41,494 diagnostic ns**. All runs ended at missing code with native exit 3, no timeout and empty stderr. Complete audio, state and save contents match the preceding same-mode checkpoint byte-for-byte. These are not gameplay or performance percentages.

Every build passed **89/89 enabled tests once**, with zero failures/errors/skips. Each exact tested executable was saved privately before its fresh run. Measured pointer/count and instruction-count checks confirm the first restored loop completed all four requested rows and the neighboring-row pass completed all sixteen output rows, without repeating their one-time prefixes. These checks do not validate output words or pixels.

## Next exact task

Next reviewed interval: **`[0x00154c84, 0x00154cb0)`**. Add the reviewed 11-instruction row-copying section. It adjusts the source pointer once, loads a preceding byte and two guest words, combines shifted byte lanes into two output words, advances both row pointers, decrements the count and conditionally repeats at interior suffix 0x00154C88. Existing handlers accept the interval. Future memory values, loop completion and fallthrough at 0x00154CB0 remain unobserved; do not include that successor or invent a return target.

Preserve all 102 reviewed sections (103 page-local blocks). Authenticate, generate and verify exact words, build/test, preserve the exact native, and use fresh owned storage for one finite run. No runtime decoding, forced returns, fabricated GPU completion, invented targets or unobserved successor bodies.

## Continuation rules

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Source/metadata publication to `main` and ongoing private builds/backups are authorized. Never force-push. Original inputs, generated game code, executables, audio, captures, saves and private recovery identifiers stay private. Keep summaries short and plain. Do not post completely black screenshots.

Details: [current recovery record](docs/recovery/CURRENT-REBUILD.md).
