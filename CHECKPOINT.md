# Latest LEGO Undercover 3DS checkpoint

## Objective and current state

The goal is a full playable native recompilation of LEGO City Undercover: The Chase Begins. This batch accepts three candidates and five sections, reaching **187 sections**. Actual startup execution advances **97,555 instructions** from the published 9122d64 checkpoint and stops at **MissingBlock `0x0014FEF0`**.

## What changed

Candidates 37 and 38 add observed missing blocks. Candidate 39 adds one observed root and two statically reviewed continuations: the internal loop tail and terminal return of the same paired-row routine. Its backward branch stays within that reviewed routine. One build, one test suite and one fresh native run are recorded for candidate 39. Original instructions, branches and guarded memory effects are preserved.

## Actual runs and verification

| Candidate | Added sections | Instruction advance | Actual stop |
|---|---:|---:|---|
| 37 | 1 | 28 | `0x00155104` |
| 38 | 1 | 188 | `0x00154E74` |
| 39 | 3 | 97,339 | `0x0014FEF0` |

All three candidates passed **90/90 local tests**, with explicit build and test exits of zero. Their native orchestration completed successfully; each native process stopped with the recorded MissingBlock exit 3, no timeout and empty stderr. Original game inputs and AOT files remained unchanged. Runtime, generator, test and configuration source stayed unchanged except for the supplement manifest. Candidate 37 matched its measured **28-instruction, 17-field** prediction; candidate 38 executed the return and advanced **188 instructions**. All three build-wrapper exits were zero; no current archive-wrapper failure is recorded.

At source preparation, hosted checks were awaiting publication. Actual results appear against the published commit in [project Actions](https://github.com/GTTeancum/Lego-Undercover-3DS-Recomp/actions).

## Captures and known limits

The complete baseline audio capture remains an exact prefix; 8 valid records were appended. All appended samples were zero. The final capture contains 285,878 records (4,574,080 bytes). Persisted state and save files stayed byte-identical to the batch baseline.

Menus, gameplay, rendering, speaker playback and Windows execution remain unverified. No new window or screen capture was taken. Instruction and guest-time counters are diagnostic evidence, not measurements of frame rate or completion.

## Remaining blocker and next action

The actual next missing block begins at `0x0014FEF0`. Its current reviewed interval is `[0x0014FEF0, 0x0014FEF8)`. This interval saves the incoming link register and calls the missing entry at `0x0015027C`; its assigned return at `0x0014FEF8` is also uncovered. The existing loop/return packet modes do not cover this call shape. Review the bounded call flow before choosing the next grouped admission, preserve the saved caller link and ordinary guarded stack write, and count accepted sections from the manifest. No future execution result is claimed.

## Resume, permissions and prior history

Continue from this checkpoint with resident authenticated inputs. Existing authorization covers pushes and GitHub Actions for this project. Keep game inputs, generated code, native binaries, captures, saves and backups private. Use cloud tools; do not use the user computer, Dot or Codex services. Do not force-push.

- [Full previous CHECKPOINT.md](https://github.com/GTTeancum/Lego-Undercover-3DS-Recomp/blob/9122d64a91b11e7acbdaaefbe01dd093ae9920cf/CHECKPOINT.md)
- [Full previous README.md](https://github.com/GTTeancum/Lego-Undercover-3DS-Recomp/blob/9122d64a91b11e7acbdaaefbe01dd093ae9920cf/README.md)
- [Full previous docs/DESKTOP.md](https://github.com/GTTeancum/Lego-Undercover-3DS-Recomp/blob/9122d64a91b11e7acbdaaefbe01dd093ae9920cf/docs/DESKTOP.md)
- [Full previous docs/recovery/CURRENT-REBUILD.md](https://github.com/GTTeancum/Lego-Undercover-3DS-Recomp/blob/9122d64a91b11e7acbdaaefbe01dd093ae9920cf/docs/recovery/CURRENT-REBUILD.md)
