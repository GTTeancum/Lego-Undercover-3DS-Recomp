# October 30, 2026 — three observed startup gaps cleared

## What changed

Read [CHECKPOINT.md](../../CHECKPOINT.md) first. Three missing startup sections were added after their actual preceding stops. The last section needed canonical A32 AND-immediate admission in the rebuild tool. Fresh headless runs passed all three gaps; menus and gameplay remain unverified.

Base commit: `b9353b73cd07e4c762dc9dd23d93d1ad1628b1c5`.

Added 3 blocks / 72 words. Totals are **36 supplements / 525 words**, **111,079 registered blocks / 545,636 verified instruction words**. All **599 original AOT pages / 603 files** and the 33 preceding supplemental records are unchanged.

## Actual fresh runs

| Round | Added interval | Actual next missing block | Extra core-0 instructions over preceding run |
| --- | --- | --- | ---: |
| 01 | `[0x00150EC0, 0x00150ED0)`; 4 words | `0x00150ED0` | 26 |
| 02 | `[0x00150ED0, 0x00150EDC)`; 3 words | `0x00152BC0` | 9 |
| 03 | `[0x00152BC0, 0x00152CC4)`; 65 words | `0x00150EE8` | 452 |

Every run used its frozen tested native, authenticated original inputs, new owned state/save directories and a finite 600-second wall bound. All completed before that bound, with native exit **3**, **MissingBlock**, empty stderr and unchanged source, executable and original AOT inventories. The wrapper's administrative success means the expected diagnostic stop was recorded, not that the game booted successfully.

The final headless run executed **61,372,742 core-0 instructions** and **99 core-1 instructions**. It recorded **2,345,554,689 diagnostic CPU ticks**, **8,748,418,381 ns** guest time, **523 display periods**, and **285,726 audio records**. The stop was thread 7, dispatch round 21,843. Elapsed host time was 122.053 seconds; this is not a game-performance measurement.

Against the retained same-mode baseline: **+487 core-0 instructions / +487 diagnostic ticks / +1,817 diagnostic ns**. Complete audio bytes, state contents and save contents match the baseline in every round. This does not establish audible playback or gameplay correctness.

## Build and instruction verification

Enabled CTest suites passed once per build: **83/83, 83/83 and 84/84**, with no failures, errors or skipped enabled cases. All three local builds had `LEGO_BUILD_DESKTOP=OFF`. The final count includes the new `ctr_and_immediate_test` and excludes the optional SDL smoke test. The Python policy suite has 35 passing methods.

The new build-time admission accepts only classic A32 immediate AND/ANDS with ordinary source/destination registers and conditions 0 through 14. It retains the complete rotated immediate and flag bit, rejects PC operands and reserved conditions, and uses the unchanged existing CoreAlu evaluator. All prior text-range, first-exit, same-page, nonoverlap and original-input checks remain in place. Runtime, service and renderer source files are unchanged.

Independent synthetic tests cover rotations, values, NZCV, false and true conditions, overlapping registers, SP/LR, preservation of unrelated CPU state, and strict missing-code stops. Python tests also cover all immediate bytes, allowed register/condition combinations, neighboring rejected encodings and exact generated metadata. No game words are included in public tests.

Each round preserves the original build log, actual JUnit, full source identities, generated verification, exact native and full run output. Final publication changes only four status documents after the last game run; all non-status source bytes match the tested candidate. The public tree contains 524 source files. Private recovery stores the current evidence directly rather than recursively duplicating older archives; older backups remain separately preserved.

## Visual and platform limits

**No fresh desktop game run or screenshot was performed for this checkpoint.** SDL3 was unavailable in this local build environment. The last verified windowed result is the loading image from the preceding checkpoint, not new visual evidence. The prior 3,883-instruction desktop/headless difference remains untraced. Menus, gameplay, speaker playback and Windows execution remain unverified.

## Next exact task

Add only **`[0x00150EE8, 0x00150EF4)`**, one block / three words / 12 bytes. This actual observed stop has two pointer adjustments followed by a post-indexed stack load into PC. All forms are already supported. Preserve guarded memory access and ordinary target validation; the stack-loaded return value is unobserved and must not be inferred from live LR.

Preserve the 36 existing records. Adding that reviewed range alone would give 37 supplements / 528 words. Authenticate, generate, verify, build/test, preserve the exact native, then use one bounded fresh run. Do not add an unobserved successor.

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Publish source/metadata to `main` without force-pushing. Keep original inputs, generated code, executables and run outputs private. User summaries should say what improved, whether startup gets farther, and what still blocks play.

The preceding checkpoint remains in Git history at `b9353b7`; earlier recovery history is in [HISTORY-7cf0d2a.md](HISTORY-7cf0d2a.md).
