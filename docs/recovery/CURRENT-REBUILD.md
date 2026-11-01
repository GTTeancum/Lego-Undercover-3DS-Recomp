# Eight observed startup continuations from c0383c3

Read [CHECKPOINT.md](../../CHECKPOINT.md) first. Base: `c0383c339eda6c391b21fa91d8a821d66f78983d`. Goal: a full playable recompilation; menus and gameplay are still unverified.

## What changed

Eight more missing startup sections now execute. A four-row copy loop, its nested stack returns, and a larger row-processing loop complete through the original control flow. Restoring the final return advances execution by 28,863 instructions before the next missing row-copy entry.

The runtime, generator, tests and build configuration are unchanged. All 152 preceding reviewed sections and all 599 original AOT pages / 603 files are preserved. Generated operations authenticate against the exact supported original input. The only public implementation change is eight appended observed ranges in `config/observed_aot_supplements.json`. Four status documents were updated after the final run. Every other source byte matches the final tested candidate.

The published checkout, original inputs, exact preceding native and all 603 AOT files were authenticated before extension. The prior accepted native was reused as the comparison baseline; no unchanged baseline game run was repeated. Private orchestration helpers were adapted to this baseline while preserving the old helpers and evidence.

## Accepted fresh runs

| Round | Added interval, exclusive end | Actual MissingBlock stop | Extra executed instructions |
| --- | --- | --- | ---: |
| 07 | `[0x0014FFA8, 0x0014FFB8)` | `0x00154a70` | 66 |
| 08 | `[0x00154A70, 0x00154A9C)` | `0x00154a9c` | 44 |
| 09 | `[0x00154A9C, 0x00154AA0)` | `0x0014ffb8` | 70 |
| 10 | `[0x0014FFB8, 0x0014FFBC)` | `0x0014fee8` | 1 |
| 11 | `[0x0014FEE8, 0x0014FEF0)` | `0x0014fbac` | 2 |
| 12 | `[0x0014FBAC, 0x0014FBB4)` | `0x00153c24` | 83 |
| 13 | `[0x00153C24, 0x00153D54)` | `0x00153d54` | 238 |
| 14 | `[0x00153D54, 0x00153D58)` | `0x001543f0` | 28,863 |

Each interval followed a recorded missing-code stop. No branch, return or completion was forced. Total advance: 29,367 core-0 instructions and 109,533 diagnostic ns; the displayed-period counter remains unchanged. These diagnostic counts do not measure gameplay completeness or host performance.

Eight newly built headless Release candidates each passed **90/90 enabled tests**, with zero failures, errors or skips. All eight fresh startup runs ended at missing original code with native exit 3, no timeout and empty stderr. Each capture was checked against its actual byte length and record count. State and saves remain byte-for-byte identical to the baseline.

The first seven captures exactly match the baseline: 285,842 records / 4,573,504 bytes. The final capture contains 285,846 records / 4,573,568 bytes: the entire prior capture is an exact prefix, followed by four valid records whose two samples are zero. This is complete diagnostic capture evidence; it does not establish speaker playback or audible content.

The final executable reports 111,204 runtime blocks and 548,085 verified instruction words. Original inputs, all 603 original AOT files and each frozen executable were verified around the accepted runs. Source inventories were checked across every new build and test run.

## Meaningful semantic checks

- The four-row copy loop matched a pre-run scalar prediction: 44 instructions, pointer advances, zero count, flags, and 16 predicted or preserved reported-state fields. Three guest-data-loaded registers were explicitly unpredicted.
- The following recorded return path matched its conditional 70-instruction prediction, stack position and flags. Two later saved-stack returns matched their complete reported register-state predictions. Saved stack addresses were kept distinct from live LR.
- The 76-word row-processing interval matched 238 instructions: a 22-word prefix once and a 54-word suffix four times. Predicted pointers, mask, count, flags and stack position matched, including a single prologue push. Nine data-derived registers were explicitly unpredicted.
- Review preserves staged byte arithmetic and truncation, ordered partial memory effects and delayed stack writeback. No approximate pixel formula, forced return, new instruction policy or runtime decoding was introduced.
- The final restored multi-register return naturally reached the next missing entry after another 28,863 instructions. No rendered-pixel or gameplay conclusion is inferred from this control-flow result.

Predictions based on saved-stack lineage state their successful-read and intact-slot conditions. Unknown destinations were left unpredicted when the available predecessor capture did not establish the saved value. Loaded and derived guest-memory values excluded from partial predictions are not presented as verified output bytes.

## Attempt history

All eight new candidate builds passed the existing 90-test suite, and all eight finite game runs produced complete accepted evidence on their first attempt. No new source tests or runtime workarounds were introduced.

The previous checkpoint retains the earlier incomplete build outputs, permission-test environment repair, and rejected truncated capture. Those are historical attempts, not failures or fresh tests in this batch. Their complete evidence and recovery references remain in the preceding private archive. See the [preceding recovery record](https://github.com/GTTeancum/Lego-Undercover-3DS-Recomp/blob/c0383c339eda6c391b21fa91d8a821d66f78983d/docs/recovery/CURRENT-REBUILD.md) for that history.

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

When tests run as root, their isolated permission-test children must be allowed to drop identity. This batch used the permitted cloud execution environment that supports those existing tests, without weakening or disabling them.

The private checkpoint retains the exact headless command and diagnostic profiles. Use fresh owned state/save/audio folders, the same original inputs, the 100,000,000 block limit and a 600-second process limit. Compare complete audio records, state/save bytes and the actual stop; a reported record count alone is insufficient.

## Visible status and hosted checks

Menus, gameplay, speaker playback and Windows execution remain unverified. No new screen capture or windowed run was performed. Captured audio is diagnostic data, not speaker playback. The four appended records have zero-valued samples. No new visual milestone, audible-content result or capture-on/off comparison is claimed.

The c0383c3 GitHub compiler, sanitizer, packaging and source-recovery checks passed. Publication and hosted-check results for this batch are recorded separately in the private publication receipt. Hosted checks are ROM-free and do not execute the original game.

## Next exact task

Actual stop: **MissingBlock `0x001543F0`**. The next reviewed interval is **`[0x001543F0, 0x0015442C)`**, 15 original words, and **has not been added**. It advances the source by one once, then copies rows using byte and aligned multi-register reads, packing shifts, stores, the original stride and a count-controlled suffix branch to `0x001543F4`. Preserve the one-time adjustment, all guarded reads including the trailing byte discarded from output, partial memory effects, pointer-update order and flags. The loop-exit successor at `0x0015442C` remains unobserved and excluded.

Preserve all 160 reviewed sections and every original page. Use ordinary chat/cloud tools only. Publishing source/metadata and compiling through GitHub Actions are authorized for this project; keep original inputs, generated instructions, natives and full evidence private, and never force-push.
