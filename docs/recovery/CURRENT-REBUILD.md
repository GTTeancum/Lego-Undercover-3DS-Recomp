# October 30, 2026 — startup return and dispatch continuation

## What changed

Starting from `c656402`, three missing startup sections were added after actual missing-code stops. Each fresh headless run passed the preceding stop. This batch changes only the supplement manifest and four status documents; runtime, generator, tests and build configuration are unchanged.

The original 599 AOT pages / 603 files and all 36 earlier supplemental records remain unchanged. Current totals are **39 supplemental blocks / 549 words**, **111,082 registered blocks / 545,660 verified instruction words**. Each generated C++ operation and its JSON record were compared, and all words were checked against the authenticated original executable.

| Round | Added interval | Words | Actual next stop | Additional executed instructions |
| --- | --- | ---: | --- | ---: |
| 01 | `[0x00150EE8, 0x00150EF4)` | 3 | `0x00152850` | 465 |
| 02 | `[0x00152850, 0x00152898)` | 18 | `0x00150EDC` | 489 |
| 03 | `[0x00150EDC, 0x00150EE8)` | 3 | `0x00152DCC` | 3 |

The first delta uses the saved same-mode `c656402` baseline; later deltas use the preceding fresh run. The cumulative advance is **957 core-0 instructions / 3,569 diagnostic ns**. This is startup execution progress, not a percentage of gameplay completion.

## Actual final result

The final run stopped at **MissingBlock `0x00152DCC`**, thread 7, dispatch round 21,843. It recorded 61,373,699 core-0 instructions, 99 core-1 instructions, 2,345,555,646 diagnostic CPU ticks and 8,748,421,950 diagnostic guest ns. Display periods remained 523; captured audio records remained 285,726.

All three runs used separate fresh state/save folders. Each native exited 3 at its missing-code stop, without a timeout or stderr output. Source, executable and original AOT identities stayed unchanged during each run. Full captured audio, state contents and save contents match the saved baseline.

All three builds passed **84/84 enabled CTest suites once**, with zero failures, errors or skipped cases. Each exact executable was saved privately before its run. The builds used the existing headless Release configuration. No new original-game desktop run or screenshot was taken. Menus, gameplay, audible playback and Windows execution remain unverified; the earlier loading image is still the last verified visible result.

The source snapshot was matched to the real published base tree before changes. Publication uses the GitHub connector with the exact parent and a checked branch head, not fabricated local ancestry. The final published source matches the tested candidate except for the four status documents updated after the final run. Hosted checks are separate from these local build/run results; consult the commit's Actions status and private publication receipt.

## Next reviewed task

Add only **`[0x00152DCC, 0x00152EB4)`**, one block / 58 words / 232 bytes. The authenticated offline review accepts the section with existing immediate-AND, shifted arithmetic, memory and fast handlers. Its first control exit is the final stack-loaded return. No runtime or admission-rule change is currently required.

The section saves registers, combines guest byte values into several row words, restores its working pointer and returns through the stack. Memory values, successful completion and the actual return target remain unobserved. Preserve normal memory faults, partial effects, stack writeback and target validation; do not substitute live LR or add an unobserved successor. The candidate fits the existing same-page 512-byte bound.

Preserve all 39 current supplements. Generate from authenticated private input, verify exact preservation and words, build/test once, preserve the exact executable, and run with fresh private storage and finite limits. Conditional totals after that one addition would be 40 supplements / 607 words; these are not current built totals.

## Working rules and history

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Publish ordinary source and metadata to `main` without force-pushing. Original inputs, generated game code, executable bodies, captures, audio, saves and recovery identifiers stay private. Keep user updates focused on what improved and what still blocks play.

The prior checkpoint remains in Git history at `c656402`. Older recovery history is retained in [HISTORY-7cf0d2a.md](HISTORY-7cf0d2a.md). Read [CHECKPOINT.md](../../CHECKPOINT.md) first next time.
