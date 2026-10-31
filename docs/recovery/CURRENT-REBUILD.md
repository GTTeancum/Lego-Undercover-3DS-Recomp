# October 31, 2026 - recovered publication

## Status

Publication base: `e44b7c4`. This checkpoint includes all eleven previously pending startup fixes. The rejected synthetic test-file upload was retried at the user's request through the same GitHub action and accepted unchanged. Both its blob and the manifest match the saved candidate. The publication itself runs no original-game code.

Five more startup gaps are filled. The four-row neighboring-data helper and the sixteen-row processing loop now finish, and both missing return steps are restored. All five fresh startup runs passed their previous stopping points. Startup reaches another missing helper; menus and gameplay remain unverified.

Preserved: all six previously unpublished fixes, all 129 prior reviewed sections, and all 599 original AOT pages / 603 files. Runtime, generator, tests and build configuration are unchanged in this batch. The existing tested 256-section capacity is retained.

Totals: **134 reviewed sections / 135 page-local blocks / 2741 supplemental words**, **111,178 runtime blocks / 547,852 verified words**.

## Fresh measured runs

| Round | Actual headless stop | Additional executed instructions | Tests |
| --- | --- | ---: | ---: |
| 01 | `0x00154710` | +15 | 89/89 |
| 02 | `0x00154738` | +30 | 89/89 |
| 03 | `0x001541ec` | +3,631 | 89/89 |
| 04 | `0x00154260` | +434 | 89/89 |
| 05 | `0x00153e98` | +1,815 | 89/89 |

Round 01 compares with the recovered pending build, not the older public checkpoint. Subsequent rows compare with the immediately preceding fresh run. The earlier six runs remain recovered evidence and are not counted as new runs.

## Validation

All 5 fresh builds passed 89/89 enabled tests once, with zero failures, errors or skips. Each exact native was saved privately before its finite run. None timed out or wrote stderr. Source, original inputs, AOT pages, earlier supplemental operations and native identities remained unchanged during execution. Four status documents were updated only after the final run; every other candidate source byte matches the final tested build.

Complete captured audio, state and save contents match the recovered candidate byte-for-byte.

Measured startup registers and instruction counts confirm the four-row neighboring-data pass and the sixteen-row processing loop both complete without repeating their one-time setup. Their expected pointer advances, exhausted counts, completion flags and checked unrelated registers match. The restored returns let execution continue through existing code to later observed stops; isolated single-step return equivalence was not captured. Separately, two private diagnostics executed the exact restored operations with authored input data: all 384 cases passed, checking 2,752 output rows against independent scalar references, unchanged input regions and untouched destination guard bytes. Those controlled-data checks do not validate the real game's rendered pixels or establish gameplay.

New measured advance: 5,925 instructions beyond the recovered candidate and 11,615 beyond public base e44b7c4. This is not a gameplay percentage. Final diagnostic time is 8,751,924,190 ns; diagnostic counters are not performance measurements.

The final post-stop screen captures are black and remain private diagnostics. No fresh windowed run or new capture-on/off paired comparison was performed. Menus, gameplay, speaker playback and Windows execution remain unverified.

Fresh hosted results must be checked after publication; earlier passing hosted checks are not claimed as validation of these sections. Full local test/build logs, run outputs and exact natives remain private.

## Next exact task

**`[0x153e98, 0x153f10)`**. Add the reviewed 30-instruction section. It loads a mask, adjusts the source pointer once, reads four guest words and a following halfword, combines shifted and masked neighboring values into four output words, advances both row pointers and decrements the count. Its first control exit conditionally repeats at interior suffix 0x00153EA0, excluding the mask load and one-time adjustment. Existing handlers accept the interval. Future memory values, loop completion and fallthrough at 0x00153F10 remain unobserved; do not add that successor or invent a return target. The private review in rounds/05/next-review/review.json is bound to the actual final stop and has no overlaps with original or supplemental code.

## Working permissions

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Ordinary source/metadata publication and ongoing private backups are authorized. Never force-push. Keep original inputs, generated game code, executables, captures, audio, saves and private recovery identifiers out of public Git. Keep summaries short and plain; omit completely black captures.
