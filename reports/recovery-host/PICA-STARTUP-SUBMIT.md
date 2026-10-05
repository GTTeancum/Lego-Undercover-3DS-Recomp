# Original PICA startup-list execution and P3D delivery

October 5, 2026. Implementation `a136cdeb05c500f9426007a05f3acbf9563b2a9c`.
Exact tested/uploaded tree: `e40bd3048979a6f78c5f9e11fb5562da5b2054ec`.
Baseline main: `19139e9fcbdc53c1eda6076f47f34af615183ca2`.

## Observed execution

The original 29824-byte list at guest 0x14003790 has SHA-256
824caecf736fbd9febe688ee15b2f1da3913f9c19cf9f5a89e4539e540877c73.
It decodes to 559 packets and 6590 register writes. Original SubmitCmdList now
executes the supported non-drawing initialization effects: 4096 program-word
uploads, 96 uniform vectors, 1792 lighting LUT writes and one matching IRQ request.
These are measured operations, not frames or a completion percentage.

Independent Python replay matches the complete register bank, program arrays,
uniform values, integer uniforms, lighting arrays and written-state bitmaps against
logging-only before/after snapshots. VS uploads cover 512 words; mirroring those
plus 3584 GS uploads covers all 4096 GS words. These particular program values are
zero supplied by the guest, not evidence of executed shader instructions. GS has
96 known float vectors and VS has none. Unwritten uniforms have no asserted value.

The actual irq_request value 0x12345678 matches the inherited comparator and
requests autostop. Only after successful supported effects, a P3D entry (ID 5)
is added to the owner's real interrupt ring and its retained EventObject is signaled.
Thread 3 changes from WaitSynchAny to Ready with pending_wake=true and Result 0.
The one-shot signal is consumed by that waiter, so the event is already unsignaled
when inspected after the IPC call. This is not a missing notification or a forced
wake. The original thread runs and subsequently waits in address arbitration.

The next untouched shared packet is DisplayTransfer (ID 3), not a draw:

```text
01000103 1f5f8000 14013950 00800080 00800080 00004400 00000000 00000000
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=170
last_ipc_session=gsp::Gpu request_header=0x000c0000
host_ipc_error=GSP queue packet requires unimplemented GPU execution
```

The packet remains at queue index 2, count 1 (header 0x102). The rejected call's
shared page and GPU state are unchanged. Shared-page SHA-256:
83cf491b296dc411027337df9876776c907b67eabf222853b423dbeedbb3ee03.
No display transfer, shader execution, rendered frame, main menu or gameplay is
claimed. Existing-file startup reaches this boundary at round 162, skipping the
original gamecoin initialization; it is not save readback.

## Implementation boundaries

PICA lists are interpreted separately from direct WriteHWRegs. The decoder handles
little-endian headers, expanded byte masks, repeated/sequential parameters and
8-byte packet padding. Upload ports consume the raw parameter where the reference
does, even with mask zero. VS/GS mirroring follows the inspected configuration.
Float24 is converted losslessly to the reference's IEEE32 storage bit pattern;
Float32 uploads preserve bits and component order. This is not shader arithmetic.

All eligible queue work is staged before mutation. Unsupported effects, malformed
lists, missing input, response/source aliases or invalid/full interrupt rings leave
the entire eligible batch pending. This atomic host policy and the 1 MiB list cap
are not discovered hardware limits or error precedence. Supported input is the
observed aligned/readable linear-heap slice with flags=0 and do_flush=0. Channel 0
size/address/trigger setup uses the inspected physical conversion; trigger is cleared
only after the list's supported execution succeeds.

Draws, command-list chaining, immediate/default attributes, swizzle uploads,
fog/procedural uploads and ambiguous repeated non-upload special commands remain
explicit stops. List exhaustion without an IRQ request never creates an interrupt.
Topology/restart affect an empty assembler only; no vertices or draw execution exist.

P3D delivery is explicitly synchronous host policy, not modeled GPU duration or
hardware scheduling parity. Kernel time stays zero. No vblank, arbitrary completion
flag, synthetic frame or extra handle is created. The retained-object event helper
uses existing wake/one-shot/sticky/pulse behavior; the old handle-based implementation
and scheduler source are unchanged. Full/invalid relay overflow recovery is not modeled.

## Validation

Full GCC and Clang builds link all 599 unchanged private page units. All 28 CTest
suites pass with each compiler and all 28 ROM-free suites pass with Clang ASan/UBSan,
leak checking and halt-on-error. Eleven production startup scenarios have byte-identical
compiler logs. All 603 private AOT members, original code and raw RomFS are unchanged.
Full IVFC-block verification was not repeated; the raw SHA matches the previously
verified image. Seven original RomFS metadata reads still match all 4892 bytes.

The new suites cover masks, packets/padding, raw upload ports, capacity/mirroring,
packed floats, partial vectors, LUT wrap/type limits, IRQ matching/autostop,
unsupported effects, source/reply aliases, owner selection, retained events,
real waiters, relay wrap/fullness, full handle tables and atomic batch failures.
A test initially used an auto-return method before its definition; the explicit
return type fixed that compile error. Its failed log is retained. No failed test
was suppressed. Final production builds and the refreshed logging-only trace pass.

Public summaries: SUBMIT-PROOF.json, SUBMIT-VALIDATION.json, SUBMIT-FRESH-GCC.txt,
SUBMIT-EXISTING-GCC.txt. Complete scripts/logs/private captures: submit-checkpoint/.
Captured list bytes and state snapshots stay out of public GitHub.

## Primary reference and next work

azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac:
pica_core.cpp (parser, internal stores, uploads, IRQ), shader_setup.cpp/.h,
packed_attribute.h, pica_types.h, regs_shader.h, regs_lighting.h, pica_core.h,
gsp_interrupt.h; earlier gpu.cpp SubmitCmdList/physical routing and gsp_gpu.cpp
relay logic. Exact source blob identities are recorded in the private references.json.

Next inspect the actual DisplayTransfer packet's source/destination backing and
pinned transfer/blitter semantics. Do not manufacture missing VRAM or return success
without moving/converting the original bytes. Preserve PICA uploads, event ownership,
and fresh/existing regressions. No unverified transfer or renderer is implemented.

Hosted CI: GitHub Actions 37355280419 completed successfully for both GCC and Clang
on implementation a136cde. These jobs are ROM-free, not original-game execution.
