# October 31, 2026 - recovered and continued startup

## Status

Five new startup gaps are filled, and the five tested fixes left unpublished last time are recovered. A sixteen-row copying loop and an eight-row processing helper now finish and return. The rebuild tool also safely joins a missing prefix to unchanged original code, without duplicating instructions or adding a jump. All five fresh runs passed their preceding stopping points. Startup reaches another missing helper; menus and gameplay remain unverified.

Recovered the previously unpublished five-section update, verified its complete archive and checked that its patch reverses to the exact published f1262c0 source tree. Its original test/run results are retained separately from the new work. All 112 recovered sections and all 599 original AOT pages / 603 files are preserved. Runtime evaluators and the original AOT pages are unchanged. The generator now supports explicitly reviewed fallthrough into an exact unchanged original block: the page is pinned by hash, its complete layout and original words are authenticated, and overlaps are rejected. CMake supplies the original AOT directory and tracks its page dependencies. Fourteen authored policy cases and an expanded synthetic runtime fixture cover joins, loops, missing targets and memory faults. Three focused Clang address/undefined-behavior sanitizer suites passed. Four status documents are updated after execution.

Totals: **117 reviewed sections / 118 page-local blocks / 2525 supplemental words**; **111,161 runtime blocks / 547,636 verified words**.

## New measured runs

| New round | Actual headless stop | Additional executed instructions | Tests |
| --- | --- | ---: | ---: |
| 01 | `0x001540ec` | +241 | 89/89 |
| 02 | `0x00153f14` | +374 | 89/89 |
| 03 | `0x0015473c` | +615 | 89/89 |
| 04 | `0x00154778` | +113 | 89/89 |
| 05 | `0x0015457c` | +18,390 | 89/89 |

Round 01 compares with the recovered pending candidate. Later rounds compare with the preceding fresh run. The five recovered earlier runs remain separate evidence and are not counted again here.

## Validation and scope

All 5 new builds passed **89/89 enabled tests once**, with zero failures, errors or skips. Each exact tested native was verified and uploaded to private recovery storage before its fresh finite run. No run timed out or wrote stderr. The recovered five builds retain their original 89/89 test reports and run evidence; those earlier game runs were not repeated.

The final run executes 62,298,344 core-0 instructions and 99 core-1 instructions; diagnostic time is 8,751,870,679 ns. This session advances **19,733 executed instructions** beyond the recovered pending candidate. Including the five recovered fixes, advance from public base `f1262c0` is **27,683 executed instructions**. These are not gameplay or performance percentages.

Complete state and save contents match the preceding same-mode checkpoint byte-for-byte. The entire prior audio capture is preserved as an exact prefix, followed by four valid silent diagnostic records; speaker playback is not established. The measured first-loop check confirms all 16 rows, both pointer advances, exhausted count, final flags and one-time setup. The next eight-row helper has measured pointer/count and one-time-setup checks. The join reached its next actual missing helper after 615 instructions, using unchanged original blocks. No isolated register capture was taken at the join helper’s own return; an incorrect private analysis expectation for that boundary was corrected without repeating the game run. The earlier generation produced byte-identical C++ before the new join was added; the wide overlapping candidate was rejected before generation and no original code was duplicated. These checks do not verify output words or pixel values. Source identities, original inputs, AOT pages, prior supplemental operations and native identities are checked before/after the runs. Four status documents are updated after the final run; all other published source bytes match the exact final tested candidate.

The final post-stop screen captures remain completely black and are retained privately. No fresh windowed run was performed and no visual milestone is claimed. Menus, gameplay, speaker playback and Windows execution remain unverified. The existing post-stop capture option is used on the final run; no new capture-on/off paired run is claimed. Hosted synthetic checks do not run the original game; current results are recorded separately from local native test and game-run evidence.

## Next exact task

Next interval: **`[0x15457c, 0x1546b0)`**, 77 original instruction words. Add the reviewed 77-instruction section. It saves three registers, adjusts the source pointer once, loads a mask and combines neighboring guest rows into four-word outputs, advancing pointers and decrementing the count by two. The first control exit conditionally repeats at interior suffix 0x001545D8, excluding its one-time setup. Existing handlers accept the interval. Future memory values, loop completion and fallthrough at 0x001546B0 remain unobserved; do not add a successor body or invent a return target.

## Working permissions

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Source/metadata publication to main and ongoing private builds/backups are authorized; never force-push. Original inputs, generated game code, executables, captures, audio, saves and recovery identifiers stay private. Keep updates short and plain. Do not post completely black screenshots.
