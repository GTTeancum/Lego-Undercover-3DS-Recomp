# October 31, 2026 - startup continuation

## Current status

Five more startup gaps are filled. A four-row copying loop and a 16-row neighboring-data pass now complete, and their missing return steps are restored. Startup reaches the next unfinished helper. All 5 fresh runs passed their preceding stops. Menus and gameplay remain unverified. Speaker playback and Windows execution also remain unverified. Start at [CHECKPOINT.md](../../CHECKPOINT.md).

Base: `8f1d469f3f5aab7e28b3219717f6af227cfad22e`. Added 5 reviewed sections / 52 words. Totals: 102 logical sections / 103 physical blocks / 2295 words; 111,146 runtime blocks / 547,406 verified words. Original 599 pages / 603 files and all 97 prior sections are preserved. Runtime, generator, tests and build settings are unchanged.

| Round | Added interval | Words | Actual stop | Instructions beyond preceding run |
| --- | --- | ---: | --- | ---: |
| 01 | `[0x00154844, 0x00154874)` | 12 | `0x00154874` | +45 |
| 02 | `[0x00154874, 0x00154878)` | 1 | `0x00154cb4` | +10,532 |
| 03 | `[0x00154CB4, 0x00154D14)` | 24 | `0x00154d14` | +24 |
| 04 | `[0x00154D14, 0x00154D4C)` | 14 | `0x00154d4c` | +210 |
| 05 | `[0x00154D4C, 0x00154D50)` | 1 | `0x00154c84` | +314 |

## Final measured result

MissingBlock `0x00154c84`, thread 7, dispatch round 21846; native exit 3, no timeout and empty stderr. Core-0 instructions: 62,266,999; core-1: 99. CPU ticks: 2,346,448,946; diagnostic guest time: 8,751,753,769 ns; display periods: 523; audio records: 285,834.

Same-mode advance: 11,125 executed instructions / 41,494 diagnostic ns. These are not gameplay percentages or a performance measurement.

## Validation and scope

All 5 builds passed **89/89 enabled tests once**, with zero failures/errors/skips. Exact tested executables were preserved privately before their fresh runs. Every run ended at missing code without timeout or stderr output.

Every original input, AOT page, prior manifest record and generated operation is preserved. Generated C++ and receipts were checked against authenticated original words. Source, inputs and executable identities were checked around builds and execution. Only observed-range metadata and four status documents change; status documents were updated after the runs.

Measured pointer/count and instruction-count checks confirm the first restored loop completed all four requested rows and the neighboring-row pass completed all sixteen output rows. Their one-time prefixes were not replayed. Checked unrelated reported registers, floating-point status and thread-local pointer were preserved; final arithmetic flags match completion. These checks do not validate output words or pixels. Further conclusions are limited to the actual recorded stops.

Complete audio, state and saves match the previous same-mode checkpoint byte-for-byte. This does not establish speaker playback. The final post-stop captures remain completely black and are retained privately. No fresh windowed run was performed and no visual milestone is claimed. The existing post-stop snapshot option was used on the final run. No new capture-on/off paired comparison was performed; earlier paired evidence remains in preceding checkpoints.

The previous checkpoint's compiler, sanitizer and packaging jobs were observed passing. Fresh hosted status is recorded separately; hosted checks do not execute the original game.

## Next reviewed section

`[0x00154c84, 0x00154cb0)`. Add the reviewed 11-instruction row-copying section. It adjusts the source pointer once, loads a preceding byte and two guest words, combines shifted byte lanes into two output words, advances both row pointers, decrements the count and conditionally repeats at interior suffix 0x00154C88. Existing handlers accept the interval. Future memory values, loop completion and fallthrough at 0x00154CB0 remain unobserved; do not include that successor or invent a return target.

Preserve all 102 sections and ordinary memory faults, partial effects, flags, stack behavior and target validation. Use fresh owned state/save/output paths and a finite run. No runtime decoder, forced return, altered game instructions or fabricated GPU completion.

## Working permissions

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Source/metadata publication to `main` and ongoing private builds/backups are authorized. Never force-push. Original inputs, generated game code, executables, audio, captures, saves and private recovery identifiers stay private. Keep summaries short and plain. Do not post completely black screenshots.

Prior records remain in Git history and separately saved private checkpoints. Historical notes remain in [HISTORY-7cf0d2a.md](HISTORY-7cf0d2a.md).
