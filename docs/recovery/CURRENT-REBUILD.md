# Eight observed startup continuations from 7f3f4c7

Read [CHECKPOINT.md](../../CHECKPOINT.md) first. Base: `7f3f4c7dc0d326e2c2a009d889228330ef8ca239`. Goal: a full playable recompilation; menus and gameplay are still unverified.

## What changed

Eight more observed startup sections now execute. The continuation completes a four-row loop and a paired-row loop, restores their register and stack returns and a caller entry, then completes another alternating row-processing loop and its return. Execution follows the original control flow to a missing stack-save/call entry.

The runtime, generator, tests and build configuration are unchanged. All 166 preceding reviewed sections and all 599 original AOT pages / 603 files are preserved. The public implementation change is eight appended observed ranges in `config/observed_aot_supplements.json`, totaling 120 original words. Four status documents were updated after the final run; every other source byte matches the final tested candidate.

The resident checkout, six original input files, accepted round-20 native and all 603 original AOT files were authenticated before extension. No checkout/input restoration or reconfiguration was repeated. The existing configure log was copied as a historical configuration record. The accepted round-20 capture remains the comparison baseline; no unchanged baseline game run was repeated. New private helper copies preserve the preceding helpers and evidence and save explicit build/test exit receipts.

## Accepted fresh runs

| Round | Added interval, exclusive end | Actual MissingBlock stop | Extra executed instructions |
| --- | --- | --- | ---: |
| 21 | `[0x00154B44, 0x00154B90)` | `0x00154B90` | 70 |
| 22 | `[0x00154B90, 0x00154B94)` | `0x00154430` | 79,487 |
| 23 | `[0x00154430, 0x0015451C)` | `0x0015451C` | 353 |
| 24 | `[0x0015451C, 0x00154520)` | `0x0014FE5C` | 44,560 |
| 25 | `[0x0014FE5C, 0x0014FE74)` | `0x00154EE4` | 441 |
| 26 | `[0x00154EE4, 0x00154F38)` | `0x00154F38` | 21 |
| 27 | `[0x00154F38, 0x00154F68)` | `0x00154F68` | 36 |
| 28 | `[0x00154F68, 0x00154F6C)` | `0x0014F608` | 22,626 |

Each interval followed a recorded missing-code stop. No branch, return or completion was forced. Total advance from round 20 is **147,594 core-0 instructions and 550,494 diagnostic ns**; the displayed-period counter remains unchanged. These counters do not measure gameplay completeness or host performance. The larger advances after returns include subsequent execution through existing coverage; they are not instruction counts for the newly added intervals alone.

Eight accepted headless Release candidates each passed **90/90 enabled tests**, with 90 unique passing JUnit cases and zero failures, errors or skips. Each build and CTest invocation has an explicit exit-0 receipt. Source inventories were checked across the build/test gates. All eight accepted startup runs ended at missing original code with native exit 3, no timeout and empty stderr. Original inputs, all 603 original AOT files and frozen executable contents remained unchanged around the runs.

The final executable reports **111,218 runtime blocks / 548,285 verified instruction words**, including **174 logical sections / 175 page-local supplemental blocks / 3,174 supplemental words**. Its actual final stop is MissingBlock `0x0014F608`, thread 7, dispatch round 21846.

## Complete captures and semantic checks

The final capture contains **285,866 records / 4,573,888 bytes**. The entire round-20 baseline of 285,850 records / 4,573,632 bytes is an exact prefix, followed by 16 valid records whose two samples are zero. The first added eight records appear after round 22, another four after round 24 and the last four after round 28. Every accepted capture was checked against its actual length and reported record count. State and save contents remain byte-for-byte identical to the baseline.

Independent partial-state predictions matched four completed runs:

| Round | Instructions predicted and measured | Predicted fields | Preserved fields | Explicitly unknown loaded fields |
| --- | ---: | ---: | ---: | ---: |
| 21 | 70 | 6 | 9 | 4 |
| 23 | 353 | 7 | 3 | 9 |
| 26 | 21 | 6 | 10 | 3 |
| 27 | 36 | 5 | 11 | 3 |

- Round 21's counted suffix skips the one-time mask load and source adjustment. Guarded halfword and multi-register reads, intermediate truncation and pointer/flag updates remain the original operations.
- Round 23 executes its prologue once, keeps the saved caller frame active and repeats only the paired-row suffix. The live link register is reused as row data; the later stack return must use the saved return value.
- Round 26 reduces the observed count from four to three, leaving its conditional register return untaken. Round 27 alternates the new suffix and the previously covered suffix to reduce three to zero, without replaying setup.
- The round-22 and round-28 register returns use live LR. The round-24 multi-register stack return uses saved caller values and retains ordered reads, partial effects, delayed stack writeback and target validation. These return reviews predict immediate boundaries only; later memory-dependent caller execution is outside their scope.
- Round 25 saves its caller frame and loads two parameter words before branching to an existing suffix. The skipped four-word prefix performs coordinate-offset setup; the caller's stack save is a separate instruction. Replaying the skipped prefix would alter the caller-supplied parameter values. This review likewise does not predict the final run state, stop or instruction total.

Partial reported-state checks do not independently verify guest output memory, rendered pixels or audible content. Unknown loaded values remain excluded. Guarded memory widths, discarded trailing source values, staged shifts and wrapping additions, partial transfer effects and individual writebacks are preserved. No approximate pixel formula, forced return, new instruction policy or runtime decoding was introduced.

## Attempt history and retained text limitations

The only rejected native launch in this batch was a **round-27 prelaunch permission failure**. The frozen native had mode `0644`; the launch raised `EACCES` before a child game process started. No audio capture was produced. The rejected executable's bytes matched the verified native and the archived executable, which retained mode `0555`. The actual rejected bytes, modes and traceback were preserved privately. The cause remains unestablished.

Execute permission was restored to `0555` on the unchanged verified bytes. A retry in fresh owned storage produced the accepted finite run and matched its partial-state prediction. No rebuild, instruction change or test relaxation was needed. The round-28 permission check found mode `0555` already present; its native succeeded on its first run. All other accepted candidates also succeeded on their first native launch. **There were no failed build or test gates in this batch.**

The retained CTest text logs for rounds **23 and 24 end after completed test 88**, without final summaries. Each corresponding JUnit file contains 90 unique passing cases and matches its saved verification hash; each explicit CTest receipt records exit 0 and no timeout. Those short text logs remain unchanged and are a retained-text limitation. The other six current CTest text logs contain complete passing summaries.

Additional retained text logs are empty: the round-21 next-review log, round-22/24/27 next-coverage logs, and round-27 process stdout. Bound review, coverage, comparison and result JSON plus successful orchestration receipts remain available. Keep the empty text files unchanged; they do not replace or invalidate those complete structured records.

A separate backup verification found the original round-28 native archive shorter than its recorded size and truncated at gzip EOF. The actual tested native and successful run evidence remain intact. The rejected archive and its original receipt are preserved, and the cause remains unknown. A replacement archive was created from the intact verified candidate and saved after full gzip EOF/CRC checks and verification of all 13 member names, sizes, modes and hashes. The compressed hash and complete decompression were checked again before and after saving; the exact native hash is unchanged. This was a backup-integrity failure, separate from the eight successful build/test gates and accepted native runs.

The preceding checkpoint preserves the two rejected round-15 verification attempts and the short round-18/19 text logs, with their successful recovery evidence. Older linked checkpoints preserve earlier failed environment checks and a rejected truncated capture. These remain historical records rather than fresh tests or failures in this batch. See the [preceding recovery record](https://github.com/GTTeancum/Lego-Undercover-3DS-Recomp/blob/7f3f4c7dc0d326e2c2a009d889228330ef8ca239/docs/recovery/CURRENT-REBUILD.md).

## Reproduce the build

The current batch reused the resident configured Release build. For a fresh environment, use the exact supported original inputs and the preserved 599-page AOT archive, keeping generated content and build outputs private:

```sh
cmake -S . -B /absolute/private/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DLEGO_BUILD_DESKTOP=OFF \
  -DLEGO_AOT_DIR=/absolute/private/aot/generated2 \
  -DLEGO_SUPPLEMENT_CODE_BIN=/absolute/private/inputs/code.bin
cmake --build /absolute/private/build --parallel 6
ctest --test-dir /absolute/private/build --output-on-failure --timeout 90
```

The recorded toolchain is CMake 3.28.3, Ninja 1.11.1, GCC 13.3.0, Clang/LLVM 18.1.3 and Python 3.12.14. When tests run as root, their isolated permission-test children must be allowed to drop identity. Use the permitted execution context supporting those unchanged tests; retain explicit completion codes and the full JUnit reports.

The private checkpoint retains the exact headless command and diagnostic profiles. Use fresh owned state/save/audio folders, the same authenticated inputs, the 100,000,000 block limit and a 600-second process limit. Check the frozen executable's content hash and execute permission immediately before launch. Compare complete audio records, state/save bytes and the actual stop; a reported record count alone is insufficient.

## Visible status and hosted checks

Menus, gameplay, speaker playback and Windows execution remain unverified. No new screen capture, windowed run or capture-on/off comparison was performed. Captured audio is diagnostic data; the 16 appended records have zero-valued samples. No fresh visible or audible-content milestone is claimed.

Hosted checks are **pending at source-preparation time**. Actual results are recorded separately against the published commit in the private publication receipt. Successful checks for a preceding commit remain historical. Hosted checks are ROM-free and do not execute the original game.

## Next exact task

Actual stop: **MissingBlock `0x0014F608`**. The next reviewed interval is **`[0x0014F608, 0x0014F610)`**, two original words, and **has not been admitted or executed**. It saves the incoming link register at stack pointer minus four with guarded preindexed writeback, then branches with link to the exact existing nine-word entry at `0x0014FF08`. The call assigns link register `0x0014F610`; that address is currently missing and remains unobserved and excluded.

Preserve the incoming saved link separately from the newly assigned call link. A failed stack store must leave SP and LR unchanged and prevent the call; successful stack writeback occurs before the call changes LR. The review covers only the immediate two-instruction boundary, conditional on the original guarded store succeeding. It does not assert the callee's later behavior or the final native-run state, stop or instruction count.

Preserve all **174 reviewed sections** and every original page. Reauthenticate the exact words and generator, admit only this observed interval, generate/verify, incrementally build/test, preserve the exact native and check its permissions, then perform one finite isolated run with complete artifact checks. Use ordinary chat/cloud tools only. Publishing source/metadata and compiling through GitHub Actions are authorized for this project; keep original inputs, generated instructions, natives and full evidence private, and never force-push.
