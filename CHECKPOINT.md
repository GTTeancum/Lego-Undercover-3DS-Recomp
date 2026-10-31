# Latest LEGO Undercover 3DS checkpoint

Updated October 31, 2026 (Eastern). Read this first and resume from `main`.

## Plain-English status

Five more startup gaps are filled. A 16-row processing pass and an eight-row helper now finish, and their missing return steps are restored. All five fresh runs passed their preceding stops. Startup reaches another missing call setup; menus and gameplay remain unverified.

The final post-stop captures remain completely black and are retained only as private diagnostics. No screenshot is posted and no fresh windowed run was taken. There is no new visual or gameplay milestone. Speaker playback and Windows execution remain unverified.

## Verified developer state

Base: `9fa13be`. Added 5 reviewed sections / 5 physical blocks / 70 words. Totals: **91 reviewed sections / 92 physical blocks / 2117 words**, **111,135 runtime blocks / 547,228 verified words**. All 599 original AOT pages / 603 files and all 86 prior sections are preserved. Runtime, generator, tests and build configuration are unchanged. Only observed-range metadata is appended.

Final stop: **MissingBlock `0x0014FB20`**. Same-mode advance: **+3,113 core-0 instructions / +11,611 diagnostic ns**. All five game runs ended at missing code with native exit 3, no timeout and empty stderr. Complete audio, state and save contents are byte-for-byte identical to the preceding same-mode checkpoint.

All five builds passed **89/89 enabled tests once**, with zero failures/errors/skips. Each exact tested executable was saved privately before its fresh game run. Measured pointer/count checks confirm the 16-row and eight-row passes; the second loop excludes its one-time prefix. These checks do not verify pixel values or establish a successful game boot.

## Next exact task

Next reviewed interval: **`[0x0014FB20, 0x0014FB30)`**, 4 instruction words. Add the reviewed four-instruction entry that saves LR on the guest stack, sets two working registers and calls the already recorded routine at 0x0014F190. Its stack write and future execution remain unobserved. The assigned return at 0x0014FB30 is still missing; do not include that continuation or invent a return target. Existing handlers accept this interval.

Preserve all 91 reviewed sections (92 physical blocks). Authenticate, generate and verify exact words, build/test, preserve the exact native, and use fresh owned storage for a finite run. No runtime decoding, forced returns, fabricated GPU completion, invented targets or unobserved successor bodies.

## Continuation rules

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Source/metadata publication to `main` and ongoing private builds/backups are authorized. Never force-push. Keep original inputs, generated game code, executables, audio, captures, saves and recovery identifiers private. Keep user summaries short and plain. Do not post completely black screen captures to the user; retain them only as private diagnostic evidence.

Details: [current recovery record](docs/recovery/CURRENT-REBUILD.md).
