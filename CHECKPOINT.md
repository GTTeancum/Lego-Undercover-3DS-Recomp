# Latest LEGO Undercover 3DS checkpoint

Updated October 31, 2026 (Eastern). Read this first and resume from `main`.

## Plain-English status

Five new startup gaps are filled, and the five tested fixes left unpublished last time are recovered. A sixteen-row copying loop and an eight-row processing helper now finish and return. The rebuild tool also safely joins a missing prefix to unchanged original code, without duplicating instructions or adding a jump. All five fresh runs passed their preceding stopping points. Startup reaches another missing helper; menus and gameplay remain unverified.

The five tested fixes left unpublished in the previous session are included, not discarded or counted as new runs. The final post-stop screen captures remain completely black and are retained privately. No fresh windowed run was performed and no visual milestone is claimed. Menus, gameplay, speaker playback and Windows execution remain unverified.

## Verified developer state

Base: `f1262c0dc24ea32d6e93df048a4bc6c0fcf87aa0`. Recovered 5 sections and added 5 newly observed sections. Totals: **117 reviewed sections / 118 page-local blocks / 2525 supplemental words**; **111,161 runtime blocks / 547,636 verified words**.

Recovered the previously unpublished five-section update, verified its complete archive and checked that its patch reverses to the exact published f1262c0 source tree. Its original test/run results are retained separately from the new work. All 112 recovered sections and all 599 original AOT pages / 603 files are preserved. Runtime evaluators and the original AOT pages are unchanged. The generator now supports explicitly reviewed fallthrough into an exact unchanged original block: the page is pinned by hash, its complete layout and original words are authenticated, and overlaps are rejected. CMake supplies the original AOT directory and tracks its page dependencies. Fourteen authored policy cases and an expanded synthetic runtime fixture cover joins, loops, missing targets and memory faults. Three focused Clang address/undefined-behavior sanitizer suites passed. Four status documents are updated after execution.

Final stop: **MissingBlock `0x0015457c`**. This session advances **19,733 executed instructions** beyond the recovered pending candidate. Including the five recovered fixes, advance from public base `f1262c0` is **27,683 executed instructions**. These are not gameplay or performance percentages. Complete state and save contents match the preceding same-mode checkpoint byte-for-byte. The entire prior audio capture is preserved as an exact prefix, followed by four valid silent diagnostic records; speaker playback is not established.

All 5 new builds passed **89/89 enabled tests once**, with zero failures, errors or skips. Each exact tested native was verified and uploaded to private recovery storage before its fresh finite run. No run timed out or wrote stderr. The recovered five builds retain their original 89/89 test reports and run evidence; those earlier game runs were not repeated. Passing these tests does not establish a successful game boot.

## Next exact task

Next reviewed interval: **`[0x15457c, 0x1546b0)`**, 77 original instruction words. Add the reviewed 77-instruction section. It saves three registers, adjusts the source pointer once, loads a mask and combines neighboring guest rows into four-word outputs, advancing pointers and decrementing the count by two. The first control exit conditionally repeats at interior suffix 0x001545D8, excluding its one-time setup. Existing handlers accept the interval. Future memory values, loop completion and fallthrough at 0x001546B0 remain unobserved; do not add a successor body or invent a return target.

Preserve all 117 reviewed sections (118 physical blocks). Authenticate inputs, generate and verify exact words, build/test, preserve the exact native, then use fresh owned storage for one finite run. No runtime decoding, forced returns, fabricated GPU completion, invented targets or unobserved successor bodies.

## Continuation rules

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Source/metadata publication to main and ongoing private builds/backups are authorized; never force-push. Original inputs, generated game code, executables, captures, audio, saves and recovery identifiers stay private. Keep updates short and plain. Do not post completely black screenshots.

Details: [current recovery record](docs/recovery/CURRENT-REBUILD.md).
