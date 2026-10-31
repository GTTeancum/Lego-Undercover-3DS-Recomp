# October 31, 2026 - startup continuation

## Current status

Six more startup gaps are filled. Two eight-row processing passes now finish, and their missing call and return links are restored. Startup reaches the next unfinished helper. All 6 fresh runs passed their preceding stops. Menus and gameplay remain unverified. Speaker playback and Windows execution also remain unverified. Start at [CHECKPOINT.md](../../CHECKPOINT.md).

Base: `1f1df7bd136ab9126a6ed8a7656321a927f4a69a`. Added 6 reviewed sections / 126 words. Totals: 97 logical sections / 98 physical blocks / 2243 words; 111,141 runtime blocks / 547,354 verified words. All 599 original pages / 603 files and all 91 preceding sections are preserved. Runtime, generator, tests and build configuration are unchanged.

| Round | Added interval | Words | Actual stop | Instructions beyond preceding run |
| --- | --- | ---: | --- | ---: |
| 01 | `[0x0014FB20, 0x0014FB30)` | 4 | `0x00153bcc` | +60 |
| 02 | `[0x00153BCC, 0x00153C20)` | 21 | `0x00153c20` | +161 |
| 03 | `[0x00153C20, 0x00153C24)` | 1 | `0x0014fb30` | +84 |
| 04 | `[0x0014FB30, 0x0014FB34)` | 1 | `0x00154264` | +82 |
| 05 | `[0x00154264, 0x001543EC)` | 98 | `0x001543ec` | +302 |
| 06 | `[0x001543EC, 0x001543F0)` | 1 | `0x00154844` | +24 |

## Final measured result

MissingBlock `0x00154844`, thread 7, dispatch round 21846; native exit 3, no timeout and empty stderr. Core-0 instructions: 62,255,874; core-1: 99. CPU ticks: 2,346,437,821; diagnostic guest time: 8,751,712,275 ns; display periods: 523; audio records: 285,834.

Same-mode advance: 713 executed instructions / 2,660 diagnostic ns. These are not a performance measurement or a gameplay percentage.

## Validation and scope

All 6 builds passed **89/89 enabled tests once**, with zero failures, errors or skips. Each exact tested executable was saved privately before its fresh finite run. Every run ended at missing code with native exit 3, no timeout and empty stderr. These checks do not establish playable gameplay.

Source, original inputs, AOT pages and natives were checked unchanged around compilation and execution. Every earlier supplemental record and generated operation is preserved. Generated C++ and receipt metadata agree with the authenticated original words. Only observed-range metadata and four status documents change; the status documents were updated after all runs.

Measured stop records verify the row-processing loop completes all eight requested rows, advances both pointers as expected, exhausts the count and does not replay its one-time mask setup. Checked unrelated registers and floating-point status are preserved; the final arithmetic flags match loop completion. A second paired-row helper also completes all eight rows with the expected source/destination advances, exhausted count and stack position; its one-time prefix is not replayed. These are checks on measured evidence, not additional CTest suites or validation of pixel values.

Complete audio, state and save contents match the preceding checkpoint byte-for-byte. This does not establish speaker playback. The final post-stop captures are completely black and remain private diagnostics. No fresh windowed run was taken and no visual milestone is claimed. The existing post-stop snapshot option was used on the final run. No new capture-on/off paired comparison was performed; the earlier paired evidence remains in preceding checkpoints.

The preceding checkpoint's release compiler, sanitizer and packaging checks have now been observed passing. Fresh hosted status for this publication is recorded separately; hosted checks do not execute the original game.

## Next reviewed section

`[0x00154844, 0x00154874)`, 12 original instruction words. Add the reviewed 12-instruction row-copying section. It adjusts the source pointer once, combines two guest words and an adjacent byte into two output words, advances the row pointers, decrements the count and conditionally repeats at the interior suffix 0x00154848. Future memory values, loop completion and fallthrough at 0x00154874 remain unobserved; do not include that successor. Existing handlers accept the interval. Preserve all 97 reviewed sections (98 page-local blocks) and the original 599 AOT pages. Do not invent a return target or include unobserved successor bodies.

Preserve guarded reads, partial writes, flags, stack effects and normal execution-target validation. Use fresh owned state/save/output paths and a finite run. No runtime decoder, forced return, altered game instructions or fabricated GPU completion.

## Working permissions

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Source/metadata publication to `main` and ongoing private builds/backups are authorized. Never force-push. Keep original inputs, generated game code, executables, audio, captures, saves and recovery identifiers private. Keep summaries short and plain. Do not post completely black screen captures.

Prior records remain in Git history and separately saved private checkpoints. Historical notes remain in [HISTORY-7cf0d2a.md](HISTORY-7cf0d2a.md).
