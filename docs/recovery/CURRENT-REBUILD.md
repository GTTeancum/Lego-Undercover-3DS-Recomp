# October 30, 2026 — startup continuation

## Current status

Five more startup sections now run, including the recovered helper from the interrupted session. The rebuild tool also handles a larger routine that crosses a code-page boundary, without changing its original instructions or adding a jump. Start at [CHECKPOINT.md](../../CHECKPOINT.md). Menus and gameplay remain unverified.

Base: `7e076537fd8514aa595a74e20cab71833e5a41ba`. Added 5 observed sections / 533 words. Totals: 64 logical sections / 65 physical blocks / 1605 words; 111,108 runtime blocks / 546,716 verified words. All 599 original pages / 603 files and 59 prior supplemental records are preserved. The generator adds explicit page-boundary fallthrough support. It validates a complete logical section up to 1024 bytes before emitting same-page physical pieces. The first real control exit still has to be the section's last original word. Existing input authentication, instruction admission, text bounds, alignment, size and nonoverlap checks remain. Runtime evaluators are unchanged.

| Round | Added interval | Words | Actual stop | Instructions beyond preceding run |
| --- | --- | ---: | --- | ---: |
| 01 | `[0x00151B6C, 0x00151D10)` | 105 | `0x001534bc` | +3813 |
| 02 | `[0x001534BC, 0x001535DC)` | 72 | `0x001535dc` | +380 |
| 03 | `[0x001535DC, 0x001535E0)` | 1 | `0x00151f68` | +18126 |
| 04 | `[0x00151F68, 0x001522BC)` | 213 | `0x001522bc` | +17737 |
| 05 | `[0x001522BC, 0x001524F4)` | 142 | `0x001524f4` | +10459 |

## Final measured result

MissingBlock `0x001524F4`, thread 7, dispatch round 21843; native exit 3, no timeout and empty stderr. Core-0 instructions: 61,462,273; core-1: 99. CPU ticks: 2,345,644,220; diagnostic guest time: 8,748,752,312 ns; display periods: 523; audio records: 285,734.

Same-mode advance: 50,515 executed instructions / 188,410 diagnostic ns. These are not a performance benchmark or a gameplay percentage.

## Validation and scope

Each exact new native passed its enabled-test gate, once with no failures/errors/skips: 86/86 (round 01), 86/86 (round 02), 86/86 (round 03), 88/88 (round 04), 88/88 (round 05). Source, original inputs, AOT pages and executables were verified unchanged around build and fresh finite execution. Every previous supplemental operation was preserved. Generated C++ and metadata agree with authenticated original instruction words. Exact executables were saved privately before execution.

The interrupted pending build was recovered. Its saved test report passed 86 suites, and the fresh first build has identical generated operations. Its run evidence was unavailable, so this continuation uses a new build and fresh game run instead of claiming a prior result.

Complete state and save contents match the previous same-mode checkpoint. The entire prior audio capture is preserved byte-for-byte, with four valid silent diagnostic records appended; this is not evidence of speaker playback. These were headless runs. No fresh game-window run or screenshot was taken. The earlier loading image remains the last verified visible result. Speaker playback and Windows execution remain unverified. Passing diagnostic tests is not evidence of playable gameplay.

This publication changes the generator, observed-range metadata, two authored test files, CMake test registration and four status documents. The old 62-block generation was independently checked byte-for-byte unchanged before adding the first crossing section. Six new Python policy tests, 41 existing policy tests, the page-boundary runtime fixture and five focused Clang address/undefined-behavior sanitizer suites passed. The new policy tests rejected the old generator; a diagnostic expectation in one test was corrected during development, with those earlier failures retained rather than described as passes. The status documents were updated after execution; all non-status source matches the final tested candidate. Hosted checks run without original game inputs. Their observed status is retained privately rather than predicted here.

## Next reviewed section

`[0x1524F4, 0x152834)`. Add the next 208-instruction helper, which combines guest byte values using stack temporaries, writes row words, restores its working pointer and returns through a stack-loaded PC. Its future memory values and return destination remain unobserved. Existing handlers accept this reviewed interval. The private review is tied to the actual final stop. Preserve memory faults, partial effects, flags and target validation. Do not infer stack-loaded returns from live LR or add unobserved successor bodies.

Use fresh owned state/save/output paths and finite execution. Preserve all 64 logical sections (65 physical blocks) and original AOT pages. No forced return, runtime decoding, game-code modification or fabricated GPU completion.

## Working permissions

Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Source/metadata publication to `main` and ongoing private builds/backups are authorized. Never force-push. Keep original inputs, generated game code, executables, audio, captures, saves and recovery identifiers private. Keep user summaries short and plain.

Prior records remain in Git history and separately saved private checkpoints. Historical notes remain in [HISTORY-7cf0d2a.md](HISTORY-7cf0d2a.md).
