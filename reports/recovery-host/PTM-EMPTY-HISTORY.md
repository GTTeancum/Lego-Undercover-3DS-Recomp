# Explicit empty PTM history checkpoint

Date: 2026-10-05. Source commit: `0c4b013b9d96d664625c6135a216874153254ff6`.
Tested/published source tree: `e53a61304ccbe37b74939d34d8af7ed27655cbab`.

## Result and its qualification

With a deliberately new test archive and `--ptm-step-mode empty`, unchanged USA
code passes both PTM GetTotalStepCount and GetStepHistory, then stops at the
original FS OpenFile request for `/gamecoin.dat` at dispatch round 73.
Without that explicit option, fresh startup still stops at GetTotalStepCount.
This is an opt-in empty desktop pedometer profile, NOT recovered console step
history, sensor input, full PTM emulation, or a hardware-parity claim.

The real request/reply trace is:

```text
ptm:u request: 000c0000
reply:         000c0080 00000000 00000000
ptm:u request: 000b00c2 00000018 00000000 00000000 0000030c 0ffff5c8
reply:         000b0042 00000000 0000030c 0ffff5c8
history output: exactly 48 zero bytes (24 little-endian u16 entries)
fs:USER next:  080201c2 00000000 00000001 00000000 00000004 0000001c
               00000003 00000000 00070002 0036a046
```

The next request is OpenFile, archive handle 1, UTF-16 path `/gamecoin.dat`,
read/write flags 3, attributes 0. It remains untouched and unsupported.
A second process on the resulting existing file takes the separate prior
OpenFileDirectly branch, header 0x08030204, round 71. That is not evidence of a
successful fresh-file initialization. The 20-byte file is still the zero extent
created by the original guest; no Play Coin contents were seeded or written.

## Source basis

Pinned primary source: azahar-emu/azahar commit
`86a9f9236ae42bb5a2b995dbc933d599d8ea07ac`:

- `src/core/hle/service/ptm/ptm.cpp`: total returns u32 zero and is marked STUBBED;
  history writes one u16 per hour from a configurable steps-per-hour setting,
  returns the mapped buffer and is also marked STUBBED. This checkpoint chooses
  only the explicitly empty profile; it does not assume the upstream setting.
- `src/core/hle/ipc.h`: mapped descriptor size occupies bits 4..31 and W rights
  produce low nibble 0xC. The actual game requests 48 bytes for 24 hours.

Also inspected devkitPro/libctru `libctru/source/services/ptmu.c`, blob
`74920543afee138d5571f0a80a88e1a2b26f6c3f`. Its history wrapper encodes `hours`
rather than `hours*2` as the buffer size. That differs from the real game's
request and pinned Azahar's size check; it was NOT copied as the wire-size rule.
Original-code inspection of wrappers 0x0012B4BC and 0x0012B508 corroborates the
actual history size expression and total reply-word consumption. Private
disassembly is retained in the private checkpoint, not committed game bytes.

## Bounds and limitations

Only exact headers 0x000C0000 and 0x000B00C2 are accepted in EmptyHistory mode.
History requires the observed W-only mapped descriptor with exactly hours*2
bytes, bounded to 2048 hours / 4096 bytes. That cap is a host safety policy,
not a discovered firmware limit. Other PTM requests remain host stops.
The u64 timestamp is opaque for an empty event history; no time arithmetic,
step accumulation, host wall clock, or scheduled task is introduced.

Full output writability and address-wrap checks precede guest writes. The
existing router preflights the entire IPC reply before calling the service.
Zero-length output dereferences nothing. Guest Write8 operations preserve
exclusive-reservation invalidation. A buffer overlapping the IPC reply is an
explicit host stop, not guessed alias behavior. The current GuestMemory requires
the whole output to fit one writable region; general kernel mapped-buffer
translation, mixed regions, other rights, and physical sensor history are not
implemented. No handles, events, threads or guest time are created/advanced.

## Validation

Full unchanged 599-page GCC and Clang executables built successfully.
GCC 15/15 CTest; Clang 15/15 CTest; Clang ASan/UBSan 15/15 ROM-free CTest passed
with leak checking and halt-on-error enabled. The new suite checks mode gating,
exact replies, repeated/reopened sessions, opaque timestamps, output sentinels,
zero/max counts, unaligned/write-only buffers, malformed descriptors/headers,
size overflow, protected/short buffers, reply guards, aliases and exclusive
reservations. Two initial test compilation mistakes (enum labels and integer
initializer-list types) were corrected before all final passing runs.

Eight full-game startup scenarios produce byte-identical GCC/Clang logs:
fresh, existing, mode off, alternate RTC epoch, no root, missing archive,
invalid root and invalid PTM mode. All 603 regular private AOT archive members
are unchanged; 599 are C++ pages. Code SHA-256 remains
`5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f`.
Registry counts (111043 blocks, 545111 raw words) remain static inventory, not
executed instructions, frames or a completion metric.

Public fresh/existing logs are PTM-FRESH-GCC.txt and PTM-EXISTING-GCC.txt.
Complete build/test/run logs, matrix validator, non-mutating diagnostic trace
source and input fingerprints are in the private `ptm-checkpoint/` archive.
The trace differs only in host logging; no game instruction or response is
replaced to manufacture progress. No initialized save, menu, graphics, audio,
controls or gameplay has been demonstrated in this checkpoint.

## Hosted CI confirmation

GitHub Actions run `37295448850` on implementation `0c4b013` completed successfully
for both GCC and Clang jobs. Hosted tests are ROM-free, not game-data execution.
