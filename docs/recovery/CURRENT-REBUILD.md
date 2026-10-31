# October 30, 2026 — startup arithmetic and registry-lifetime continuation

## Current status

Added the missing arithmetic support and fixed a code-table lookup crash exposed by tests. Five more startup sections now run, including both processing loops and their returns. This advances startup only; menus and gameplay remain unverified. Start at [CHECKPOINT.md](../../CHECKPOINT.md).

Base: `28b8c7d9707f37daff3a66e507083db55ff0d176`. Added 5 observed sections / 128 words. Totals: 59 supplements / 1072 words; 111,102 runtime blocks / 546,183 verified instruction words. All 599 original pages / 603 files and 54 prior supplemental records are preserved. The generator now admits canonical immediate RSB with no PC operands. Its arithmetic still uses the existing evaluator. The runtime lookup cache now stores validated indices rather than borrowed block pointers, checking current entries and sorted insertion gaps on each hit. Two new native suites cover arithmetic and reused-registry lifetimes; Python policy tests cover the new admission. The same-page 1024-byte limit and all original-input, extent, overlap and first-exit guards remain unchanged.

| Round | Added interval | Words | Actual stop | Instructions beyond preceding run |
| --- | --- | ---: | --- | ---: |
| 01 | `[0x001515B8, 0x00151604)` | 19 | `0x0015330c` | +262 |
| 02 | `[0x0015330C, 0x001533B4)` | 42 | `0x001533b4` | +150 |
| 03 | `[0x001533B4, 0x001534B8)` | 65 | `0x001534b8` | +485 |
| 04 | `[0x001534B8, 0x001534BC)` | 1 | `0x001511b8` | +13177 |
| 05 | `[0x001511B8, 0x001511BC)` | 1 | `0x00151b6c` | +9690 |

## Final measured result

MissingBlock `0x00151B6C`, thread 7, dispatch round 21843; native exit 3, no timeout and empty stderr. Core-0 instructions: 61,411,758; core-1: 99. CPU ticks: 2,345,593,705; diagnostic guest time: 8,748,563,902 ns; display periods: 523; audio records: 285,730.

Same-mode advance: 23,764 executed instructions / 88,635 diagnostic ns. These are not a speed benchmark or a gameplay completion percentage.

## Validation and scope

Every exact new native passed its full enabled-test gate once: 84/84 (round 01), 86/86 (round 02), 86/86 (round 03), 86/86 (round 04), 86/86 (round 05), with zero failures/errors/skips. Targeted development tests first exposed a stale code-table pointer in the old lookup cache; the deterministic lifetime regression failed before the fix and passed afterward. The final focused Clang AddressSanitizer/UndefinedBehaviorSanitizer run passed all five selected suites. These development failures and final results are retained separately from successful build gates. Every previous supplemental operation and all original AOT files were preserved. Generated C++ and metadata match authenticated original instruction words. Source, original inputs, AOT and frozen executables were checked unchanged around compilation and each finite game run. The exact tested executable was saved privately before execution.

The complete prior audio capture is preserved byte-for-byte as an exact prefix; the final capture adds 4 well-formed diagnostic records. Complete state and save contents match the prior same-mode checkpoint. This is a longer diagnostic capture, not evidence of speaker playback. These were headless game runs. No new screenshot or visual progress is claimed; the earlier loading image remains the last verified windowed result. Speaker playback and Windows execution remain unverified.

Publication includes the narrow instruction-admission change, registry cache lifetime fix, authored native and Python tests, test registration, observed-range metadata and four current status documents. The status documents were updated after the final run; all non-status source matches the final tested candidate. Hosted CI runs without original game files. Its observed status is retained privately rather than predicted here.

## Next reviewed section

`[0x00151B6C, 0x00151D10)`. Add the next 105-instruction helper, which combines neighboring byte values into row words, restores its working pointer and returns through a stack-loaded address. The future memory values and return target remain unobserved. Existing handlers accept this reviewed interval. The private review is tied to the final actual missing-code stop. Preserve ordinary memory faults, partial effects and target validation. Do not infer a stack-loaded return from live LR or add unobserved successor bodies.

Use fresh owned state/save/output paths and finite execution. Preserve all 59 supplements and 599 original pages. No forced return, runtime decoding, game-code modification or fabricated GPU completion.

## Working permissions

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Source/metadata publication to `main` and ongoing private LEGO builds/backups are authorized. Never force-push. Keep original game inputs, generated code, executable bodies, audio, captures and saves private. Keep user-facing summaries short and plain.

Prior records remain in Git history and separately saved private checkpoints. Historical notes are retained in [HISTORY-7cf0d2a.md](HISTORY-7cf0d2a.md).
