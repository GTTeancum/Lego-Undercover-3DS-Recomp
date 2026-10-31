# October 31, 2026 - observed startup continuation

## Status

Six more startup gaps are filled. Three processing loops now finish their requested rows, and all three missing return steps are restored. All six fresh game runs passed their preceding stopping points. Startup reaches another missing setup step; menus and gameplay remain unverified.

Runtime, generator, tests and build configuration are unchanged. All 117 previous reviewed sections and all 599 original AOT pages / 603 files are preserved. Only observed-range metadata and four status documents change; the documents were updated after execution.

Totals: **123 reviewed sections / 124 page-local blocks / 2632 supplemental words**; **111,167 runtime blocks / 547,743 verified words**.

## Measured runs

| Round | Actual headless stop | Additional executed instructions | Tests |
| --- | --- | ---: | ---: |
| 01 | `0x001546b0` | +239 | 89/89 |
| 02 | `0x00154d50` | +24 | 89/89 |
| 03 | `0x00154d90` | +58 | 89/89 |
| 04 | `0x001552a8` | +2,235 | 89/89 |
| 05 | `0x001552d4` | +74 | 89/89 |
| 06 | `0x0014f58c` | +102 | 89/89 |

Round 01 compares with the saved same-mode `cf4ec78` run. Later rounds compare with the immediately preceding fresh run. No earlier run is counted as new work.

## Validation and scope

All 6 builds passed **89/89 enabled tests once**, with zero failures, errors or skips. Each exact tested executable was saved privately before its fresh finite run. No run timed out or wrote stderr. Complete audio, state and save contents match the previous same-mode checkpoint byte-for-byte. These are startup checks, not gameplay or speaker playback.

The final run executes 62,301,076 core-0 instructions and 99 core-1 instructions. Diagnostic time is 8,751,880,868 ns. The measured advance from `cf4ec78` is **2,732 executed instructions**, not a gameplay percentage.

Measured pointer, row-count and instruction-count checks confirm that the three restored loops finish eight, four and eight output rows, respectively, without repeating their one-time setup. Checked unrelated registers, floating-point status and thread-local pointer are preserved; final arithmetic flags match completion. These checks do not verify output words or pixel values. Each restored return allowed execution to reach the next actual stop through existing code; isolated single-step return equivalence was not captured.

Source identities, original inputs, AOT pages, prior supplemental operations and tested-native identities were checked before and after execution. Four status documents were updated after the final run; all other published source bytes match the exact final tested candidate. Full build logs, one full test report per build and fresh run outputs are retained privately.

The final post-stop screen captures remain completely black and are retained privately. No fresh windowed run was performed and no visible milestone is claimed. Menus, gameplay, speaker playback and Windows execution remain unverified. The existing post-stop capture option was used on the final run; no new paired capture-on/off run was performed. Hosted synthetic checks do not run the original game and are tracked separately from the local tests and game-run evidence.

## Next exact task

Next interval: **`[0x14f58c, 0x14f5a4)`**. Add the reviewed six-instruction setup section. It initializes two working registers, saves six registers on the guest stack, loads two guest-state words and branches to the already recorded interior suffix at 0x0014F464. Existing handlers accept the interval. Preserve guarded memory accesses, partial effects, flags and normal target validation. Future memory values and execution beyond this section remain unobserved; do not add an unobserved successor body or invent a return target.

## Working permissions

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Source/metadata publication to main and ongoing private builds/backups are authorized. Never force-push. Keep original inputs, generated code, executables, captures, audio, saves and recovery identifiers private. Keep summaries short and plain. Do not post completely black screenshots.
