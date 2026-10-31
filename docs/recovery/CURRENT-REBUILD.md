# October 30, 2026 - startup continuation

## Current status

Five more missing startup sections now run: two byte-processing helpers, an averaging helper, a row-copying loop and its return step. Together they let startup move through a substantially longer stretch of its existing code. Start at [CHECKPOINT.md](../../CHECKPOINT.md). Menus and gameplay remain unverified.

Base: `aa82c6547ff88a6a18a80d5b461a161bc85ab2fa`. Added 5 observed sections / 301 words. Totals: 69 logical sections / 70 physical blocks / 1906 words; 111,113 runtime blocks / 547,017 verified words. All 599 original pages / 603 files and all 64 preceding sections are preserved. Runtime, generator, tests and build configuration are unchanged.

| Round | Added interval | Words | Actual stop | Instructions beyond preceding run |
| --- | --- | ---: | --- | ---: |
| 01 | `[0x001524F4, 0x00152834)` | 208 | `0x00152eb4` | +5597 |
| 02 | `[0x00152EB4, 0x00152FC0)` | 67 | `0x00152a7c` | +347293 |
| 03 | `[0x00152A7C, 0x00152AC8)` | 19 | `0x00153b20` | +434849 |
| 04 | `[0x00153B20, 0x00153B38)` | 6 | `0x00153b38` | +96 |
| 05 | `[0x00153B38, 0x00153B3C)` | 1 | `0x001546b8` | +24 |

## Final measured result

MissingBlock `0x001546B8`, thread 7, dispatch round 21846; native exit 3, no timeout and empty stderr. Core-0 instructions: 62,250,132; core-1: 99. CPU ticks: 2,346,432,079; diagnostic guest time: 8,751,690,858 ns; display periods: 523; audio records: 285,830.

Same-mode advance: 787,859 executed instructions / 2,938,546 diagnostic ns. These are not a performance benchmark or a gameplay percentage.

## Validation and scope

Each exact new native passed 88/88 enabled tests once, with no failures/errors/skips. Source, original inputs, AOT pages and executables were verified unchanged around build and fresh finite execution. Every prior supplemental operation was preserved. Generated C++ and metadata agree with authenticated original instruction words. Exact executables were saved privately before execution.

Complete state and save contents match the preceding same-mode checkpoint. The entire prior audio capture is preserved byte-for-byte, with 96 valid silent diagnostic records appended; this is not evidence of speaker playback. These were headless runs. No fresh game-window run or screenshot was taken. The earlier loading image remains the last verified visible result. Speaker playback and Windows execution remain unverified. Passing diagnostic tests is not evidence of playable gameplay.

This publication changes observed-range metadata and four status documents. The status documents were updated after execution; all non-status source matches the final tested candidate. Hosted checks run without original game inputs. Their observed status is retained privately rather than predicted here.

## Next reviewed section

`[0x1546b8, 0x1546d0)`. Add the reviewed six-instruction loop that copies two guest words per row, advances both pointers, decrements its row count and conditionally repeats. Future memory values, loop completion and the fallthrough beyond this section remain unobserved; do not add the successor body. Existing handlers accept this reviewed interval. The private review is tied to the actual final stop. Preserve memory faults, partial effects, flags and target validation. Do not infer stack-loaded returns from live LR or add unobserved successor bodies.

Use fresh owned state/save/output paths and finite execution. Preserve all 69 logical sections (70 physical blocks) and original AOT pages. No forced return, runtime decoding, game-code modification or fabricated GPU completion.

## Working permissions

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Source/metadata publication to `main` and ongoing private builds/backups are authorized. Never force-push. Keep original inputs, generated game code, executables, audio, captures, saves and recovery identifiers private. Keep user summaries short and plain.

Prior records remain in Git history and separately saved private checkpoints. Historical notes remain in [HISTORY-7cf0d2a.md](HISTORY-7cf0d2a.md).
