# Device VRAM MemoryFill checkpoint

October 5, 2026. Implementation `8a2cf0153f39ffcbf75e98bcd77b6378b07749b6`.
Exact tested/uploaded tree: `6b079aa558863eb0ece5bcddad4339e2207abe11`.
Baseline: `8bad0fc0e253489ccf24c31dac7d92b00adb1c81`, tree
`431bd3c3dcb6e6faf4b0b17bb1a6aea532921b2d`.

## Original execution

The previously pending packet now executes against the existing explicitly configured
device-owned VRAM bank, not a new CPU mapping or a substitute output:

```text
01000102 1f070800 00000000 1f138800 00000000 00000000 000c8000 02010201
queue header: 00000109 -> 0000000a
reply:        000c0040 00000000
```

Channel 0 writes 819200 bytes in 32-bit patterns, value zero, over
0x1F070800..0x1F138800. The zero start disables channel 1 completely, including its
residual end/control fields. PSC0 (ID 0) is queued after the actual bytes are written;
then trigger bit 0 clears and finish bit 1 sets, leaving control 0x0202.
The complete register image, full 6 MiB VRAM bank and GSP page match an independent
Python replay of the captured packet. PICA upload state is unchanged by the fill.

The actual thread 3 changes WaitSynchAny -> Ready, pending_wake=true, Result=0.
It consumes the one-shot event, so the event is already unsignaled in the after
snapshot. Handle count stays 17, guest time stays zero. This is the existing
synchronous host IRQ policy, not measured GPU duration or an artificial time advance.

IMPORTANT: the original target range was already zero under the explicit
reference-zero startup policy. The game run therefore changes zero VRAM bytes.
This alone cannot prove a working nonzero fill. Separate ROM-free captures start
with nonzero sentinels and verify all bytes of three cases: a 32-bit fill changes
819200 bytes; overlapping dual fills change 2304 bytes; a 24-bit fill changes 384
bytes. GCC/Clang captures match exactly, and independent replay verifies every
VRAM byte, register and relay-page byte for each. These captures are synthetic,
not recovered game images or rendered frames.

## Next untouched request

Fresh production startup reaches round 198; existing-gamecoin startup reaches 190.
The next packet is a DIFFERENT DisplayTransfer:

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=198
host_ipc_error=DisplayTransfer format/layout/scaling flags are unsupported
queue_header=0000010a
packet=01000103 1f070800 1f300000 01900200 019001e0 01001004 00000000 00000000
```

Both endpoints select device VRAM. Decoding the captured fields using the pinned
register definitions gives input 512x400 RGBA8, programmed output size 480x400,
RGB8 output, horizontal half-scale and crop flag. The software transfer formula
would produce 240x400 output pixels; that conversion and VRAM output path are NOT
implemented yet. The supported older transfer is only flags 0x4400, equal-size
RGBA4 VRAM-to-private-linear-heap. Do not silently reuse it for this packet.

The new request remains queued. Its full VRAM, register, PICA upload, GSP page and
captured kernel snapshots are unchanged across rejection. GSP page SHA-256:
`b633bcdb420102646e9ba4288ca37c649ae50f12dde7d1c5e258806802d405fc`.
No rendered frame, main menu, shader execution or gameplay is established.

## Implementation scope and safety

New StageMemoryFill stages two channels without mutating guest/device state. A
zero start skips the entire channel. Nonzero starts with clear trigger store setup
but do not fill, set finish, or notify. Triggered channels support 16-, 24-, and
32-bit patterns with explicit little-endian byte order. When both width bits are
set, 24-bit takes precedence, following the inspected software path.

PSC selection follows the pinned GPU::Execute: when both starts are nonzero,
channel 0's notification is suppressed and triggered channel 1 requests PSC0.
Otherwise the enabled/triggered channel requests its own PSC ID. Selection depends
on start fields, not on whether both triggers are set. Overlapping channels commit
in channel order, so channel 1 overwrites overlapping channel-0 bytes.

Supported ranges are nonempty, 8-byte-aligned, half-open device VRAM spans. A
one-past-bank end is accepted. Only control bits 0x0303 are admitted; each triggered
channel is bounded to 1 MiB. Partial 24-bit patterns at a range end stop instead
of reproducing the software loop's potential overrun. These stricter checks, the
work bound and boundary policy are HOST POLICY, not discovered firmware capacity
or complete invalid-input parity. No FCRAM fill, CPU VRAM map, active MMIO fill
trigger, GPU timing, renderer, or new cache model is implemented.

The existing queue validates the entire eligible batch and relay capacity before
any dequeue, register, byte or event effect. Unsupported tails roll back valid
prefixes. A later transfer reading an earlier staged fill is rejected as a stale
snapshot dependency; forwarding inside a batch is not implemented. Separate calls
read the committed fill correctly, and independent mixed PICA/fill batches preserve
register and IRQ order. A fill write after preflight allocates nothing. Failed
staging allocation changes no device bytes. A hypothetical commit invariant failure
reports partial state after dequeue and does not emit PSC for that failed channel.

Existing response-permission and GSP-backing alias checks run before mutation.
Queue header/relay writes use the actual shared backing and invalidate its exclusive
reservations. Private VRAM is device-owned and has no CPU mapping or CPU reservation
epochs. The new code does not allocate kernel handles or change scheduler algorithms.

## Validation

Baseline restore verified all 429 manifest members and 300 source blobs, recreated
the exact Git tree and reproduced the round-193 stop before editing. Original code,
all 603 private AOT members (599 C++ pages), and the raw RomFS SHA remain unchanged.
Full IVFC-block checking was not repeated; the entire raw hash matches the previously
verified image. Registry 111043 blocks / 545111 words is STATIC inventory, not frames.

GCC and Clang full native executables link all 599 private pages. All 33 CTest suites
pass with each compiler; all 33 ROM-free Clang ASan/UBSan suites pass with leak checks
and halt-on-error. Fourteen production scenarios have identical compiler logs and
exits, preserving fresh/existing, strict no-VRAM, missing input and invalid-option
branches. Default strict VRAM still stops at the earlier transfer at round 170.

Two new suites cover nonzero pattern widths, original range size, capacity/boundary
checks, disabled/dual channels, IRQ selection, overlap, mixed PICA batches, 15-packet
wrap, real waiters, closed event handles, full handle tables, protected responses,
shared-response aliases, reservation coherence, dependency stops and allocation
failure. No preexisting test was removed or changed. The first compile exposed an
aggregate-brace error, fixed before all passing builds; its log is retained. An
initial foreground matrix hit the tool timeout; the complete unique-root rerun
passed all fourteen cases. Neither setup issue is hidden or reported as a passing test.

Eight implementation paths changed: CMakeLists.txt, cmake/LEGOHostRuntime.cmake,
gsp_gpu_service.h, gsp_command_queue.cpp, new gsp_memory_fill.h/.cpp, and two new
tests. Production IPC, vendor dispatcher/opcode backends, scheduler, PICA interpreter,
existing transfer backend and file backends are unchanged. No Windows/macOS build.

## Evidence and next work

Private evidence: fill-checkpoint/. Build logs, 33-suite logs, validation-summary.json,
identity-proof.json, original-captures/, synthetic capture folders and fill-proof.json.
make_trace.py compiles a logging-only alternate IPC object; the production router is
unchanged and ordinary builds independently reach the same stop. verify_fill.py
imports no production fill algorithm. Native/trace binaries and objects are excluded
from backups; their source/build scripts remain. Private captures stay off GitHub.

Next implement the exact scaled RGBA8-to-RGB8 VRAM transfer above. Reinspect the
pinned pixel decoding, averaging, Morton addressing, crop/dimension behavior and
PPF ordering. Preserve real source/destination bank data; do not substitute a frame
or emit completion before byte effects. Rerun original code on a NEW empty shared
archive with empty PTM, verified RomFS and explicit reference-zero VRAM.

Primary reference: azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
GPU::Execute MemoryFill and GPU::MemoryFill in src/video_core/gpu.cpp,
blob 40f29fea0867b0e8cf4d89a8753ddabf02c3b54b. Software fill in
src/video_core/renderer_software/sw_blitter.cpp, blob
0a68afd07c3e22fdfaaf441d4744ef79abb2a86f. Earlier regs_external.h defines control
and format fields. Reference behavior is pinned HLE semantics, not blanket hardware
parity. The web mirror failed; the exact pinned files were read through GitHub.

## Hosted CI confirmation

GitHub Actions run `37384971072` on implementation `8a2cf01` completed successfully
for both GCC and Clang jobs. Hosted suites are ROM-free, not original-game runs.
