# October 31, 2026 - startup continuation

## Current status

Five more startup gaps are filled. A 16-row copying loop and an eight-row neighboring-data helper now complete, and their missing return steps are restored. Startup reaches the next unfinished helper. All 5 fresh runs passed their preceding stops. Menus and gameplay remain unverified. Speaker playback and Windows execution also remain unverified. Start at [CHECKPOINT.md](../../CHECKPOINT.md).

Base: `2a3fd034706db1ad5e28d584a6c0a21c6a19a350`. Added 5 reviewed sections / 53 words. Totals: 107 logical sections / 108 physical blocks / 2348 words; 111,151 runtime blocks / 547,459 verified words. Original 599 pages / 603 files and all 102 prior sections are preserved. Runtime, generator, tests and build settings are unchanged.

| Round | Added interval | Words | Actual stop | Instructions beyond preceding run |
| --- | --- | ---: | --- | ---: |
| 01 | `[0x00154C84, 0x00154CB0)` | 11 | `0x00154cb0` | +161 |
| 02 | `[0x00154CB0, 0x00154CB4)` | 1 | `0x00155044` | +24 |
| 03 | `[0x00155044, 0x001550AC)` | 26 | `0x001550ac` | +26 |
| 04 | `[0x001550AC, 0x001550E4)` | 14 | `0x001550e4` | +98 |
| 05 | `[0x001550E4, 0x001550E8)` | 1 | `0x00153d9c` | +3,353 |

## Final measured result

MissingBlock `0x00153d9c`, thread 7, dispatch round 21846; native exit 3, no timeout and empty stderr. Core-0 instructions: 62,270,661; core-1: 99. CPU ticks: 2,346,452,608; diagnostic guest time: 8,751,767,427 ns; display periods: 523; audio records: 285,834.

Same-mode advance: 3,662 executed instructions / 13,658 diagnostic ns. These are not gameplay percentages or a performance measurement.

## Validation and scope

All 5 builds passed **89/89 enabled tests once**, with zero failures/errors/skips. Exact tested executables were preserved privately before their fresh runs. Every run ended at missing code without timeout or stderr output.

Every original input, AOT page, prior manifest record and generated operation is preserved. Generated C++ and receipts were checked against authenticated original words. Source, inputs and executable identities were checked around builds and execution. Only observed-range metadata and four status documents change; status documents were updated after the runs.

Measured pointer/count and instruction-count checks confirm the first restored copying loop completed all sixteen requested rows and the neighboring-row helper completed all eight output rows without repeating their one-time prefixes. Checked unrelated reported registers, floating-point status and thread-local pointer were preserved; final arithmetic flags match completion. These checks do not validate output words or pixels. Further conclusions are limited to the actual recorded stops.

Complete audio, state and saves match the previous same-mode checkpoint byte-for-byte. This does not establish speaker playback. The final post-stop captures remain completely black and are retained privately. No fresh windowed run was performed and no visual milestone is claimed. The existing post-stop snapshot option was used on the final run. No new capture-on/off paired comparison was performed; earlier paired evidence remains in preceding checkpoints.

The previous checkpoint's compiler, sanitizer and packaging jobs were observed passing. Fresh hosted status is recorded separately; hosted checks do not execute the original game.

## Next reviewed section

`[0x00153d9c, 0x00153e94)`. Add the reviewed 62-instruction section. It saves three registers, adjusts the source pointer once, loads a mask, combines neighboring guest rows into four-word outputs, advances row pointers and decrements the row count by two. Its first control exit conditionally repeats at the interior suffix 0x00153DE4, excluding the one-time prefix. Existing handlers accept the interval. Future memory values, loop completion and fallthrough at 0x00153E94 remain unobserved; do not add that successor or invent a return target.

Preserve all 107 sections and ordinary memory faults, partial effects, flags, stack behavior and target validation. Use fresh owned state/save/output paths and a finite run. No runtime decoder, forced return, altered game instructions or fabricated GPU completion.

## Working permissions

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Source/metadata publication to `main` and ongoing private builds/backups are authorized. Never force-push. Original inputs, generated game code, executables, audio, captures, saves and private recovery identifiers stay private. Keep summaries short and plain. Do not post completely black screenshots.

Prior records remain in Git history and separately saved private checkpoints. Historical notes remain in [HISTORY-7cf0d2a.md](HISTORY-7cf0d2a.md).
