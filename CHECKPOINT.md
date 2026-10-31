# Latest LEGO Undercover 3DS checkpoint

Updated October 30, 2026 (Eastern). Read this first and resume from `main`.

## Plain-English status

Five more missing startup sections now run: two byte-processing helpers, an averaging helper, a row-copying loop and its return step. Together they let startup move through a substantially longer stretch of its existing code. Each of the 5 fresh game runs passed its preceding stopping point. Menus and gameplay remain unverified.

These were headless runs. No fresh game-window run or screenshot was taken. The earlier loading image remains the last verified visible result. Speaker playback and Windows execution remain unverified.

## Verified developer state

Base: `aa82c65`. Added 5 reviewed sections / 5 physical blocks / 301 words. Totals: **69 reviewed sections / 70 physical blocks / 1906 words**, **111,113 runtime blocks / 547,017 verified words**. All 599 original AOT pages / 603 files and all 64 prior sections are preserved. Runtime, generator, tests and build configuration are unchanged.

Final stop: **MissingBlock `0x001546B8`**. Same-mode advance: **+787,859 core-0 instructions / +2,938,546 diagnostic ns**. All runs ended at missing code with native exit 3, no timeout and empty stderr. Complete state and save contents match the preceding same-mode checkpoint. The entire prior audio capture is preserved byte-for-byte, with 96 valid silent diagnostic records appended; this is not evidence of speaker playback.

Every build passed **88/88 enabled tests once**, with zero failures/errors/skips. Each exact tested executable was saved privately before its fresh game run. These checks do not establish a successful game boot.

## Next exact task

Next reviewed interval: **`[0x1546b8, 0x1546d0)`**, 6 instruction words. Add the reviewed six-instruction loop that copies two guest words per row, advances both pointers, decrements its row count and conditionally repeats. Future memory values, loop completion and the fallthrough beyond this section remain unobserved; do not add the successor body. Existing handlers accept this reviewed interval.

Preserve all 69 reviewed sections (70 physical blocks). Authenticate, generate and verify exact words, build/test, preserve the exact native, and use fresh owned storage for a finite run. No runtime decoding, forced returns, fabricated GPU completion, invented targets or unobserved successor bodies.

## Continuation rules

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Source/metadata publication to `main` and ongoing private builds/backups are authorized. Never force-push. Keep original inputs, generated game code, executables, audio, captures, saves and recovery identifiers private. Keep user summaries short and plain.

Details: [current recovery record](docs/recovery/CURRENT-REBUILD.md).
