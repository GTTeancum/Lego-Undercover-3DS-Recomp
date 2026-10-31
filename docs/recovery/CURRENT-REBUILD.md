# October 31, 2026 - startup continuation

## Current status

Seven more startup gaps are filled: two calls, their setup and dispatch, a complete 16-row processing helper, its return and the next caller dispatch. Fresh execution now reaches the next missing helper. Start at [CHECKPOINT.md](../../CHECKPOINT.md). Menus and gameplay remain unverified.

Base: `0755bdeae9ebf0436cf92e51f5d9e212c938ac20`. Added 7 observed sections / 102 words. Totals: 81 logical sections / 82 physical blocks / 2021 words; 111,125 runtime blocks / 547,132 verified words. All 599 original pages / 603 files and all 74 preceding sections are preserved. Runtime, generator, tests and build configuration are unchanged. Only observed-range metadata is appended while preserving all previous records.

| Round | Added interval | Words | Actual stop | Instructions beyond preceding run |
| --- | --- | ---: | --- | ---: |
| 01 | `[0x0014F444, 0x0014F44C)` | 2 | `0x0014f44c` | +16 |
| 02 | `[0x0014F44C, 0x0014F454)` | 2 | `0x0014f454` | +22 |
| 03 | `[0x0014F454, 0x0014F4B4)` | 24 | `0x00154d94` | +24 |
| 04 | `[0x00154D94, 0x00154E14)` | 32 | `0x00154e14` | +32 |
| 05 | `[0x00154E14, 0x00154E5C)` | 18 | `0x00154e5c` | +270 |
| 06 | `[0x00154E5C, 0x00154E60)` | 1 | `0x0014f4b4` | +1 |
| 07 | `[0x0014F4B4, 0x0014F510)` | 23 | `0x00155008` | +23 |

## Final measured result

MissingBlock `0x00155008`, thread 7, dispatch round 21846; native exit 3, no timeout and empty stderr. Core-0 instructions: 62,251,764; core-1: 99. CPU ticks: 2,346,433,711; diagnostic guest time: 8,751,696,945 ns; display periods: 523; audio records: 285,834.

Same-mode advance: 388 executed instructions / 1,447 diagnostic ns. These are not a performance benchmark or a gameplay percentage.

## Validation and scope

All seven builds passed 89/89 enabled tests once. No failures/errors/skips occurred in these final build gates. Source, original inputs, AOT pages and executables were verified unchanged around build and fresh finite execution. Every prior supplemental operation was preserved. Generated C++ and metadata agree with authenticated original instruction words. Exact executables were saved privately before execution.

Complete audio, state and save contents are byte-for-byte identical to the preceding same-mode checkpoint. This does not establish speaker playback. The final post-stop captures remain completely black and are retained only as private diagnostics. No new screenshot is posted and no fresh windowed run was taken. No visual or gameplay milestone is claimed; speaker playback and Windows execution remain unverified. Passing diagnostic tests is not evidence of playable gameplay.

This publication appends observed-range metadata and updates four status documents. Runtime, generator, tests and build configuration are unchanged. The status documents were updated after execution; all non-status source matches the final tested candidate. Hosted checks run without original game inputs. Their observed status is retained privately rather than predicted here.

## Next reviewed section

`[0x155008, 0x155040)`. Add the reviewed 14-instruction helper section that loads a mask, combines neighboring guest word and halfword values into one output row word, advances pointers, decrements the row count and conditionally loops to an interior suffix at 0x00155010. The actual future memory values, loop completion and fallthrough at 0x00155040 remain unobserved. Do not include that successor body or invent a return target. Existing handlers accept this reviewed interval. The private review is tied to the actual final stop. Preserve memory faults, partial effects, flags and target validation. Do not infer stack-loaded returns from live LR or add unobserved successor bodies.

Use fresh owned state/save/output paths and finite execution. Preserve all 81 logical sections (82 physical blocks) and original AOT pages. No forced return, runtime decoding, game-code modification or fabricated GPU completion.

## Working permissions

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Source/metadata publication to `main` and ongoing private builds/backups are authorized. Never force-push. Keep original inputs, generated game code, executables, audio, captures, saves and recovery identifiers private. Keep user summaries short and plain. Do not post completely black screen captures to the user; retain them only as private diagnostic evidence.

Prior records remain in Git history and separately saved private checkpoints. Historical notes remain in [HISTORY-7cf0d2a.md](HISTORY-7cf0d2a.md).
