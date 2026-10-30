# October 30, 2026 — startup helper continuation

## What changed

5 more missing startup sections are filled. Each fresh game run passed its previous stopping point. Startup now reaches another unfinished section. Menus and gameplay remain unverified.

Starting from `122580d`, 5 bounded sections were added only after actual missing-code stops. This batch changes the supplement manifest and four status documents. Runtime, generator, tests and build configuration are unchanged.

All 599 original AOT pages / 603 files and all 39 earlier supplemental records remain unchanged. Current totals are **44 supplements / 661 words**, **111,087 runtime blocks / 545,772 verified instruction words**. Every generated operation was compared with its JSON receipt and authenticated original input.

| Round | Added interval | Words | Actual next stop | Additional executed instructions |
| --- | --- | ---: | --- | ---: |
| 01 | `[0x00152DCC, 0x00152EB4)` | 58 | `0x00152b20` | 105 |
| 02 | `[0x00152B20, 0x00152BC0)` | 40 | `0x00151078` | 40 |
| 03 | `[0x00151078, 0x0015108C)` | 5 | `0x0015108c` | 5 |
| 04 | `[0x0015108C, 0x00151098)` | 3 | `0x00151098` | 43 |
| 05 | `[0x00151098, 0x001510B0)` | 6 | `0x001510bc` | 624 |

The first delta uses the retained `122580d` headless run; subsequent deltas use the preceding fresh run. Total advance: **817 executed core-0 instructions / 3,047 diagnostic ns**. This is startup execution progress, not a gameplay completion percentage.

## Actual final result

Final stop: **MissingBlock `0x001510BC`**, thread 7, dispatch round 21,843. Counters: 61,374,516 core-0 instructions, 99 core-1 instructions, 2,345,556,463 diagnostic CPU ticks and 8,748,424,997 diagnostic guest ns. Display periods: 523; complete captured audio records: 285,726.

Each run used fresh owned state/save/output paths, stopped with native exit 3, had no timeout and wrote no stderr. Complete captured audio and all state/save file contents match the baseline. Input, native, source and original AOT identities were verified before and after each run. The first full source rebuild was interrupted by the tool timeout and resumed without source changes; no test or game run was automatically retried.

All 5 builds passed **84/84 enabled tests once each**, with zero failures, errors or skips. Each exact tested executable was saved privately before its fresh game run.

No new desktop game run or screenshot was taken. The earlier loading image remains the last verified visible result. Menus, gameplay, audible playback and Windows execution remain unverified.

The restored source snapshot's computed Git tree matches the published parent. It is not a fabricated Git checkout. Publication uses the connector with the exact parent and a checked branch head, without force-pushing. Only the four status documents changed after the final tested candidate. Hosted checks are separate ROM-free checks, not original-game runs; consult the commit's Actions status and the private publication receipt.

## Next reviewed task

Add only **`[0x001510BC, 0x001510D0)`**, one block / 5 words / 20 bytes. Five original instructions advance the working pointer, load and pop a guest-stack word, test its flag, establish LR `0x001510DC`, and conditionally branch to recorded `0x00151028`. Existing fast, memory and immediate-TST handlers cover the range. The future stack value and branch outcome remain unobserved. Preserve memory faults, stack writeback, flags and target validation; do not include the unobserved fallthrough `0x001510D0` or eventual return body.

Preserve all 44 current supplements and the 599 original pages. Conditional totals after that single addition would be 45 supplements / 666 words; these are not current built totals. Authenticate, generate, build/test once, preserve the exact native and use a finite fresh run to identify the next actual stop.

## Working rules and history

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Publish source and metadata to `main` without force-pushing. Keep original inputs, generated game code, native bodies, captures, audio, saves and private recovery identifiers out of public Git. Keep user summaries short and plain.

The preceding checkpoint remains in Git history at `122580d`; older recovery history is retained in [HISTORY-7cf0d2a.md](HISTORY-7cf0d2a.md). Resume from [CHECKPOINT.md](../../CHECKPOINT.md).
