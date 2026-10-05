# FS GetPriority and first GSP ownership checkpoint

Date: October 5, 2026. Implementation: `ad22af5b7e2added0475de84b2963f473a6f2070`.
Tested/uploaded source tree: `270f61e7a946906356c82fa3626f741d0883d001`.
Base: `36d8534dea59857261268e71d88a7604c197cebf`.
All 540 prior checkpoint files and 241 source identities verified before editing;
the original GetPriority stop was reproduced with only the test-root name changed.

## Original execution

GetPriority now replies `08630080 00000000 00000000`, returning the previously
stored module priority zero. The pinned UINT32_MAX branch only logs; the getter
returns that sentinel unchanged when it has not been set. The original wrapper
at 0x00131254 reads the returned value from IPC word 2. No thread priority,
default scheduling value, filesystem worker or time advancement is invented.

That change first exposed a failed gsp::Gpu lookup. The original guest then sent
AcquireRight and RegisterInterruptRelayQueue through handle zero, finally stopping
at SVC 0x1F at 0x00130910, round 95. This was not a valid GPU shared-memory request.
A logging-only trace captured the lookup failure and both invalid-handle calls.

Registering GSP discovery exposed the genuine AcquireRight request at round 92.
After implementing its first uncontended ownership transition:

```
AcquireRight request: 00160042 00000000 00000000 ffff8001
reply:               00160040 00000000
next request:        00130042 00000001 00000000 0007001c
```

The current unchanged-game fresh stop is UnsupportedIpc at PC 0x0025947C,
round 94, session gsp::Gpu, header 0x00130042 (RegisterInterruptRelayQueue).
The copied event handle resolves to an actual one-shot EventObject, unsignaled.
GSP ownership diagnostics show process 1 and client thread 1. The registration
request is untouched; no shared-memory handle, GSP numeric thread ID, interrupt
or GPU completion has been fabricated.

The existing-file process reaches the same request at round 86, skipping the
fresh gamecoin initialization. This does not establish gamecoin readback or
save/load. The default guest-written gamecoin hash remains
970a8b30f57b772c2c1c5686e634b9e4ab7055b43caec90e64076f81ed08b4b6.
All seven original RomFS metadata reads still match the source image, totaling
4892 bytes. These are header/directory/file tables, not rendered assets.

## Implemented ownership and deliberate limits

Each GSP connection gets its own internal session identity. The module records
one weak owning identity. Kernel handle duplicates share that session; closing
only one duplicate retains ownership, while destruction of the final session
reference releases it. Another connection can then acquire. These identities
are not the reference service's four numeric relay slots or guest threads.
Slot allocation and service connection-limit parity remain unimplemented.

Only exact flag-zero AcquireRight with one copy descriptor is handled while
unowned. The copied handle must resolve to the actual current ProcessObject;
a same-numbered but different process object is not accepted. Invalid or wrong-
type handles return transport InvalidHandle without a reply. Other process
objects cause a host stop. Repeat acquisition, contention, nonzero flags,
TryAcquireRight, ReleaseRight and every other GSP request remain host stops.
These are explicit implementation limits, not claimed hardware error semantics.

AcquireRight does not allocate handles, signal events, advance clocks or run
GPU work. The reference's renderer/program-settings paths are not ported here.
Response-writability preflight occurs before ownership mutation. FS GetPriority
also allocates no handles, even under handle exhaustion, and preserves shared
module priority across initialized sessions without changing scheduler state.

## Validation

Full 599-page GCC and Clang builds succeeded. GCC 21/21 and Clang 21/21 CTest
passed. Clang ASan/UBSan passed 21/21 ROM-free suites with leaks/halt-on-error
enabled. Eleven paired original-game scenarios have byte-identical compiler logs:
fresh, existing, PTM-off, alternate RTC, no RomFS, no root, missing archive,
invalid root, invalid PTM mode, missing RomFS and wrong RomFS size.

New tests cover stored priority and its sentinel, shared/independent session
state, duplicate ownership, final-reference release, copied process validation,
wrong object types, unmodeled contention, exact replies, malformed requests and
protected/partial response buffers. The initial GSP test needed an enum-to-u32
cast; it was corrected before all final builds. No failing suite was suppressed.

Original code, all 603 AOT archive files (599 pages), and the raw RomFS are unchanged.
The 111043 registry blocks and 545111 instruction words are static integrity
inventory, not frames or executed-instruction counts. Kernel time remains zero.
No rendered frame, main menu, audio, controls, completed initializer count or
playable gameplay has been established.

Public logs: GSP-FRESH-GCC.txt, GSP-EXISTING-GCC.txt, GSP-ROMFS-READ-PROOF.json,
GSP-VALIDATION.json. Complete private build/test/trace logs: priority-checkpoint/.
The trace uses an alternate logging-only IPC object; production IPC is unchanged.

## Primary references inspected

azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac:
- src/core/hle/service/fs/fs_user.cpp, GetPriority/SetPriority;
  blob 4f538400de9608ac92c212dc0eb8fc19c241e573, body in lines 1000-1290.
- src/core/hle/service/gsp/gsp_gpu.cpp, AcquireRight/AcquireGpuRight,
  ClientDisconnected, RegisterInterruptRelayQueue, constructor and SessionData;
  blob 6f915e4d6a5d27853321d0a233103afb9d877bc0.
- src/core/hle/service/gsp/gsp_gpu.h, service/session declarations;
  blob fe08556003db960ba22a69cb5846a2c360d4aaca.

Next: reconstruct real GSP relay slots/shared-memory ownership and the observed
RegisterInterruptRelayQueue response, including the reference's nonzero first-
initialization result. Do not invent a shared-memory handle or a successful map.

## Hosted CI confirmation

GitHub Actions `37315649774` on implementation `ad22af5` completed successfully
for both GCC and Clang jobs. Hosted tests are ROM-free, not original-game runs.
