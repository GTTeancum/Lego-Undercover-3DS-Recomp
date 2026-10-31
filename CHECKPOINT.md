# Latest LEGO Undercover 3DS checkpoint

Updated October 31, 2026 (Eastern). Read this first and resume from `main`.

## Plain-English status

Six more startup gaps are filled. Two eight-row processing passes now finish, and their missing call and return links are restored. Startup reaches the next unfinished helper. All 6 fresh runs passed their preceding stopping points. Menus and gameplay remain unverified. Speaker playback and Windows execution also remain unverified.

The final post-stop captures are completely black and remain private diagnostics. No fresh windowed run was taken and no visual milestone is claimed.

## Verified developer state

Base: `1f1df7b`. Added 6 reviewed sections / 6 physical blocks / 126 words. Totals: **97 reviewed sections / 98 physical blocks / 2243 words**, **111,141 runtime blocks / 547,354 verified words**. All 599 original pages / 603 files and all 91 prior sections are preserved. Runtime, generator, tests and build configuration are unchanged.

Final stop: **MissingBlock `0x00154844`**. Same-mode advance: **+713 core-0 instructions / +2,660 diagnostic ns**. Complete audio, state and save contents match the previous checkpoint byte-for-byte.

All 6 builds passed **89/89 enabled tests once**, with zero failures, errors or skips. Each exact tested executable was saved privately before its fresh finite run. Every run ended at missing code with native exit 3, no timeout and empty stderr. These checks do not establish playable gameplay.

Measured stop records confirm the restored processing loop completes all eight requested rows with expected pointer advances and exhausted count, without replaying its one-time setup. The second paired-row helper also completes eight rows with its one-time source adjustment and register save executed once. These checks do not verify the output words or pixel values.

## Next exact task

`[0x00154844, 0x00154874)`, 12 original instruction words. Add the reviewed 12-instruction row-copying section. It adjusts the source pointer once, combines two guest words and an adjacent byte into two output words, advances the row pointers, decrements the count and conditionally repeats at the interior suffix 0x00154848. Future memory values, loop completion and fallthrough at 0x00154874 remain unobserved; do not include that successor. Existing handlers accept the interval. Preserve all 97 reviewed sections (98 page-local blocks) and the original 599 AOT pages. Do not invent a return target or include unobserved successor bodies.

Authenticate, generate and verify exact words, build/test, preserve the exact native, and use fresh owned storage for a finite run. No runtime decoding, forced returns, fabricated GPU completion or invented successors.

## Continuation rules

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Source/metadata publication to `main` and ongoing private builds/backups are authorized. Never force-push. Keep original inputs, generated game code, executables, audio, captures, saves and recovery identifiers private. Keep summaries short and plain. Do not post completely black screen captures.

Details: [current recovery record](docs/recovery/CURRENT-REBUILD.md).
