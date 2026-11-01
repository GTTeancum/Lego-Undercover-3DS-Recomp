# October 31, 2026 — six observed startup continuations

Read [CHECKPOINT.md](../../CHECKPOINT.md) first. Base: `f18fb670b8a7c1c6fbd05df4c901c8ccd4c69468`. Goal: a full playable recompilation; menus and gameplay are still unverified.

## What changed

Six more missing startup sections now run, including the unfinished caller step recovered from the preceding session. Startup proceeds through a table lookup, bit processing, its table dispatch, a processing path that returns through existing code, and the following call. It now reaches another missing setup entry.

The runtime, generator, tests and build configuration are unchanged. All 146 preceding reviewed sections and all 599 original AOT pages / 603 files are preserved. Generated operations authenticate against the exact supported original input. The only public implementation change is six appended observed ranges in `config/observed_aot_supplements.json`. Four status documents were updated after the runs. Every other source byte matches the final tested candidate.

The preceding session had saved, but not published, the first caller candidate. Its executable and generated-source hashes, all 2,853 input words, historical 90-test report and 532 nonmanifest source files were checked before reuse. Its sole additional section was the already reviewed caller step. The fresh source build regenerated the published baseline's supplemental C++ byte-for-byte before the additions.

## Accepted fresh runs

| Round | Added interval, exclusive end | Actual MissingBlock stop | Extra executed instructions |
| --- | --- | --- | ---: |
| 01 (recovered) | `[0x0014FBA4, 0x0014FBAC)` | `0x0014fed8` | +15 |
| 02 | `[0x0014FED8, 0x0014FEE0)` | `0x0015003c` | +2 |
| 03 | `[0x0015003C, 0x00150060)` | `0x00150060` | +9 |
| 04 | `[0x00150060, 0x00150070)` | `0x0014ff90` | +4 |
| 05 | `[0x0014FF90, 0x0014FFA8)` | `0x0014fee0` | +102 |
| 06 | `[0x0014FEE0, 0x0014FEE8)` | `0x0014ffa8` | +15 |

Each interval followed a recorded missing-code stop. No branch, return or completion was forced. Total advance: 147 core-0 instructions and 548 diagnostic ns.

Five newly built headless Release candidates each passed **90/90 enabled tests**, with zero failures, errors or skips. The recovered candidate retains its verified historical 90-test report and was executed again in this session. All six accepted fresh startup runs ended at missing original code with native exit 3, no timeout and empty stderr. Every accepted capture contains all 285,842 reported records (4,573,504 bytes). Complete audio, state and saves are byte-for-byte identical to the baseline.

The final executable reports 111,196 runtime blocks and 547,987 verified instruction words. All 603 original AOT files also match the preceding private checkpoint's inventory. Original inputs and frozen executables were verified before and after the accepted runs. Source snapshots were checked across each new build and test run.

## Meaningful semantic checks

- Independent scalar table reads, shifting and flag calculations predicted the nine-instruction processing step's complete reported register state and untaken conditional call. The actual run matched, advancing nine instructions.
- An independent relative-offset calculation predicted the next four-instruction dispatch destination. The actual run matched every reported register and advanced four instructions.
- The selected processing entry preserves a six-register stack context and enters an existing block four instructions after its beginning. Review confirmed suffix execution without replaying its prefix, and that the two saved dimensions fit the existing stack accesses. Its fresh run returned to the caller after 102 additional instructions. No claim about rendered pixels is derived from this control-flow result.
- The last caller step preserves flags and the saved guest-stack link while entering the now-recorded routine again. Its fresh run advances 15 instructions to the next actual missing entry.

## Rejected attempts and environment repair

Initial local verification exposed incomplete build outputs and two filesystem tests that could not drop an isolated child's privileges inside the restricted process sandbox. The affected outputs were regenerated, and the full suite passed in the permitted cloud execution environment. The first recovered-caller run retained a truncated audio prefix despite reporting a complete write. That capture was rejected and preserved; the same frozen native was rerun in fresh isolated storage and produced a complete, byte-identical capture. Subsequent accepted runs validate actual file length and record count as well as checksums. The root cause of the incomplete outputs was not established; no runtime change was made to conceal it.

The fresh baseline suite and all five newly built candidates passed 90/90 after environment repair. Historical tests, rejected runs and accepted new runs are kept distinct. No new source tests, instruction-policy expansion or runtime workaround were introduced in this batch.

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

When the tests run as root, their isolated permission-test children must be allowed to drop identity; the restricted cloud process sandbox did not provide that capability. Use a suitable permitted execution environment without weakening the tests.

The private checkpoint retains the exact headless command and diagnostic profiles. Use fresh owned state/save/audio folders, the same original inputs, the 100,000,000 block limit and a 600-second process limit. Compare actual audio records, complete state/save bytes and the actual stop; a reported record count alone is insufficient.

## Visible status and hosted checks

Menus, gameplay, speaker playback and Windows execution remain unverified. No new screen capture or windowed run was performed. Complete captured audio is diagnostic data, not speaker playback. No new visual milestone or capture-on/off comparison is claimed.

The baseline GitHub compiler, sanitizer and packaging checks passed. Current publication and hosted-check results are recorded in the private publication receipt; do not infer a completed current run from the baseline. Hosted checks are ROM-free and do not execute the original game.

## Next exact task

Actual stop: **MissingBlock `0x0014FFA8`**. The next reviewed interval is **`[0x0014FFA8, 0x0014FFB8)`**, four original words. It saves LR on the guest stack, sets both working dimensions to four, and calls the already recorded routine at `0x0014F444`. Existing handlers accept the interval. Preserve guarded stack-write ordering, flags, the link assignment and ordinary target validation. The assigned continuation at `0x0014FFB8` remains unobserved and must not be included.

Preserve all 152 reviewed sections and every original page. Use ordinary chat/cloud tools only. Publishing source/metadata and compiling through GitHub Actions are authorized for this project; keep original inputs, generated instructions, natives and full evidence private, and never force-push.
