# October 31, 2026 - startup continuation

## Current status

Five more startup gaps are filled: an eight-row processing loop, its return, the next setup and table dispatch, and two stack returns. Startup now completes both processing passes and reaches another missing helper. Start at [CHECKPOINT.md](../../CHECKPOINT.md). Menus and gameplay remain unverified.

Base: `3a08db49fb3b1ff58e6b477765f94d8be3ec75dd`. Added 5 observed sections / 26 words. Totals: 86 logical sections / 87 physical blocks / 2047 words; 111,130 runtime blocks / 547,158 verified words. All 599 original pages / 603 files and all 81 preceding sections are preserved. Runtime, generator, tests and build configuration are unchanged. Only observed-range metadata is appended while preserving all previous records.

| Round | Added interval | Words | Actual stop | Instructions beyond preceding run |
| --- | --- | ---: | --- | ---: |
| 01 | `[0x00155008, 0x00155040)` | 14 | `0x00155040` | +98 |
| 02 | `[0x00155040, 0x00155044)` | 1 | `0x0014f510` | +1 |
| 03 | `[0x0014F510, 0x0014F534)` | 9 | `0x0014f534` | +108 |
| 04 | `[0x0014F534, 0x0014F538)` | 1 | `0x0014f5b4` | +1 |
| 05 | `[0x0014F5B4, 0x0014F5B8)` | 1 | `0x00154b94` | +76 |

## Final measured result

MissingBlock `0x00154B94`, thread 7, dispatch round 21846; native exit 3, no timeout and empty stderr. Core-0 instructions: 62,252,048; core-1: 99. CPU ticks: 2,346,433,995; diagnostic guest time: 8,751,698,004 ns; display periods: 523; audio records: 285,834.

Same-mode advance: 284 executed instructions / 1,059 diagnostic ns. These are not a performance benchmark or a gameplay percentage.

## Validation and scope

All five builds passed 89/89 enabled tests once. No failures/errors/skips occurred in these final build gates. Source, original inputs, AOT pages and executables were verified unchanged around build and fresh finite execution. Every prior supplemental operation was preserved. Generated C++ and metadata agree with authenticated original instruction words. Exact executables were saved privately before execution.

Complete audio, state and save contents are byte-for-byte identical to the preceding same-mode checkpoint. This does not establish speaker playback. The final post-stop captures remain completely black and are retained only as private diagnostics. No new screenshot is posted and no fresh windowed run was taken. No visual or gameplay milestone is claimed; speaker playback and Windows execution remain unverified. Passing diagnostic tests is not evidence of playable gameplay.

This publication appends observed-range metadata and updates four status documents. Runtime, generator, tests and build configuration are unchanged. The status documents were updated after execution; all non-status source matches the final tested candidate. Hosted checks run without original game inputs. Their observed status is retained privately rather than predicted here.

## Next reviewed section

`[0x154b94, 0x154c2c)`. Add the reviewed 38-instruction entry section that loads a mask, combines neighboring guest halfword and word values from two rows, writes two output words, advances the row pointers and decrements the row count. Its first control exit is a conditional return through LR. Future memory values, that condition outcome, the return destination and fallthrough at 0x00154C2C remain unobserved; do not include an unobserved successor body. Existing handlers accept this reviewed interval. The private review is tied to the actual final stop. Preserve memory faults, partial effects, flags and target validation. Do not infer stack-loaded returns from live LR or add unobserved successor bodies.

Use fresh owned state/save/output paths and finite execution. Preserve all 86 logical sections (87 physical blocks) and original AOT pages. No forced return, runtime decoding, game-code modification or fabricated GPU completion.

## Working permissions

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Source/metadata publication to `main` and ongoing private builds/backups are authorized. Never force-push. Keep original inputs, generated game code, executables, audio, captures, saves and recovery identifiers private. Keep user summaries short and plain. Do not post completely black screen captures to the user; retain them only as private diagnostic evidence.

Prior records remain in Git history and separately saved private checkpoints. Historical notes remain in [HISTORY-7cf0d2a.md](HISTORY-7cf0d2a.md).
