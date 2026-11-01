# Latest LEGO Undercover 3DS checkpoint

Resume from `main`. This batch starts from `c0383c339eda6c391b21fa91d8a821d66f78983d`.

## Objective and current state

Goal: a full playable native recompilation of **LEGO City Undercover: The Chase Begins**.

Eight more missing startup sections now execute. A four-row copy loop, its nested stack returns, and a larger row-processing loop complete through the original control flow. Restoring the final return advances execution by 28,863 instructions before the next missing row-copy entry.

Menus, gameplay, speaker playback and Windows execution remain unverified. No new screen capture or windowed run was performed. Earlier black captures remain historical private diagnostics.

## Changes and actual checks

The runtime, generator, tests and build configuration are unchanged. All 152 preceding reviewed sections and all 599 original AOT pages / 603 files are preserved. Generated operations authenticate against the exact supported original input. The only public implementation change is eight appended observed ranges in `config/observed_aot_supplements.json`.

Totals: **160 reviewed sections / 161 page-local blocks / 2,974 supplemental words**; **111,204 runtime blocks / 548,085 verified words**.

Eight newly built headless Release candidates each passed **90/90 enabled tests**, with zero failures, errors or skips. All eight fresh startup runs ended at missing original code with native exit 3, no timeout and empty stderr. Each capture was checked against its actual byte length and record count. State and saves remain byte-for-byte identical to the baseline.

The first seven captures exactly match the baseline: 285,842 records / 4,573,504 bytes. The final capture contains 285,846 records / 4,573,568 bytes: the entire prior capture is an exact prefix, followed by four valid records whose two samples are zero. This is complete diagnostic capture evidence; it does not establish speaker playback or audible content.

Independent pre-run predictions matched the two restored loops and the intervening return paths, with unknown guest data explicitly excluded where appropriate. Total advance from the published baseline: **29,367 executed instructions / 109,533 diagnostic ns**. These are not gameplay percentages or host-performance measurements.

No new build, test or runtime attempt failed in this batch. Earlier failed environment checks and the rejected capture remain in the preceding private recovery checkpoint.

## Remaining blocker and next action

Actual stop: **MissingBlock `0x001543F0`**. The next reviewed interval is **`[0x001543F0, 0x0015442C)`**, 15 original words, and **has not been added**. It advances the source by one once, then copies rows using byte and aligned multi-register reads, packing shifts, stores, the original stride and a count-controlled suffix branch to `0x001543F4`. Preserve the one-time adjustment, all guarded reads including the trailing byte discarded from output, partial memory effects, pointer-update order and flags. The loop-exit successor at `0x0015442C` remains unobserved and excluded.

Preserve all 160 reviewed sections. Reauthenticate the private inputs, review the complete first-exit interval, generate and verify exact words, build/test, preserve the exact native and use fresh owned storage for a finite run. Verify actual audio-file length against the reported record count. Do not add unobserved successors, infer a stack return from live LR, decode at runtime or fabricate GPU completion.

## Resume and permissions

Repository: `GTTeancum/Lego-Undercover-3DS-Recomp`, branch `main`. The private recovery checkpoint records the published commit, original-input recovery references, exact executable, run commands, actual results and next review. Its links to earlier checkpoints preserve historical rejected attempts.

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Source/metadata publication and GitHub Actions compilation are authorized for this project. Never force-push. Keep original inputs, generated game code, executables, captures, audio, saves and private recovery identifiers out of public Git. Keep summaries short and omit black screenshots.

Details and reproduction settings: [current recovery record](docs/recovery/CURRENT-REBUILD.md).
