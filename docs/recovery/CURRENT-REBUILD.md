# October 30, 2026 — observed startup continuation

## Current continuation

Read [CHECKPOINT.md](../../CHECKPOINT.md) first. The rebuild now accepts the required arithmetic and larger code sections. It adds **2 missing sections of startup code**. The game gets farther through startup in both the windowed and background checks.

The tested candidate starts from `7cf0d2a`. The continuation adds **2 metadata blocks / 73 instruction words** beyond the prior 31 blocks / 380 words. Totals are **33 supplements / 453 words**, with **111,076 registered blocks / 545,564 verified instruction words**. All **599 original AOT pages / 603 files** and all 31 preceding supplement records remain unchanged.

`tools/generate_aot_supplement.py` now admits canonical AND/SUB register operands with immediate shifts and permits same-page supplement ranges up to 512 bytes. The new arithmetic admission excludes PC operands, register-specified shifts, reserved encodings and condition 15. First-exit, authenticated-input, text, alignment and nonoverlap guards remain unchanged. `tests/test_generate_aot_supplement.py` covers admitted and rejected encodings and bounded larger spans. `tests/ctr_supplemental_registry_test.cpp` adds synthetic value, condition and flag checks, plus a 96-word block whose interior loop verifies stepping and dispatch without replaying its prefix. Existing evaluator and runtime source files are unchanged.

Each added range followed an authenticated actual preceding stop. Original words and preserved supplemental records were verified before each bounded run; future stops were not assumed.

| Observed round | Added metadata | Actual headless stop | Change from preceding headless |
| --- | --- | --- | --- |
| 01 | `[0x001528A0, 0x001529B8)`; 70 words | MissingBlock `0x001529B8` | +175 core-0 instructions / +653 diagnostic ns |
| 02 | `[0x001529B8, 0x001529C4)`; 3 words | MissingBlock `0x00150EC0` | +3 core-0 instructions / +11 diagnostic ns |

The first row compares with the published 7cf0d2a headless baseline; later rows compare with the preceding round. Only the final round also received a fresh desktop run.

Both final modes stop at **MissingBlock `0x00150EC0`**. Native-child / outer-launcher exits are **3/3** headless and **3/3** desktop. Neither run timed out or wrote stderr.

Cumulative change from the preserved same-mode `7cf0d2a` baseline: headless **+178 core-0 instructions / +664 diagnostic ns**; desktop **+178 core-0 instructions / +664 diagnostic ns**.

| Final actual measurement | Headless | SDL desktop |
| --- | ---: | ---: |
| Actual stop | MissingBlock `0x00150EC0` | MissingBlock `0x00150EC0` |
| Core-0 instructions | 61,372,255 | 61,368,372 |
| Core-1 instructions | 99 | 99 |
| Diagnostic CPU ticks | 2,345,554,202 | 2,345,554,202 |
| Diagnostic guest time, ns | 8,748,416,564 | 8,748,416,564 |
| Thread / dispatch round | 7 / 21,843 | 7 / 21,843 |
| Display periods / presentations | 523 / N/A | 523 / 524 |
| Native-child / outer-launcher exit | 3 / 3 | 3 / 3 |
| Wall timeout | False | False |
| Host duration, seconds | 179.342 | 150.937 |
| Complete audio records | 285,726 | 285,726 |

Native-child and outer-launcher statuses describe the actual runtime. The runner’s administrative completion status is recorded separately and does not establish a successful game boot.

## Validation and provenance

The exact final native passed **84/84 enabled CTest suites** in its single full-suite invocation. The actual final JUnit records **0 failures, 0 errors and 0 skipped cases**. No test permission correction or automatic retry was used for that final build.

The exact final native was preserved before both final runs. Every round keeps its own actual build/JUnit, selected executable, generated verification, source/input identities, bounded-run receipts and complete outputs. Each new round has an actual **84/84 passing JUnit** from one full invocation. The frozen candidate contained **522 tracked files: 518 other files and the same four existing status documents**. All 522 candidate identities and all 606 original-input identities were checked before and after the build and game runs. The four status documents were updated after the final desktop capture closed. During publication recovery, the preceding recovery history was moved unchanged to a separate file; the published tree contains 523 tracked files. Runtime, test, tool and configuration bytes still match the tested candidate.

Cross-mode comparison matches final registers, diagnostic time, complete audio, state contents and save contents. The phase-2 comparison still records **3,883 fewer desktop core-0 instructions** than headless; the difference remains untraced.

The desktop records **524 presentations**. The actual **1280×720 first-nonblack startup capture** has identical decoded RGBA pixels to the original baseline; complete BMP bytes are identical. This records the initial startup image. Usable menus, gameplay, speaker playback and Windows execution remain unverified.

Complete audio, state and save comparisons use their full captured contents. Screenshot comparison uses complete BMP files and decoded RGBA pixels. Original inputs, generated game code, native bodies and runtime outputs remain private.

## Next exact task

Append only **`[0x00150EC0, 0x00150ED0)` — 1 block / 4 words / 16 bytes**. Four original instructions advance a working pointer, restore two stack values, adjust an argument, and call an already recorded routine. Existing fast and memory handlers cover the section; its later return remains unobserved.

Reviewed continuations: direct call after successful stack reads `0x00150DDC` is recorded original; assigned LR; eventual return remains conditional `0x00150ED0` is unrecorded. Keep unobserved successor bodies outside this change.

Preserve the 33 current records. If this reviewed range alone is added, conditional totals become **34 supplements / 457 words**. Generate only from authenticated private input, verify existing admission and exact preservation, build/test, preserve the tested native, and use a finite fresh run to identify the next observed stop. Do not add unobserved successors.

Work in standard chat and the cloud; do not use Codex services, Dot or the user’s computer. Ordinary source/metadata publication to `main` is authorized; do not force-push. Keep original inputs, generated code, executables, captures, audio, saves and private recovery identifiers out of public Git. Keep user updates short and in plain English: what improved, whether the game gets farther, and what still blocks play.

## Earlier checkpoints

The preceding recovery history is retained byte-for-byte in [HISTORY-7cf0d2a.md](HISTORY-7cf0d2a.md). This file keeps only the latest continuation so future updates do not duplicate the full history.
