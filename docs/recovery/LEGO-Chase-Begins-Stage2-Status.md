# LEGO Chase Begins — Stage 2 execution status

**September 21, 2026. Native startup checkpoint; not playable.**

## What ran

A standalone headless C++ executable ran actual ARM startup code translated from
the supplied USA game revision. Both GCC and Clang Linux builds produced identical
startup logs. There is no general ARM interpreter/JIT fallback and no Zelda game
launcher in this target. Historical upstream namespaces remain in reused code.

The upload's SHA-256 matched the checksum list. The source snapshot identifies
TriAevum commit `a9b447709d4405848d75352354891059cebb9ff8`.

## Verified results

| Check | Result |
|---|---|
| Python preparation/frontend tests | 17 passed |
| Native kernel/memory assertions, active in Release | 75 passed |
| Neutral TriAevum module test executables | 12 passed, separate from the LEGO target |
| BSS startup routine | Cleared 2,219,880 bytes pre-filled with 0xA5; adjacent guard words unchanged |
| Startup memory setup | Committed the normal and linear heaps requested by the game |
| Global initialization | 225 of 296 initializer callbacks returned before the next unsupported SVC |
| GCC versus Clang | Identical logged startup state and stopping point |
| Wrong executable revision with identical file length | Rejected by SHA-256 check |
| Regeneration | Identical generated C++ and manifest on the tested host |
| Windows build/execution | Not tested; supplied scripts reviewed only |

The application's 64 MiB budget is accounted for as follows:

- Existing code/data/BSS/main-stack allocation: 4,935,680 bytes.
- Normal heap: base `0x08000000`, size `0x0124B000`.
- Linear heap: base `0x14000000`, size `0x02900000`.

Those allocations total 67,108,864 bytes. They do not prove that later memory
management, resource-limit edge cases or multiple threads are implemented.

## Exact current blocker

```text
SVC 0x00000027 pc=0x0013313C DuplicateHandle (not implemented) STOP
Initializer callbacks completed: 225 / 296
Native blocks executed: 557726
```

The request's input `r1` is `0xFFFF8000`, the current-thread pseudo-handle in the
pinned host ABI. The bootstrap intentionally has no DuplicateHandle implementation.
This is a known stop inside initialization, not successful game startup.

Extending the host requires proper handle/object ownership and main-thread
semantics rather than returning a fabricated successful handle. Subsequent
threading, service IPC, filesystem and graphics integration remain separate work.

## Translation and validation limits

The bounded generator emitted 5,000 block entries covering 29,805 distinct
instruction addresses, with 118 static roots still pending. Its generated set has
no explicit unsupported-lowering traps; that is **not** evidence that all executable
instructions or indirect targets are supported. The startup-specific frontend is
not a general replacement for whole-program control-flow recovery.

The 296 initializer table entries are slot-relative pointers, verified from the
game's dispatcher at `0x001336CC`. Three additional callback roots were observed
at runtime and inspected: `0x002DEB78`, `0x002FAC60`, and `0x003261EC`.

Configuration-page defaults and non-memory resource caps follow the uploaded
TriAevum host policy. Physical-hardware trace equivalence has not been checked.
Passing these tests is not a proof of all ARM/VFP semantics.

No renderer, audio playback, game filesystem IPC, controller input, saves, movies,
networking, multi-thread scheduling, dynamic code loading or gameplay has been
validated. No screen or playable Windows EXE is claimed.

## Handoff

Extract the source package, run **Build-and-Test.cmd**, select the original extracted
decrypted USA game image, and return the ZIP generated under **logs**. The expected
result is a passed BSS test followed by the documented DuplicateHandle stop.
The game image and framework do not need to be uploaded again.

See README.md for prerequisites and instructions. Actual logs and machine-readable
results are in reports/. The source ZIP contains translated C++ and supporting
source, not the original game image, code.bin, assets or a prebuilt Windows binary.