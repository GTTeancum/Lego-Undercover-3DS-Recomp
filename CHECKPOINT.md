# Latest LEGO Undercover 3DS checkpoint

Resume from `main`. This batch starts from `7ddc51e0c03b57b29140bf0ef2f99ee72bb91d99`.

## Objective and current state

Goal: a full playable native recompilation of **LEGO City Undercover: The Chase Begins**.

Six more missing startup sections now execute. An eight-row copy loop, an alternating row-processing loop and their returns complete through the original control flow. A one-instruction mask-load prefix rejoins unchanged original AOT coverage and advances startup by another 24,040 instructions before the next missing row-processing entry.

Menus, gameplay, speaker playback and Windows execution remain unverified. No new screen capture or windowed run was performed. Earlier black captures remain historical private diagnostics.

## Changes and actual checks

The runtime, generator, tests and build configuration are unchanged. All 160 preceding reviewed sections and all 599 original AOT pages / 603 files are preserved. Generated operations authenticate against the exact supported original input. The public implementation change is six appended observed ranges in `config/observed_aot_supplements.json`, including an authenticated join to an unchanged original block.

Totals: **166 reviewed sections / 167 page-local blocks / 3,054 supplemental words**; **111,210 runtime blocks / 548,165 verified words**.

Six accepted headless Release candidates each passed **90/90 enabled tests**, with zero failures, errors or skips in their accepted gate. All six fresh startup runs ended at missing original code with native exit 3, no timeout and empty stderr. Each capture was checked against its actual byte length and record count. State and saves remain byte-for-byte identical to the baseline.

The first five captures exactly match the baseline: 285,846 records / 4,573,568 bytes. The final capture contains 285,850 records / 4,573,632 bytes: the entire prior capture is an exact prefix, followed by four valid records whose two samples are zero. This is complete diagnostic capture evidence; it does not establish speaker playback or audible content.

Independent pre-run partial-state predictions matched the eight-row loop and both observed parts of the alternating loop. Return and original-code-join reviews state their limited boundary scope; they do not predict final state after execution continues through guest-memory-dependent callers. Total advance from the published baseline: **24,561 executed instructions / 91,607 diagnostic ns**. These are not gameplay percentages or host-performance measurements.

The first candidate required recovery from two rejected verification attempts: three test executables could not start, then a clean rebuild left one test executable without execute permission. Relinking that executable and rerunning the complete gate produced 90/90 passing tests. The cause was not established; both failed attempts are retained privately. No game run was made from either rejected attempt. All six accepted native runs produced complete evidence on their first execution.

## Remaining blocker and next action

Actual stop: **MissingBlock `0x00154B44`**. The next reviewed interval is **`[0x00154B44, 0x00154B90)`**, 19 original words, and **has not been added**. It loads the mask and advances the source by two once, then reads a preceding halfword and an aligned multi-register group, packs and combines the values, stores two words, advances both row pointers and repeats the suffix at `0x00154B4C` while the count is nonzero. Preserve the one-time setup, all guarded reads, staged arithmetic, partial memory effects, pointer-update order and flags. The loop-exit successor at `0x00154B90` remains unobserved and excluded.

Preserve all 166 reviewed sections. Reauthenticate the private inputs, review the complete first-exit interval, generate and verify exact words, build/test, preserve the exact native and use fresh owned storage for a finite run. Verify actual audio-file length against the reported record count. Do not add unobserved successors, infer a stack return from live LR, decode at runtime or fabricate GPU completion.

## Resume and permissions

Repository: `GTTeancum/Lego-Undercover-3DS-Recomp`, branch `main`. The private recovery checkpoint records the published commit, original-input recovery references, exact executable, run commands, actual results and next review. It preserves the new failed verification attempts and references earlier checkpoints for historical evidence.

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Source/metadata publication and GitHub Actions compilation are authorized for this project. Never force-push. Keep original inputs, generated game code, executables, captures, audio, saves and private recovery identifiers out of public Git. Keep summaries short and omit black screenshots.

Details and reproduction settings: [current recovery record](docs/recovery/CURRENT-REBUILD.md).
