# Eight observed startup continuations from 9cd3f9c

Read [CHECKPOINT.md](../../CHECKPOINT.md) first. Base: `9cd3f9ca88ea4820abf66b6f6f8276c4ba2d4432`. Goal: a full playable recompilation; menus and gameplay are still unverified.

## What changed

Eight more observed startup sections now execute. The continuation restores a stack-save/call pair, a parameter-adjust/call pair and their stack return, then completes an alternating row-processing loop and its register return, followed by a wider paired-row loop and its stack return. Execution follows original control flow to a missing halfword-copy loop.

The public implementation change is eight appended observed ranges in `config/observed_aot_supplements.json`, totaling 92 original words. All 174 preceding reviewed sections and all 599 original AOT pages / 603 files are preserved. The runtime, generator, tests and build configuration are unchanged. Four status documents were updated after the final run; every other source byte matches the final tested candidate.

The resident checkout, six original input files, accepted round-28 native and all 603 original AOT files were authenticated before extension. No restoration or new configuration was performed. The existing configure log was copied as a historical configuration record. The accepted round-28 capture remains the comparison baseline; no unchanged baseline game run was repeated.

New private helper copies preserve earlier helpers and evidence. Each candidate archive contains exactly 12 regular files and is verified through complete gzip EOF/CRC, exact member names, sizes, modes and hashes, source rechecks and compressed hashes before and after verification. Launch requires a successful private save receipt and matching archive identity. The native's hash and execute permission are checked immediately before launch; archive identity is checked again after execution.

## Accepted fresh runs

| Round | Added interval, exclusive end | Actual MissingBlock stop | Extra executed instructions |
| --- | --- | --- | ---: |
| 29 | `[0x0014F608, 0x0014F610)` | `0x0014F610` | 307 |
| 30 | `[0x0014F610, 0x0014F618)` | `0x0014F618` | 198 |
| 31 | `[0x0014F618, 0x0014F620)` | `0x0015524C` | 351 |
| 32 | `[0x0015524C, 0x00155284)` | `0x00155284` | 14 |
| 33 | `[0x00155284, 0x001552A4)` | `0x001552A4` | 56 |
| 34 | `[0x001552A4, 0x001552A8)` | `0x001540F0` | 4,186 |
| 35 | `[0x001540F0, 0x001541E8)` | `0x001541E8` | 370 |
| 36 | `[0x001541E8, 0x001541EC)` | `0x001550E8` | 9,057 |

Each interval followed a recorded missing-code stop. No branch, return or completion was forced. Total advance from round 28 is **14,539 core-0 instructions and 54,228 diagnostic ns**; the displayed-period counter remains unchanged. These counters do not measure gameplay completeness or host performance. Larger advances after calls and returns include execution through existing coverage, beyond the newly admitted words.

Eight headless Release candidates each passed **90/90 enabled tests**, with 90 unique passing JUnit cases and zero failures, errors or skips. Each build and CTest invocation has an explicit exit-0 receipt. Source inventories were checked across the build/test gates. All eight accepted startup runs ended at missing original code with native exit 3, no timeout and empty stderr. Original inputs, all 603 original AOT files, frozen executables and their saved archives remained unchanged around the runs.

The final executable reports **111,226 runtime blocks / 548,377 verified instruction words**, including **182 logical sections / 183 page-local supplemental blocks / 3,266 supplemental words**. Its actual final stop is MissingBlock `0x001550E8`, thread 7, dispatch round 21846.

## Complete captures and semantic checks

The final capture contains **285,870 records / 4,573,952 bytes**. The entire round-28 baseline of 285,866 records / 4,573,888 bytes is an exact prefix, followed by four valid records whose two samples are zero. Rounds 29–35 are byte-identical to the baseline; only round 36 extends it. Every accepted capture was checked against its actual length and reported record count. Persisted state and save files remain byte-for-byte identical to the baseline.

Independent partial-state predictions matched three completed runs:

| Round | Instructions predicted and measured | Predicted fields | Preserved fields | Explicitly unknown loaded fields |
| --- | ---: | ---: | ---: | ---: |
| 32 | 14 | 6 | 10 | 3 |
| 33 | 56 | 6 | 10 | 3 |
| 35 | 370 | 7 | 3 | 9 |

- Round 29 saves the incoming caller link before calling an existing nine-word section. Round 30 adjusts a parameter and calls the same entry. Round 31 restores that parameter and returns through the saved stack word. The saved caller link remains distinct from the live link overwritten by subsequent calls.
- Round 32 reads two source rows and writes one output row. Its observed count decreases from eight to seven, so the conditional return is untaken. Each source row uses a preceding byte and a complete word: the final byte is discarded by packing, but the full guarded word read remains necessary.
- Round 33 alternates four passes through the new suffix and three passes through the existing interior tail. It reduces seven to zero without replaying the one-time setup, then reaches the newly observed fallthrough. Round 34 returns through live LR into an existing section.
- Round 35 executes an 18-word prefix once and a 44-word tail eight times. It reads 17 source rows and writes 16 output rows, preserving halfword packing, masked shifts, register-bank alternation and ordered multi-register transfers. The incoming link is saved before later halfword loads reuse live LR for row data.
- Round 36 restores saved registers and PC through ascending stack reads. Earlier successful loads remain effective if a later read faults; stack writeback occurs only after all reads succeed. The saved return word has authenticated write lineage but no independent readback. Call and return reviews predict immediate boundaries only; their downstream memory-dependent execution is outside those forecasts.

Partial reported-state checks do not independently verify guest output memory, rendered pixels or audible content. Unknown loaded values remain excluded. Guarded memory widths, discarded trailing source values, staged shifts, wrapping additions, partial transfer effects and individual writebacks are preserved. No approximate pixel formula, forced return, new instruction policy or runtime decoding was introduced.

## Attempt history and retained text limitations

Five initial build-wrapper invocations failed their **post-archive source-consistency check** after their build and CTest processes had already returned 0. The affected rounds are **30, 32, 34, 35 and 36**. In each case the archive passed full gzip EOF/CRC and contained exactly 12 regular files; the other 11 source identities matched, while the resident CTest text log had become an exact shorter prefix of the archived log. The cause remains unknown.

| Round | Preserved shortened log | Complete archived log | Completed passes in shortened log |
| --- | ---: | ---: | ---: |
| 30 | 10,355 bytes | 10,645 bytes | 88 |
| 32 | 10,355 bytes | 10,645 bytes | 88 |
| 34 | 10,485 bytes | 10,645 bytes | 89 |
| 35 | 10,485 bytes | 10,645 bytes | 89 |
| 36 | 10,355 bytes | 10,645 bytes | 88 |

The complete archived text, complete 90-case JUnit and explicit successful CTest exit agree. Each actual shortened log, original complete archive, helper traceback and exit-1 record was preserved before restoration. The exact full log bytes were recovered from that archive, followed by all 12 source identity checks and another unchanged-archive check. Explicit recovery receipts distinguish those later reconciliations from the initial rejected checks. No native bytes changed, and no build, test or native run was repeated for these recoveries. All eight current CTest text logs now include 90 passing cases and complete summaries.

The round-36 next-review text log is empty. Its bound review and coverage JSON remain complete and authenticated to the original code, generator and actual stdout. The other current review, coverage and process stdout logs are nonempty. Empty expected stderr files are not evidence failures.

There were no failed build or CTest processes and no rejected native launches in this batch. Each accepted native ran once in fresh owned storage. The preceding checkpoint retains the round-27 permission rejection, round-28 truncated backup and verified replacement, short round-23/24 text logs and earlier linked failures as historical evidence. See the [preceding recovery record](https://github.com/GTTeancum/Lego-Undercover-3DS-Recomp/blob/9cd3f9ca88ea4820abf66b6f6f8276c4ba2d4432/docs/recovery/CURRENT-REBUILD.md).

## Reproduce the build

The current batch reused the resident configured Release build. For a fresh environment, use the exact supported original inputs and preserved 599-page AOT archive, keeping generated content and build outputs private:

```sh
cmake -S . -B /absolute/private/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DLEGO_BUILD_DESKTOP=OFF \
  -DLEGO_AOT_DIR=/absolute/private/aot/generated2 \
  -DLEGO_SUPPLEMENT_CODE_BIN=/absolute/private/inputs/code.bin
cmake --build /absolute/private/build --parallel 6
ctest --test-dir /absolute/private/build --output-on-failure --timeout 90
```

The recorded toolchain is CMake 3.28.3, Ninja 1.11.1, GCC 13.3.0, Clang/LLVM 18.1.3 and Python 3.12.14. When tests run as root, their isolated permission-test children must be allowed to drop identity. Use the permitted execution context supporting those unchanged tests; retain explicit completion codes and full JUnit reports.

The private checkpoint retains the exact headless command and diagnostic profiles. Use fresh owned state/save/audio folders, the same authenticated inputs, the 100,000,000 block limit and a 600-second process limit. Check the frozen executable's hash and execute permission immediately before launch. Require the saved, validated archive before running, and recheck its identity afterward. Compare complete audio records, state/save bytes and the actual stop; a reported record count alone is insufficient.

## Visible status and hosted checks

Menus, gameplay, speaker playback and Windows execution remain unverified. No new screen capture, windowed run or capture-on/off comparison was performed. Captured audio is diagnostic data; the four appended records have zero-valued samples. No fresh visible or audible-content milestone is claimed.

Hosted checks are **pending at source-preparation time**. Actual results are recorded separately against the published commit in the private publication receipt. Successful checks for a preceding commit remain historical. Hosted checks are ROM-free and do not execute the original game.

## Next exact task

Actual stop: **MissingBlock `0x001550E8`**. The next reviewed interval is **`[0x001550E8, 0x00155104)`**, seven original words, and **has not been admitted or executed**. It copies each row through two ordered halfword reads, packs a word, stores it, advances both pointers by 512, decrements the count and loops while nonzero. Preserve both halfword accesses and their separate post-index writebacks rather than substituting a word read.

For the observed count four, the saved unexecuted forecast predicts **28 instructions** to MissingBlock `0x00155104`, with 17 predicted or preserved fields and two unknown loaded registers. It expects source `0x1451DF9E`, destination `0x1432D9A0`, count zero and CPSR `0x60000010`. This forecast assumes future admission and successful guarded accesses; it is not a completed result.

If the second halfword read faults, the first loaded value and its pointer increment remain effective. A later store fault preserves the completed reads and source stride advance while preventing destination writeback and the count decrement. Keep these partial effects and normal fault reporting.

Preserve all **182 reviewed sections** and every original page. Reauthenticate the exact words and generator, admit only this observed interval, generate/verify, incrementally build/test, save and validate the exact native archive, then perform one finite isolated run with complete artifact and prediction checks. Use ordinary chat/cloud tools only. Source/metadata publication and GitHub Actions compilation are authorized for this project; keep original inputs, generated instructions, natives and full evidence private, and never force-push.
