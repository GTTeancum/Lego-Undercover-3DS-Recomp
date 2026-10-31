# October 30, 2026 — larger startup routine continuation

## Current status

The rebuild tool now accepts larger startup routines. The previously blocked routine and four more missing startup sections now run. This advances startup only; menus and gameplay remain unverified. Read [CHECKPOINT.md](../../CHECKPOINT.md) first.

Base commit: `14b9f922de4bc570396c42ea5d9a30fced250604`. Added 5 observed sections / 245 instruction words. Totals: 54 supplemental blocks / 944 words; 111,097 runtime blocks / 546,055 verified words. Original 599 pages / 603 files and all 49 preceding supplemental records are unchanged. The generator's per-block limit is extended from 512 to 1024 bytes, retaining the same-page, first-exit, original-input, instruction-admission, alignment, text and nonoverlap checks. Python tests verify the new boundary, exact tail emission and rejection of missing/altered/unknown instructions. Synthetic C++ tests exercise 96-, 160- and 256-word blocks with interior loops through both dispatch paths. Runtime evaluators and build configuration are unchanged.

| Round | Added interval | Words | Actual stop | Instructions beyond preceding run |
| --- | --- | ---: | --- | ---: |
| 01 | `[0x00151D10, 0x00151F68)` | 150 | `0x00152a30` | +308 |
| 02 | `[0x00152A30, 0x00152A7C)` | 19 | `0x001510d0` | +385 |
| 03 | `[0x001510D0, 0x001510DC)` | 3 | `0x00152cc4` | +1307 |
| 04 | `[0x00152CC4, 0x00152DCC)` | 66 | `0x00152834` | +1347 |
| 05 | `[0x00152834, 0x00152850)` | 7 | `0x001515b8` | +5688 |

## Final measured result

The final native stopped at MissingBlock `0x001515B8`, thread 7, dispatch round 21843, exit 3. It did not time out and stderr was empty. It executed 61,387,994 core-0 and 99 core-1 instructions; diagnostic CPU ticks 2,345,569,941; guest time 8,748,475,267 ns; display periods 523; complete audio records 285,726.

Compared with the saved same-mode baseline, startup advanced 9,035 executed instructions / 33,698 diagnostic ns. This is not a performance benchmark or gameplay completion percentage.

## Validation

All 5 exact natives passed their 84-test suite once, with no failures/errors/skips or automatic retries. Generated C++ words and metadata match their generation receipts and authenticated original code. All previous supplement records are preserved. Source, AOT, inputs and frozen executables were checked unchanged around the builds and finite runs. Each executable was saved privately before its run.

Complete audio, save and state contents were compared byte-for-byte with the previous checkpoint and are unchanged. No new game-window run or screenshot was taken. The historical loading image is the last verified visible result. Speaker playback and Windows remain unverified.

During test development, the new size tests first rejected the old limit as expected. One test diagnostic expectation was then corrected for the unknown-mnemonic terminal case; production behavior was unchanged. All final candidate gates passed.

Source publication changes the supplement generator, its Python policy tests, the synthetic C++ registry test, the supplement manifest and four current status documents. The status documents are updated after the final run. Hosted verification runs without original game files; it is not a game test. Current hosted results are retained separately in the private checkpoint rather than predicted here.

## Next reviewed section

`[0x001515B8, 0x00151604)`: 19 words / 76 bytes. The next helper copies a pair of guest words to eight rows and returns through LR. Its memory values and eventual return destination remain unobserved. Existing handlers accept the reviewed instructions. The private offline review is tied to the final run's actual missing-code stop. Do not add an unobserved successor body or assume the eventual return destination is recorded.

Use fresh owned state/save/output paths, finite execution and strict ordinary memory/target checks. Preserve the original pages and 54 existing supplements; no forced return, game-code patch, runtime decoding or fabricated completion.

## Continuation permissions

Use ordinary tools in the chat's cloud workspace, not Codex services, Dot or the user's computer. Ordinary source and metadata publication to `main` is authorized; never force-push. The user authorized ongoing private builds and backups in the existing private recovery folder. Keep original game inputs, generated code, executables and all captures private. Keep summaries short and plain.

Previous current records remain in Git history and their separately preserved private checkpoints. Earlier recovery history remains in [HISTORY-7cf0d2a.md](HISTORY-7cf0d2a.md).
