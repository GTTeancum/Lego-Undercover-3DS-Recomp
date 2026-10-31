# Latest LEGO Undercover 3DS checkpoint

Updated October 31, 2026 (Eastern). Read this first and resume from `main`.

## Plain-English status

Five more startup gaps are filled: a copying loop, its return, a missing call, a stack return and the next call setup. The program can also save both current screen images after execution stops, without SDL. Each of the 5 fresh game runs passed its preceding stopping point. Menus and gameplay remain unverified.

A fresh headless post-stop capture shows both screens completely black: 400x240 top and 320x240 bottom. This is the current stopped scanout, not a windowed screenshot or the earlier first-nonblack loading capture. Those different capture points are not a regression comparison. No fresh windowed run was possible because the pinned SDL3 download failed. Menus, gameplay, speaker playback and Windows execution remain unverified.

## Verified developer state

Base: `abf6cc6`. Added 5 reviewed sections / 5 physical blocks / 13 words. Totals: **74 reviewed sections / 75 physical blocks / 1919 words**, **111,118 runtime blocks / 547,030 verified words**. All 599 original AOT pages / 603 files and all 69 prior sections are preserved. The host gains opt-in --scanout-snapshot output, a bounded raw-image writer and authored tests with CMake wiring. Runtime evaluators, scheduler, renderer, scanout decoder and build-time generator are unchanged. The capture is performed only after execution stops; it issues no guest event, input or render command.

Final stop: **MissingBlock `0x0014F444`**. Same-mode advance: **+1,244 core-0 instructions / +4,640 diagnostic ns**. All runs ended at missing code with native exit 3, no timeout and empty stderr. Complete state and save contents match the preceding same-mode checkpoint. The entire prior audio capture is preserved byte-for-byte, with four valid silent diagnostic records appended; this is not evidence of speaker playback.

The first four builds each passed 88/88 enabled tests once; the final build passed 89/89 once, including the new snapshot suite, with zero failures/errors/skips. Each exact tested executable was saved privately before its fresh game run. These checks do not establish a successful game boot.

A deliberate second fresh run of the exact final native with capture disabled produced identical stop, counters, normalized original diagnostics, complete audio, state and save contents. Both capture-on and capture-off runs ended at MissingBlock with native exit 3, no timeout and empty stderr. This is a paired diagnostic comparison, not an automatic retry.

The snapshot and existing display-decoder suites passed a focused Clang address/undefined-behavior sanitizer run. Six command-line validation cases passed. Snapshot tests check exact RGB bytes, immutable input, black frames, invalid image dimensions and alpha, exclusive paths, and retained write/flush/close failures.

## Next exact task

Next reviewed interval: **`[0x14f444, 0x14f44c)`**, 2 instruction words. Add the reviewed two-instruction entry that saves six registers on the guest stack and calls the already recorded routine at 0x0015117C. Its later continuation at 0x0014F44C remains unobserved; do not include that successor or invent a return target. Existing handlers accept this reviewed interval.

Preserve all 74 reviewed sections (75 physical blocks). Authenticate, generate and verify exact words, build/test, preserve the exact native, and use fresh owned storage for a finite run. No runtime decoding, forced returns, fabricated GPU completion, invented targets or unobserved successor bodies.

## Continuation rules

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Source/metadata publication to `main` and ongoing private builds/backups are authorized. Never force-push. Keep original inputs, generated game code, executables, audio, captures, saves and recovery identifiers private. Keep user summaries short and plain.

Details: [current recovery record](docs/recovery/CURRENT-REBUILD.md).
