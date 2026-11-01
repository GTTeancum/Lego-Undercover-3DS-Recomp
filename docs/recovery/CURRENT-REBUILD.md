# Six observed startup continuations from 7ddc51e

Read [CHECKPOINT.md](../../CHECKPOINT.md) first. Base: `7ddc51e0c03b57b29140bf0ef2f99ee72bb91d99`. Goal: a full playable recompilation; menus and gameplay are still unverified.

## What changed

Six more missing startup sections now execute. An eight-row copy loop, an alternating row-processing loop and their returns complete through the original control flow. A one-instruction mask-load prefix rejoins unchanged original AOT coverage and advances startup by another 24,040 instructions before the next missing row-processing entry.

The runtime, generator, tests and build configuration are unchanged. All 160 preceding reviewed sections and all 599 original AOT pages / 603 files are preserved. Generated operations authenticate against the exact supported original input. The public implementation change is six appended observed ranges in `config/observed_aot_supplements.json`, including an authenticated join to an unchanged original block. Four status documents were updated after the final run. Every other source byte matches the final tested candidate.

The checkout, all six restored input files, the exact preceding native and all 603 original AOT files were authenticated before extension. The accepted round-14 native and capture were reused as the comparison baseline; no unchanged baseline game run was repeated. New private orchestration helpers preserve the preceding helpers and evidence.

## Accepted fresh runs

| Round | Added interval, exclusive end | Actual MissingBlock stop | Extra executed instructions |
| --- | --- | --- | ---: |
| 15 | `[0x001543F0, 0x0015442C)` | `0x0015442C` | 113 |
| 16 | `[0x0015442C, 0x00154430)` | `0x00154974` | 24 |
| 17 | `[0x00154974, 0x00154A14)` | `0x00154A14` | 40 |
| 18 | `[0x00154A14, 0x00154A6C)` | `0x00154A6C` | 66 |
| 19 | `[0x00154A6C, 0x00154A70)` | `0x00154AA0` | 278 |
| 20 | `[0x00154AA0, 0x00154AA4)` | `0x00154B44` | 24,040 |

Each interval followed a recorded missing-code stop. No branch, return or completion was forced. Total advance: **24,561 core-0 instructions and 91,607 diagnostic ns**; the displayed-period counter remains unchanged. These diagnostic counts do not measure gameplay completeness or host performance. The final 24,040-instruction advance includes execution enabled by the new prefix; it is not a claim that every executed instruction belonged to unchanged code.

Six accepted headless Release candidates each passed **90/90 enabled tests**, with zero failures, errors or skips in their accepted gate. All six fresh startup runs ended at missing original code with native exit 3, no timeout and empty stderr. Each capture was checked against its actual byte length and record count. State and saves remain byte-for-byte identical to the baseline.

The first five captures exactly match the baseline: 285,846 records / 4,573,568 bytes. The final capture contains 285,850 records / 4,573,632 bytes: the entire prior capture is an exact prefix, followed by four valid records whose two samples are zero. This is complete diagnostic capture evidence; it does not establish speaker playback or audible content.

The final executable reports **111,210 runtime blocks and 548,165 verified instruction words**, including 166 logical sections / 167 page-local supplemental blocks / 3,054 supplemental words. Original inputs, all 603 original AOT files and each frozen executable were verified around the accepted runs. Source inventories were checked across every new build and test gate.

## Meaningful semantic checks

- The eight-row copy loop matched its pre-run prediction: 113 instructions, pointer advances, zero count, flags and 14 predicted or preserved reported-state fields. Five guest-data-loaded registers were explicitly unpredicted. The suffix branch skips the one-time source adjustment.
- The next row-processing entry matched 40 instructions and 13 reported fields. Its initial count of four becomes three, so the conditional return is untaken. Six guest-data-derived registers were explicitly unpredicted.
- The alternating suffix matched 66 instructions and 13 reported fields: 22 new instructions, 22 instructions from the preceding reviewed suffix, then the new 22 again. Counts progress from three to two to one to zero without replaying setup.
- The restored register returns were independently authenticated with their existing destinations. Their immediate boundary effects were reviewed; final state after the recorded callers read guest memory was left unpredicted.
- The mask-load prefix joins an exact unchanged original block using the original page hash. Review authenticates the existing routine and its conditional 71-instruction path to its first return. That local boundary prediction is not presented as an observed intermediate-state check or a final-run prediction.
- Guarded byte, halfword and multiple-register reads, staged shifts and wrapping additions, ordered partial effects and individual postindexed writebacks remain the original operations. No approximate pixel formula, forced return, new instruction policy or runtime decoding was introduced.

Partial reported-state checks do not independently verify output memory, rendered pixels or audible content. Unknown guest-memory values remain explicitly excluded from predictions.

## Attempt history

The first round-15 attempt recorded **87 passing tests and three BAD_COMMAND launch failures**. Its build and Ninja logs ended at 125/849 steps even though the helper reached CTest. One rejected executable was empty and two others lacked execute permission. The logs do not establish the underlying cause; no native game run was performed.

A preserved clean-rebuild attempt completed all **849 build steps**, then recorded **89 passing tests and one BAD_COMMAND launch failure**. The affected executable was an ELF file without execute permission; CTest returned 8. It was preserved and relinked, after which the complete existing suite passed **90/90**. No test assertions, source code or build configuration were weakened or disabled.

Rounds 16-20 passed their build and test gate on the first attempt. All six accepted natives produced complete startup evidence on their first execution. Both new rejected verification attempts remain in the private checkpoint, alongside the successful recovery records.

The retained CTest text logs for rounds 18 and 19 stop after tests 83 and 88 respectively. Their complete JUnit files each contain 90 unique passing cases and match the hashes in the successful verification receipts. Those text logs remain unchanged; the full XML results and successful process completion support the accepted gates.

The preceding checkpoint retains older failed environment checks and a rejected truncated capture. Those are historical attempts and are not counted as fresh tests or failures here. See the [preceding recovery record](https://github.com/GTTeancum/Lego-Undercover-3DS-Recomp/blob/7ddc51e0c03b57b29140bf0ef2f99ee72bb91d99/docs/recovery/CURRENT-REBUILD.md).

## Reproduce the build

Use the exact supported original inputs and the preserved 599-page AOT archive. Keep all generated content and build outputs private. With CMake, GCC, Clang, LLVM objdump, Ninja and Python available:

```sh
cmake -S . -B /absolute/private/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DLEGO_BUILD_DESKTOP=OFF \
  -DLEGO_AOT_DIR=/absolute/private/aot/generated2 \
  -DLEGO_SUPPLEMENT_CODE_BIN=/absolute/private/inputs/code.bin
cmake --build /absolute/private/build --parallel 6
ctest --test-dir /absolute/private/build --output-on-failure --timeout 90
```

When tests run as root, their isolated permission-test children must be allowed to drop identity. This batch used the permitted cloud execution environment supporting those unchanged tests. The restored toolchain was CMake 3.28.3, Ninja 1.11.1, GCC 13.3.0, Clang/LLVM 18.1.3 and Python 3.12.14.

The private checkpoint retains the exact headless command and diagnostic profiles. Use fresh owned state/save/audio folders, the same original inputs, the 100,000,000 block limit and a 600-second process limit. Compare complete audio records, state/save bytes and the actual stop; a reported record count alone is insufficient.

## Visible status and hosted checks

Menus, gameplay, speaker playback and Windows execution remain unverified. No new screen capture or windowed run was performed. Captured audio is diagnostic data, not speaker playback. The four appended records have zero-valued samples. No new visual milestone, audible-content result or capture-on/off comparison is claimed.

The 7ddc51e GitHub compiler, sanitizer, packaging and source-recovery checks passed. Publication and hosted-check results for this batch are recorded separately in the private publication receipt. Hosted checks are ROM-free and do not execute the original game.

## Next exact task

Actual stop: **MissingBlock `0x00154B44`**. The next reviewed interval is **`[0x00154B44, 0x00154B90)`**, 19 original words, and **has not been added**. It loads the mask and advances the source by two once, then reads a preceding halfword and an aligned multi-register group, packs and combines the values, stores two words, advances both row pointers and repeats the suffix at `0x00154B4C` while the count is nonzero. Preserve the one-time setup, all guarded reads, staged arithmetic, partial memory effects, pointer-update order and flags. The loop-exit successor at `0x00154B90` remains unobserved and excluded.

Preserve all 166 reviewed sections and every original page. Use ordinary chat/cloud tools only. Publishing source/metadata and compiling through GitHub Actions are authorized for this project; keep original inputs, generated instructions, natives and full evidence private, and never force-push.
