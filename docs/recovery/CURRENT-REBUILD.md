# October 30, 2026 — startup decisions continuation

## Current status

Five more startup gaps are filled, including the byte-processing loop and its return. Startup now runs through that routine and reaches a larger unfinished helper. This advances startup only; menus and gameplay remain unverified. Read [CHECKPOINT.md](../../CHECKPOINT.md) first.

Base commit: `fb309a51629d785ee6f821819543f8e518df6bc8`. Added 5 observed sections / 38 instruction words. Totals: 49 supplemental blocks / 699 words; 111,092 runtime blocks / 545,810 verified words. Original 599 pages / 603 files and all 44 preceding supplemental records are unchanged. No runtime, evaluator, generator, test or build-configuration change was needed.

| Round | Added interval | Words | Actual stop | Instructions beyond preceding run |
| --- | --- | ---: | --- | ---: |
| 01 | `[0x001510BC, 0x001510D0)` | 5 | `0x001510dc` | +493 |
| 02 | `[0x001510DC, 0x001510E8)` | 3 | `0x001510b0` | +3014 |
| 03 | `[0x001510B0, 0x001510BC)` | 3 | `0x00153768` | +96 |
| 04 | `[0x00153768, 0x001537D0)` | 26 | `0x001537d0` | +80 |
| 05 | `[0x001537D0, 0x001537D4)` | 1 | `0x00151d10` | +760 |

## Final measured result

The final native stopped at MissingBlock `0x00151D10`, thread 7, dispatch round 21843, exit 3. It did not time out and stderr was empty. It executed 61,378,959 core-0 and 99 core-1 instructions; diagnostic CPU ticks 2,345,560,906; guest time 8,748,441,569 ns; display periods 523; complete audio records 285,726.

Compared with the saved same-mode baseline, startup advanced 4,443 executed instructions / 16,572 diagnostic ns. This is not a performance benchmark or gameplay completion percentage.

## Validation

All 5 exact natives passed their 84-test suite once, with no failures/errors/skips or automatic retries. Generated C++ words and metadata match their generation receipts and authenticated original code. All previous supplement records are preserved. Source, AOT, inputs and frozen executables were checked unchanged around the builds and finite runs. Each executable was saved privately before its run.

Complete audio, save and state contents were compared byte-for-byte with the previous checkpoint and are unchanged. No new game-window run or screenshot was taken. The historical loading image is the last verified visible result. Speaker playback and Windows remain unverified.

Source publication changes only the supplement manifest and four current status documents. The status documents are updated after the final run. Hosted verification runs without original game files; it is not a game test. Current hosted results are retained separately in the private checkpoint rather than predicted here.

## Next reviewed section

`[0x00151D10, 0x00151F68)`: 150 words / 600 bytes. The next helper combines guest byte values into row words and returns through the stack. It requires a tested increase to the size limit before the rebuild tool can include it. All instruction forms are individually supported, but the complete interval exceeds the current 512-byte generation limit. Add a tested size-policy extension first; do not bypass its bounds or fabricate a branch to split the routine. The private offline review is tied to the final run's actual missing-code stop. Do not add an unobserved successor body or infer a stack-loaded target from live LR.

Use fresh owned state/save/output paths, finite execution and strict ordinary memory/target checks. Preserve the original pages and 49 existing supplements; no forced return, game-code patch, runtime decoding or fabricated completion.

## Continuation permissions

Use ordinary tools in the chat's cloud workspace, not Codex services, Dot or the user's computer. Ordinary source and metadata publication to `main` is authorized; never force-push. The user authorized ongoing private builds and backups in the existing private recovery folder. Keep original game inputs, generated code, executables and all captures private. Keep summaries short and plain.

Previous current records remain in Git history and their separately preserved private checkpoints. Earlier recovery history remains in [HISTORY-7cf0d2a.md](HISTORY-7cf0d2a.md).
