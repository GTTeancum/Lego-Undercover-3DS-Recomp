# Latest LEGO Undercover 3DS checkpoint

Resume from `main`. This batch starts from `7f3f4c7dc0d326e2c2a009d889228330ef8ca239`.

## Objective and current state

Goal: a full playable native recompilation of **LEGO City Undercover: The Chase Begins**.

Eight more observed startup sections now execute. Additional row-processing loops, their register and stack returns, and a caller entry resume original control flow. Startup advances another **147,594 executed instructions** to a missing stack-save/call entry.

Menus, gameplay, speaker playback and Windows execution remain unverified. No new screen capture or windowed run was performed. Earlier black captures remain historical private diagnostics.

## Changes and actual checks

The runtime, generator, tests and build configuration are unchanged. All 166 preceding reviewed sections and all 599 original AOT pages / 603 files are preserved. Generated operations authenticate against the exact supported original input. The public implementation change is eight appended observed ranges in `config/observed_aot_supplements.json`.

Totals: **174 reviewed sections / 175 page-local blocks / 3,174 supplemental words**; **111,218 runtime blocks / 548,285 verified words**.

Eight accepted headless Release candidates each passed **90/90 enabled tests**, with zero failures, errors or skips in their complete JUnit reports. Explicit build and CTest receipts record exit 0 for every candidate. All eight accepted startup runs ended at missing original code with native exit 3, no timeout and empty stderr. Capture lengths and record counts were checked; state and saves remain byte-for-byte identical to the accepted round-20 baseline.

The final capture contains **285,866 records / 4,573,888 bytes**. The entire baseline of 285,850 records / 4,573,632 bytes is an exact prefix, followed by 16 valid records whose two samples are zero. This is diagnostic capture evidence and does not establish speaker playback or audible content.

Four independent partial-state predictions matched: round 21, 70 instructions and 15 predicted or preserved reported fields; round 23, 353 instructions and 10 fields; round 26, 21 instructions and 16 fields; round 27, 36 instructions and 16 fields. Guest-memory-loaded values and output bytes outside those predictions remain unverified by these checks. Return and caller reviews cover immediate boundaries only. Total advance from the published baseline is **147,594 instructions / 550,494 diagnostic ns**; these are not gameplay percentages or host-performance measurements.

One round-27 launch attempt was rejected before a game process started: the frozen executable had lost execute permission, although its bytes matched the verified native and archived executable. The rejected bytes and traceback were preserved. Restoring execute permission and using fresh storage produced the accepted run; the cause remains unknown. There were no failed build or test gates in this batch. The final round-28 native already had the required mode and succeeded on its first run.

Rounds 23 and 24 retain CTest text logs ending after test 88 without final summaries. Both have complete, hash-bound 90-case passing JUnit reports and explicit CTest exit 0 with no timeout. Preserve the short logs as a text-record limitation, separately from the launch rejection. The other six text logs are complete. Some retained review, coverage and process text logs are empty; the bound JSON results and successful orchestration receipts remain available.

A separate backup-integrity check found the original round-28 native archive shorter than its recorded size and truncated at gzip EOF. The tested native and successful run remain intact; the failed archive and its original receipt are preserved. The cause is unknown. A replacement was saved after full gzip EOF/CRC and member name, size, mode and hash verification. Its compressed hash and complete decompression were checked again before and after saving; the exact tested native is unchanged. This backup failure is separate from the accepted build, test and native-run results.

The resident checkout, inputs, original AOT and preceding native were authenticated. No source/input restoration or reconfiguration was repeated; the existing configure record was copied for provenance. Hosted checks are pending at source-preparation time; actual results are recorded separately against the published commit.

## Remaining blocker and next action

Actual stop: **MissingBlock `0x0014F608`**, thread 7, dispatch round 21846. The next reviewed interval is **`[0x0014F608, 0x0014F610)`**, two original words, and **has not been added or executed**. It saves the incoming link register at stack pointer minus four, with guarded preindexed writeback, then calls the already reviewed nine-word entry at `0x0014FF08`. The call assigns link register `0x0014F610`; that return address is currently missing and remains unobserved and excluded.

Preserve all 174 reviewed sections. Reauthenticate the exact words and current generator, admit only the reviewed interval, generate and verify, build/test, preserve the exact native and check its execute permission before a finite run in fresh owned storage. Keep the incoming saved link distinct from the call's newly assigned link. A failed stack store must leave the stack pointer and link register unchanged and prevent the call. The review predicts only this immediate boundary, not the later callee or final run.

## Resume and permissions

Repository: `GTTeancum/Lego-Undercover-3DS-Recomp`, branch `main`. The private checkpoint records the published commit, input recovery references, exact executable, commands, actual results and next review. It preserves this rejected launch, incomplete text logs and previous checkpoint dependencies without relabeling historical failures as current attempts.

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Source/metadata publication and GitHub Actions compilation are authorized for this project. Never force-push. Keep original inputs, generated game code, executables, captures, audio, saves and private recovery identifiers out of public Git. Keep summaries short and omit black screenshots.

Details and reproduction settings: [current recovery record](docs/recovery/CURRENT-REBUILD.md).
