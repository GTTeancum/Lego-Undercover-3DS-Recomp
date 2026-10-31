# Latest LEGO Undercover 3DS checkpoint

Updated October 31, 2026 (Eastern). Read this first and resume from `main`.

## Plain-English status

Seven more startup gaps are filled: two calls, their setup and dispatch, a complete 16-row processing helper, its return and the next caller dispatch. Fresh execution now reaches the next missing helper. Each of the 7 fresh game runs passed its preceding stopping point. Menus and gameplay remain unverified.

The final post-stop captures remain completely black and are retained only as private diagnostics. No new screenshot is posted and no fresh windowed run was taken. No visual or gameplay milestone is claimed; speaker playback and Windows execution remain unverified.

## Verified developer state

Base: `0755bde`. Added 7 reviewed sections / 7 physical blocks / 102 words. Totals: **81 reviewed sections / 82 physical blocks / 2021 words**, **111,125 runtime blocks / 547,132 verified words**. All 599 original AOT pages / 603 files and all 74 prior sections are preserved. Runtime, generator, tests and build configuration are unchanged. Only observed-range metadata is appended while preserving all previous records.

Final stop: **MissingBlock `0x00155008`**. Same-mode advance: **+388 core-0 instructions / +1,447 diagnostic ns**. All runs ended at missing code with native exit 3, no timeout and empty stderr. Complete audio, state and save contents are byte-for-byte identical to the preceding same-mode checkpoint. This does not establish speaker playback.

All seven builds passed 89/89 enabled tests once, with zero failures/errors/skips. Each exact tested executable was saved privately before its fresh game run. These checks do not establish a successful game boot.

## Next exact task

Next reviewed interval: **`[0x155008, 0x155040)`**, 14 instruction words. Add the reviewed 14-instruction helper section that loads a mask, combines neighboring guest word and halfword values into one output row word, advances pointers, decrements the row count and conditionally loops to an interior suffix at 0x00155010. The actual future memory values, loop completion and fallthrough at 0x00155040 remain unobserved. Do not include that successor body or invent a return target. Existing handlers accept this reviewed interval.

Preserve all 81 reviewed sections (82 physical blocks). Authenticate, generate and verify exact words, build/test, preserve the exact native, and use fresh owned storage for a finite run. No runtime decoding, forced returns, fabricated GPU completion, invented targets or unobserved successor bodies.

## Continuation rules

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Source/metadata publication to `main` and ongoing private builds/backups are authorized. Never force-push. Keep original inputs, generated game code, executables, audio, captures, saves and recovery identifiers private. Keep user summaries short and plain. Do not post completely black screen captures to the user; retain them only as private diagnostic evidence.

Details: [current recovery record](docs/recovery/CURRENT-REBUILD.md).
