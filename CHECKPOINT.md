# Latest LEGO Undercover 3DS checkpoint

Updated October 31, 2026 (Eastern). Read this first and resume from `main`.

## Plain-English status

Five more startup gaps are filled: an eight-row processing loop, its return, the next setup and table dispatch, and two stack returns. Startup now completes both processing passes and reaches another missing helper. Each of the 5 fresh game runs passed its preceding stopping point. Menus and gameplay remain unverified.

The final post-stop captures remain completely black and are retained only as private diagnostics. No new screenshot is posted and no fresh windowed run was taken. No visual or gameplay milestone is claimed; speaker playback and Windows execution remain unverified.

## Verified developer state

Base: `3a08db4`. Added 5 reviewed sections / 5 physical blocks / 26 words. Totals: **86 reviewed sections / 87 physical blocks / 2047 words**, **111,130 runtime blocks / 547,158 verified words**. All 599 original AOT pages / 603 files and all 81 prior sections are preserved. Runtime, generator, tests and build configuration are unchanged. Only observed-range metadata is appended while preserving all previous records.

Final stop: **MissingBlock `0x00154B94`**. Same-mode advance: **+284 core-0 instructions / +1,059 diagnostic ns**. All runs ended at missing code with native exit 3, no timeout and empty stderr. Complete audio, state and save contents are byte-for-byte identical to the preceding same-mode checkpoint. This does not establish speaker playback.

All five builds passed 89/89 enabled tests once, with zero failures/errors/skips. Each exact tested executable was saved privately before its fresh game run. These checks do not establish a successful game boot.

## Next exact task

Next reviewed interval: **`[0x154b94, 0x154c2c)`**, 38 instruction words. Add the reviewed 38-instruction entry section that loads a mask, combines neighboring guest halfword and word values from two rows, writes two output words, advances the row pointers and decrements the row count. Its first control exit is a conditional return through LR. Future memory values, that condition outcome, the return destination and fallthrough at 0x00154C2C remain unobserved; do not include an unobserved successor body. Existing handlers accept this reviewed interval.

Preserve all 86 reviewed sections (87 physical blocks). Authenticate, generate and verify exact words, build/test, preserve the exact native, and use fresh owned storage for a finite run. No runtime decoding, forced returns, fabricated GPU completion, invented targets or unobserved successor bodies.

## Continuation rules

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Source/metadata publication to `main` and ongoing private builds/backups are authorized. Never force-push. Keep original inputs, generated game code, executables, audio, captures, saves and recovery identifiers private. Keep user summaries short and plain. Do not post completely black screen captures to the user; retain them only as private diagnostic evidence.

Details: [current recovery record](docs/recovery/CURRENT-REBUILD.md).
