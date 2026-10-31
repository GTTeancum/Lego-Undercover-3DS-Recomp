# October 31, 2026 - startup continuation

## Current status

Five more startup gaps are filled. A 16-row processing pass and an eight-row helper now finish, and their missing return steps are restored. All five fresh runs passed their preceding stops. Startup reaches another missing call setup; menus and gameplay remain unverified. Start at [CHECKPOINT.md](../../CHECKPOINT.md).

Base: `9fa13bea84afc2adb3cdece78e1cdbfaaf7cdef5`. Added 5 observed sections / 70 words. Totals: 91 logical sections / 92 physical blocks / 2117 words; 111,135 runtime blocks / 547,228 verified words. All 599 original pages / 603 files and all 86 preceding sections are preserved. Runtime, generator, tests and build configuration are unchanged.

| Round | Added interval | Words | Actual stop | Instructions beyond preceding run |
| --- | --- | ---: | --- | ---: |
| 01 | `[0x00154B94, 0x00154C2C)` | 38 | `0x00154C2C` | +38 |
| 02 | `[0x00154C2C, 0x00154C80)` | 21 | `0x00154C80` | +315 |
| 03 | `[0x00154C80, 0x00154C84)` | 1 | `0x00154F6C` | +24 |
| 04 | `[0x00154F6C, 0x00154F90)` | 9 | `0x00154F90` | +65 |
| 05 | `[0x00154F90, 0x00154F94)` | 1 | `0x0014FB20` | +2671 |

## Final measured result

MissingBlock `0x0014FB20`, thread 7, dispatch round 21846; native exit 3, no timeout and empty stderr. Core-0 instructions: 62,255,161; core-1: 99. CPU ticks: 2,346,437,108; diagnostic guest time: 8,751,709,615 ns; display periods: 523; audio records: 285,834.

Same-mode advance: 3,113 executed instructions / 11,611 diagnostic ns. These are not a performance benchmark or a gameplay percentage.

## Validation and scope

All five builds passed 89/89 enabled tests once, with zero failures/errors/skips. Source, original inputs, AOT pages and executables were verified unchanged around each build and fresh finite execution. Every prior supplemental operation was preserved. Generated C++ and metadata agree with authenticated original words. Exact executables were saved privately before execution.

Measured stop records confirm the first routine completed its requested 16 rows with the expected pointer advances and exhausted count. Its return and the existing caller dispatch then reached the next helper. That helper completed all eight requested rows with the one-time source adjustment excluded from its loop. The final return permits another 2,671 instructions of execution before the next missing entry. These checks establish pointer/count and continuation effects, not pixel values or isolated single-step return equivalence.

Complete audio, state and save contents are byte-for-byte identical to the preceding same-mode checkpoint. This does not establish speaker playback. The final post-stop captures remain completely black and are retained only as private diagnostics. No screenshot is posted and no fresh windowed run was taken. There is no new visual or gameplay milestone. Speaker playback and Windows execution remain unverified. The existing post-stop snapshot option was used only on the final run; no new capture-on/off paired comparison was performed.

This publication appends observed-range metadata and updates four status documents after execution. All non-status source matches the final tested candidate. The previous checkpoint's release compiler, sanitizer and packaging jobs have now been observed passing. Fresh hosted status for this publication is retained separately; hosted checks run without original game inputs.

One initial host configuration attempt exceeded a short wrapper timeout before the builds; the subsequent configuration completed. That development receipt is retained. None of the five game runs timed out, and none of their full test gates was retried.

## Next reviewed section

`[0x0014FB20, 0x0014FB30)`. Add the reviewed four-instruction entry that saves LR on the guest stack, sets two working registers and calls the already recorded routine at 0x0014F190. Its stack write and future execution remain unobserved. The assigned return at 0x0014FB30 is still missing; do not include that continuation or invent a return target. Existing handlers accept this interval. The private review is tied to the actual final stop and confirms no overlap with existing recorded sections. Preserve memory faults, partial effects, flags and normal target validation.

Use fresh owned state/save/output paths and finite execution. Preserve all 91 logical sections (92 physical blocks) and original AOT pages. No forced return, runtime decoding, game-code modification or fabricated GPU completion.

## Working permissions

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Source/metadata publication to `main` and ongoing private builds/backups are authorized. Never force-push. Keep original inputs, generated game code, executables, audio, captures, saves and recovery identifiers private. Keep user summaries short and plain. Do not post completely black screen captures to the user; retain them only as private diagnostic evidence.

Prior records remain in Git history and separately saved private checkpoints. Historical notes remain in [HISTORY-7cf0d2a.md](HISTORY-7cf0d2a.md).
