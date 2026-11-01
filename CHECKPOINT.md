# Latest LEGO Undercover 3DS checkpoint

Updated October 31, 2026 (Eastern). Resume from `main`.

## Objective and current state

Goal: a full playable native recompilation of **LEGO City Undercover: The Chase Begins**.

Six more missing startup sections now run, including the unfinished caller step recovered from the preceding session. Startup proceeds through a table lookup, bit processing, its table dispatch, a processing path that returns through existing code, and the following call. It now reaches another missing setup entry.

Menus, gameplay, speaker playback and Windows execution remain unverified. No new screen capture or windowed run was performed. Earlier black captures remain historical private diagnostics.

## Changes and actual checks

Base: `f18fb670b8a7c1c6fbd05df4c901c8ccd4c69468`. The runtime, generator, tests and build configuration are unchanged. All 146 preceding reviewed sections and all 599 original AOT pages / 603 files are preserved. Generated operations authenticate against the exact supported original input. The only public implementation change is six appended observed ranges in `config/observed_aot_supplements.json`.

Totals: **152 reviewed sections / 153 page-local blocks / 2,876 supplemental words**; **111,196 runtime blocks / 547,987 verified words**.

Five newly built headless Release candidates each passed **90/90 enabled tests**, with zero failures, errors or skips. The recovered candidate retains its verified historical 90-test report and was executed again in this session. All six accepted fresh startup runs ended at missing original code with native exit 3, no timeout and empty stderr. Every accepted capture contains all 285,842 reported records (4,573,504 bytes). Complete audio, state and saves are byte-for-byte identical to the baseline.

Independent scalar calculations predicted the table-processing result and computed dispatch destination before execution; both runs matched every predicted register, preserved register and instruction count. The processing path then returned through existing code after 102 further instructions.

Total advance from the published baseline: **147 executed instructions / 548 diagnostic ns**. These are not gameplay percentages or host-performance measurements.

The earlier failed environment checks and rejected capture remain in the private recovery evidence. See the detailed recovery record for their resolution.

## Remaining blocker and next action

Actual stop: **MissingBlock `0x0014FFA8`**. The next reviewed interval is **`[0x0014FFA8, 0x0014FFB8)`**, four original words. It saves LR on the guest stack, sets both working dimensions to four, and calls the already recorded routine at `0x0014F444`. Existing handlers accept the interval. Preserve guarded stack-write ordering, flags, the link assignment and ordinary target validation. The assigned continuation at `0x0014FFB8` remains unobserved and must not be included.

Preserve all 152 reviewed sections. Reauthenticate the private inputs, review the complete first-exit interval, generate and verify exact words, build/test, preserve the exact native and use fresh owned storage for a finite run. Verify actual audio-file length against the reported record count. Do not add unobserved successors, infer a stack return from live LR, decode at runtime or fabricate GPU completion.

## Resume and permissions

Repository: `GTTeancum/Lego-Undercover-3DS-Recomp`, branch `main`. The private recovery checkpoint records the published commit, original-input recovery references, exact executable, run commands, successful results, failed attempts and next review.

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Source/metadata publication and GitHub Actions compilation are authorized for this project. Never force-push. Keep original inputs, generated game code, executables, captures, audio, saves and private recovery identifiers out of public Git. Keep summaries short and omit black screenshots.

Details and reproduction settings: [current recovery record](docs/recovery/CURRENT-REBUILD.md).
