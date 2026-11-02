# Reviewed startup callbacks from 0e4737b

## Objective and actual state

The goal is a full playable native recompilation of LEGO City Undercover: The Chase Begins. This batch adds **12 reviewed logical sections containing 64 original instructions**, reaching **255 logical sections / 257 physical blocks / 3,744 supplemental instruction words**. Three fresh native startup runs advance **120,851 core0 instructions** beyond published `0e4737b` and stop at **MissingBlock `0x0014F620`**.

## Changes and measured execution

Adds two row-processing callbacks and their returns, plus four wrappers from an authenticated original callback table with their separate stack returns. Original instructions, branches, register effects and guarded memory behavior are preserved. Runtime, generator, tests and the original AOT pages are unchanged. Only the supplement manifest and current-status documentation change publicly.

| Candidate | Added sections / words | Measured instruction advance | Actual MissingBlock | Native seconds |
|---|---:|---:|---|---:|
| 45 | 2 / 23 | 73,447 | `0x0014FB34` | 202.032 |
| 46 | 8 / 20 | 41,650 | `0x00154920` | 242.083 |
| 47 | 2 / 21 | 5,754 | `0x0014F620` | 267.813 |

Every final candidate build passed **90/90 existing local tests** with configure, build and CTest subprocess exits of zero. Each exact tested native was verified and privately saved before its fresh run. All three native runs returned the recorded MissingBlock exit 3, with no timeout or stderr. Input, AOT, source, native, archive and save-metadata integrity checks passed. Total translated coverage does not establish that every statically admitted section executed.

The final run recorded **63,338,096 core0 instructions**, 99 core1 instructions and 2,347,520,043 diagnostic CPU ticks, with 523 modeled display periods. The final guest-time counter is 8,755,748,732 ns. These are diagnostic measurements, not hardware timing or gameplay completion.

## Verification and preserved failures

Candidate 45 initially passed 86/90 tests. Two test executables were found empty; two filesystem permission tests failed in the restricted UID/GID environment. The empty outputs were preserved and rebuilt, and the unchanged suite passed 90/90 in a cloud environment supporting its required identity changes. The retry controller subsequently failed an incorrect prediction of two executable relinks; the actual build relinked three, including a byte-identical native executable. A separate empty raw configure-log copy was restored exactly from its complete, hash-bound canonical copy. All original failures and the later verification are retained.

Candidate 46's build and all 90 tests passed, but its archive controller detected permissions of 0644 on two evidence files whose archive recorded 0444. Only those permissions were restored; all 14 member contents, the compressed archive and the native binary stayed unchanged. No build or test rerun was needed. Candidate 47 preserved the admission/manifest modes throughout and completed its build, tests and archive controller with exit 0.

The causes of the empty files and unexpected permission changes remain unproven. These recovery records do not replace or relabel the original failed attempts.

At source preparation, hosted checks for this change had not yet run. Their actual results are attached to the published commit in [project Actions](https://github.com/GTTeancum/Lego-Undercover-3DS-Recomp/actions). Hosted ROM-free checks do not execute the original game.

## Captures and limits

The final diagnostic audio file contains **285,966 records / 4,575,488 bytes**. It is byte-identical to candidate 46 and retains the complete published-baseline capture as an exact prefix, with 16 valid zero-sample records appended across this batch. Persisted state and both save files remain byte-identical to the baseline. This is diagnostic capture evidence; sample rate and speaker playback remain unasserted.

Menus, gameplay, game rendering, speaker playback and Windows execution remain unverified. No new window or screenshot was captured. Diagnostic counters and cloud elapsed times do not establish frame rate or gameplay completion.

## Remaining blocker and next action

The next observed missing block begins at `0x0014F620`. Review a bounded group rooted in that actual captured state and its authenticated original calls or returns. The unchanged logical-section limit is 256, leaving one slot. If the useful connected group requires more space, first implement and verify a separate bounded capacity change, retaining the per-section size limit, original-word authentication, opcode restrictions, overlap checks and strict missing-target behavior. Test the new count boundary, late entries and registry behavior before admitting more game code.

## Resume and permissions

Resume from this published source and the latest private checkpoint. The current private helpers deliberately bind the earlier source commit and evidence; review and rebase them to the new published commit before the next candidate. Restore only missing private dependencies. Preserve the existing 255-section manifest prefix and the original 599 pages / 603 AOT files.

Existing authorization covers cloud work and GitHub retrieval, pushes and Actions for this project. Keep game inputs, generated instructions, binaries, captures, saves, backup identifiers and credentials private. Do not use Codex services, Dot, the user PC or force-push. Keep updates concise and save a resumable checkpoint at a stopping point.

- [Complete previous CHECKPOINT.md](https://github.com/GTTeancum/Lego-Undercover-3DS-Recomp/blob/0e4737b5eba2d09ed6002daffd44d2650b06bf6a/CHECKPOINT.md)
- [Complete previous README.md](https://github.com/GTTeancum/Lego-Undercover-3DS-Recomp/blob/0e4737b5eba2d09ed6002daffd44d2650b06bf6a/README.md)
- [Complete previous docs/DESKTOP.md](https://github.com/GTTeancum/Lego-Undercover-3DS-Recomp/blob/0e4737b5eba2d09ed6002daffd44d2650b06bf6a/docs/DESKTOP.md)
- [Complete previous docs/recovery/CURRENT-REBUILD.md](https://github.com/GTTeancum/Lego-Undercover-3DS-Recomp/blob/0e4737b5eba2d09ed6002daffd44d2650b06bf6a/docs/recovery/CURRENT-REBUILD.md)
