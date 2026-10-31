# October 31, 2026 - startup continuation

## Current status

Five more startup gaps are filled: a copying loop, its return, a missing call, a stack return and the next call setup. The program can also save both current screen images after execution stops, without SDL. Start at [CHECKPOINT.md](../../CHECKPOINT.md). Menus and gameplay remain unverified.

Base: `abf6cc6ff583f56d310ffe1676970082a4cf232d`. Added 5 observed sections / 13 words. Totals: 74 logical sections / 75 physical blocks / 1919 words; 111,118 runtime blocks / 547,030 verified words. All 599 original pages / 603 files and all 69 preceding sections are preserved. The host gains opt-in --scanout-snapshot output, a bounded raw-image writer and authored tests with CMake wiring. Runtime evaluators, scheduler, renderer, scanout decoder and build-time generator are unchanged. The capture is performed only after execution stops; it issues no guest event, input or render command.

| Round | Added interval | Words | Actual stop | Instructions beyond preceding run |
| --- | --- | ---: | --- | ---: |
| 01 | `[0x001546B8, 0x001546D0)` | 6 | `0x001546d0` | +48 |
| 02 | `[0x001546D0, 0x001546D4)` | 1 | `0x0014f318` | +60 |
| 03 | `[0x0014F318, 0x0014F31C)` | 1 | `0x0014f31c` | +45 |
| 04 | `[0x0014F31C, 0x0014F320)` | 1 | `0x0014f5a4` | +1087 |
| 05 | `[0x0014F5A4, 0x0014F5B4)` | 4 | `0x0014f444` | +4 |

## Final measured result

MissingBlock `0x0014F444`, thread 7, dispatch round 21846; native exit 3, no timeout and empty stderr. Core-0 instructions: 62,251,376; core-1: 99. CPU ticks: 2,346,433,323; diagnostic guest time: 8,751,695,498 ns; display periods: 523; audio records: 285,834.

Same-mode advance: 1,244 executed instructions / 4,640 diagnostic ns. These are not a performance benchmark or a gameplay percentage.

## Validation and scope

The first four builds each passed 88/88 enabled tests once; the final build passed 89/89 once, including the new snapshot suite. No failures/errors/skips occurred in these final build gates. Source, original inputs, AOT pages and executables were verified unchanged around build and fresh finite execution. Every prior supplemental operation was preserved. Generated C++ and metadata agree with authenticated original instruction words. Exact executables were saved privately before execution.

Complete state and save contents match the preceding same-mode checkpoint. The entire prior audio capture is preserved byte-for-byte, with four valid silent diagnostic records appended; this is not evidence of speaker playback. A fresh headless post-stop capture shows both screens completely black: 400x240 top and 320x240 bottom. This is the current stopped scanout, not a windowed screenshot or the earlier first-nonblack loading capture. Those different capture points are not a regression comparison. No fresh windowed run was possible because the pinned SDL3 download failed. Menus, gameplay, speaker playback and Windows execution remain unverified. Passing diagnostic tests is not evidence of playable gameplay.

This publication adds post-stop snapshot diagnostics and authored tests alongside the observed-range metadata and four status documents. The status documents were updated after execution; all non-status source matches the final tested candidate. Hosted checks run without original game inputs. Their observed status is retained privately rather than predicted here.

A deliberate second fresh run of the exact final native with capture disabled produced identical stop, counters, normalized original diagnostics, complete audio, state and save contents. Both capture-on and capture-off runs ended at MissingBlock with native exit 3, no timeout and empty stderr. This is a paired diagnostic comparison, not an automatic retry.

The snapshot and existing display-decoder suites passed a focused Clang address/undefined-behavior sanitizer run. Six command-line validation cases passed. Snapshot tests check exact RGB bytes, immutable input, black frames, invalid image dimensions and alpha, exclusive paths, and retained write/flush/close failures.

## Next reviewed section

`[0x14f444, 0x14f44c)`. Add the reviewed two-instruction entry that saves six registers on the guest stack and calls the already recorded routine at 0x0015117C. Its later continuation at 0x0014F44C remains unobserved; do not include that successor or invent a return target. Existing handlers accept this reviewed interval. The private review is tied to the actual final stop. Preserve memory faults, partial effects, flags and target validation. Do not infer stack-loaded returns from live LR or add unobserved successor bodies.

Use fresh owned state/save/output paths and finite execution. Preserve all 74 logical sections (75 physical blocks) and original AOT pages. No forced return, runtime decoding, game-code modification or fabricated GPU completion.

## Working permissions

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Source/metadata publication to `main` and ongoing private builds/backups are authorized. Never force-push. Keep original inputs, generated game code, executables, audio, captures, saves and recovery identifiers private. Keep user summaries short and plain.

Prior records remain in Git history and separately saved private checkpoints. Historical notes remain in [HISTORY-7cf0d2a.md](HISTORY-7cf0d2a.md).
