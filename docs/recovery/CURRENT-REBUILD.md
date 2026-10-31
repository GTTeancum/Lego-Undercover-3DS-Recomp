# October 31, 2026 - continuation after recovered publication

## Status

The eleven pending startup fixes and the tested 256-section limit were published first as `461a306`. The previously rejected synthetic test-file upload was accepted unchanged through the same action on the user's explicit retry. This continuation preserves that published source and adds 5 newly observed sections.

Five new startup gaps are filled. A sixteen-row neighboring-byte processing loop and an eight-row copying loop now complete, their missing return steps are restored, and a setup step connects to existing code. All five fresh runs passed their preceding stops. Startup reaches another missing helper; menus and gameplay remain unverified.

All 134 previous sections and all 599 original AOT pages / 603 files remain unchanged. Runtime, generator, tests and build configuration are unchanged. Only observed metadata and four post-run status documents change.

Totals: **139 reviewed sections / 140 physical blocks / 2795 supplemental words**, **111,183 runtime blocks / 547,906 verified words**.

## Actual new runs

| Round | Actual headless stop | Additional executed instructions | Tests |
| --- | --- | ---: | ---: |
| 01 | `0x00153f10` | +450 | 89/89 |
| 02 | `0x00153d58` | +9,979 | 89/89 |
| 03 | `0x00153d98` | +121 | 89/89 |
| 04 | `0x0014fb08` | +222 | 89/89 |
| 05 | `0x00153b3c` | +2,970 | 89/89 |

Round 01 compares with the retained final run of the newly published execution source. Subsequent rows compare with the preceding fresh run. Publication of the eleven recovered fixes did not count those older runs as new.

All 5 exact new natives passed their 89-suite test gate once and were saved privately before fresh finite game runs. Every run ended at missing code with native exit 3, no timeout and empty stderr. State and saves match the preceding checkpoint. Its full audio capture remains an exact prefix, with 4 additional diagnostic records.

Measured startup pointers, row counts and instruction counts confirm completion of all sixteen rows in the first loop and all eight rows in the copying loop, without replaying their one-time prefixes. Checked unrelated registers and status fields are preserved, and completion flags match the original arithmetic. The restored returns and setup allow execution through existing code to the later recorded stops; isolated single-step return equivalence was not captured. These startup checks do not validate actual output words or rendered pixels.

The new neighboring-byte helper passed a separate private controlled-data diagnostic: 256 cases covering 2,176 output rows matched an independent scalar reference, preserved input data and left destination guard bytes untouched. This verifies controlled arithmetic/memory behavior, not real-game pixels or gameplay.

Source, native, original input and AOT identities were checked around every fresh run. Prior supplemental operations remain unchanged. Four status documents were updated only after the last run; every other source byte matches the final tested candidate.

Cumulative advance from `461a306`: 13,742 executed core-0 instructions / 51,255 diagnostic ns. These are not a gameplay percentage or a performance measurement.

The final post-stop captures are black and remain private diagnostics. No fresh windowed run or new capture-on/off comparison was performed. Menus, gameplay, speaker playback and Windows execution remain unverified.

The initial recovered publication's GCC, Clang, sanitizer and packaging jobs were observed passing. New hosted checks for this continuation must be observed separately; no earlier result is substituted for a new result. Hosted checks do not execute the original game.

## Next exact task

**`[0x153b3c, 0x153bc4)`**. Add the reviewed 34-instruction section. It saves two working registers, loads a mask and prepares the first source row once, then combines neighboring guest rows into pairs of four-word output rows. It advances both pointers and decrements the row count by two. The first control exit conditionally repeats at interior suffix 0x00153B5C, excluding the one-time setup. Existing handlers accept the interval. Future memory values, loop completion and fallthrough at 0x00153BC4 remain unobserved; do not add that successor or invent a return target. The private review in rounds/05/next-review/review.json is bound to the actual final stop and has no overlaps with original or supplemental code.

## Working permissions

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Ordinary source/metadata publication and ongoing private backups are authorized. Never force-push. Keep original game inputs, generated code, executables, audio, screenshots, saves and private identifiers out of public Git. Keep user updates short and plain; omit black screenshots.
