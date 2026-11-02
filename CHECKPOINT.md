# Latest LEGO Undercover 3DS checkpoint

## Objective and actual state

The goal is a full playable native recompilation of LEGO City Undercover: The Chase Begins. This batch translates **56 reviewed sections** in **5 fresh candidates**, reaching **243 sections**. Actual native startup execution advances **573,982 instructions** from published 63c5aef and stops at **MissingBlock `0x00154520`**.

## Changes and actual checks

Adds missing calls, returns from the saved stack and row processing callbacks through reviewed groups. Original instructions, branches and guarded memory effects are preserved. Translated section counts describe admitted code coverage; execution evidence consists of measured native instruction totals and final stops. Coverage of each statically included section remains unmeasured.

| Candidate | Reviewed sections added | Instruction advance | Actual stop |
|---|---:|---:|---|
| 40 | 13 | 81 | `0x0015522C` |
| 41 | 4 | 339 | `0x001554B0` |
| 42 | 16 | 47,492 | `0x0015516C` |
| 43 | 16 | 97,583 | `0x0014FB84` |
| 44 | 7 | 428,487 | `0x00154520` |

Each candidate passed **90/90 local tests**, with explicit build and CTest exits of zero, and completed an accepted fresh native run. Native exits are the recorded MissingBlock exit 3, with no timeout or stderr. Original inputs and AOT files stayed unchanged. Runtime, generator, tests and configuration source stayed unchanged except for the supplement manifest. Intermediate forecasts, including the earlier 15- and 21-instruction predictions, require their own measured boundary receipts; these native totals do not verify them.

Post-archive wrapper checks exited 1 for candidates 40, 42, 43, 44 after detecting shortened local test logs; their build and CTest subprocesses exited 0. The original failures were preserved and the exact complete archived logs restored. Build/CTest succeeded; native, archive and source identities stayed unchanged, with no rebuild or test rerun. The cause remains unknown.

At source preparation, hosted checks were awaiting publication. Actual results appear against the published commit in [project Actions](https://github.com/GTTeancum/Lego-Undercover-3DS-Recomp/actions).

## Captures and limits

The final diagnostic capture contains 285,950 records (4,575,232 bytes). The entire baseline remains an exact prefix, with 72 valid records appended. All appended samples are zero. Persisted state and saves stayed byte-identical.

Menus, gameplay, rendering, speaker playback and Windows execution remain unverified. No new window or screenshot was captured. Diagnostic counters do not establish frame rate or gameplay completion.

## Remaining blocker and next action

The next actual missing block begins at `0x00154520`. The current review interval is `[0x00154520, 0x00154578)`. Review a bounded group rooted at this observed stop and its directly connected calls or callbacks, using authenticated control flow. Derive counts from the accepted manifest and admitted intervals, then build, test and run the reviewed group as one candidate. No future execution result is claimed.

## Resume and permissions

Existing authorization covers cloud work and GitHub retrieval, pushes and Actions for this project. Keep game inputs, generated instructions, binaries, captures, saves and backups private. Do not use Codex services, Dot, the user PC or force-push.

- [Complete previous CHECKPOINT.md](https://github.com/GTTeancum/Lego-Undercover-3DS-Recomp/blob/63c5aefc07f9621538c7bc7f119e0bbd30984ca1/CHECKPOINT.md)
- [Complete previous README.md](https://github.com/GTTeancum/Lego-Undercover-3DS-Recomp/blob/63c5aefc07f9621538c7bc7f119e0bbd30984ca1/README.md)
- [Complete previous docs/DESKTOP.md](https://github.com/GTTeancum/Lego-Undercover-3DS-Recomp/blob/63c5aefc07f9621538c7bc7f119e0bbd30984ca1/docs/DESKTOP.md)
- [Complete previous docs/recovery/CURRENT-REBUILD.md](https://github.com/GTTeancum/Lego-Undercover-3DS-Recomp/blob/63c5aefc07f9621538c7bc7f119e0bbd30984ca1/docs/recovery/CURRENT-REBUILD.md)
