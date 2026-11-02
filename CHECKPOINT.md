# Latest LEGO Undercover 3DS checkpoint

Resume from `main`. This batch starts from `9cd3f9ca88ea4820abf66b6f6f8276c4ba2d4432`.

## Objective and current state

Goal: a full playable native recompilation of **LEGO City Undercover: The Chase Begins**.

Eight more observed startup sections now execute. They restore a pair of calls and their saved-stack return, two row-processing loops, and their register and stack returns. Startup advances another **14,539 executed instructions** to a missing halfword-copy loop.

Menus, gameplay, speaker playback and Windows execution remain unverified. No new screen capture or windowed run was performed. Earlier black captures remain historical private diagnostics.

## Changes and actual checks

The public implementation change is eight appended observed ranges in `config/observed_aot_supplements.json`, totaling 92 original words. All 174 preceding reviewed sections and all 599 original AOT pages / 603 files are preserved. Generated operations authenticate against the exact supported original input. The runtime, generator, tests and build configuration are unchanged.

Totals: **182 reviewed sections / 183 page-local blocks / 3,266 supplemental words**; **111,226 runtime blocks / 548,377 verified words**.

Eight headless Release candidates each passed **90/90 enabled tests**, with zero failures, errors or skips in complete JUnit reports. Explicit build and CTest receipts record exit 0 for every candidate. All eight accepted startup runs ended at missing original code with native exit 3, no timeout and empty stderr. Each native was saved privately before launch, and each completed run preserved its verified native archive. Original inputs and original AOT files remained unchanged.

The final capture contains **285,870 records / 4,573,952 bytes**. The entire round-28 baseline of 285,866 records / 4,573,888 bytes is an exact prefix, followed by four valid records whose two samples are zero. Rounds 29–35 have identical baseline captures; the four added records appear only after round 36. Persisted state and save files remain byte-for-byte identical to the baseline. These are diagnostic captures, without a speaker-playback or audible-content claim.

Three independent partial-state predictions matched: round 32, 14 instructions and 16 predicted or preserved reported fields; round 33, 56 instructions and 16 fields; round 35, 370 instructions and 10 fields. Guest-memory-loaded values and output bytes outside those predictions remain unverified by these checks. Call and return reviews cover immediate boundaries only. Total advance is **14,539 instructions / 54,228 diagnostic ns**; these are not gameplay percentages or host-performance measurements.

Five post-archive source checks failed because the local CTest text log shortened after a complete copy had been archived: rounds 30, 32, 34, 35 and 36. The build and CTest processes had already succeeded. Each complete archive passed gzip EOF/CRC and all 12 member checks; only the source text log differed. The shortened logs, original complete archives, tracebacks and failed helper exits were preserved. Restoring the exact archived log bytes reconciled every source member without changing the native or rerunning tests. The cause remains unknown. All eight current CTest logs now contain complete passing summaries; the rejected shorter versions remain in attempt history.

Round 36's next-review text log is empty; its authenticated review and coverage JSON are complete. The preceding checkpoint retains older failures and incomplete logs as historical evidence. No native launch failed in this batch.

The resident checkout, six original inputs, original AOT and preceding native were authenticated. No restoration, new configuration or unchanged baseline game run was repeated; the existing configure record was copied for provenance. Hosted checks are pending at source-preparation time; actual results are recorded separately against the published commit.

## Remaining blocker and next action

Actual stop: **MissingBlock `0x001550E8`**, thread 7, dispatch round 21846. The next reviewed interval is **`[0x001550E8, 0x00155104)`**, seven original words, and **has not been added or executed**. It reads two ordered halfwords per row, packs a word, stores it, advances source and destination by the original stride, decrements the count and branches back while nonzero.

For the observed count four, the unexecuted partial prediction is **28 instructions** to MissingBlock `0x00155104`, with 17 predicted or preserved reported fields and two unknown loaded registers. This forecast depends on future admission and successful guarded accesses. Preserve both halfword reads and their separate post-index effects: if the second read fails, the first read's pointer update remains effective.

Preserve all 182 reviewed sections. Reauthenticate the exact words and generator, admit only the observed interval, generate and verify, build/test, save and validate the exact native archive, then perform one finite run in fresh owned storage and compare the actual result with the saved prediction.

## Resume and permissions

Repository: `GTTeancum/Lego-Undercover-3DS-Recomp`, branch `main`. The private checkpoint records the published commit, input recovery references, exact executable, commands, actual results and next review. It preserves all five rejected source-log checks and their reconciliations, plus the preceding checkpoint dependencies.

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Source/metadata publication and GitHub Actions compilation are authorized for this project. Never force-push. Keep original inputs, generated game code, executables, captures, audio, saves and private recovery identifiers out of public Git. Keep summaries short and omit black screenshots.

Details and reproduction settings: [current recovery record](docs/recovery/CURRENT-REBUILD.md).
