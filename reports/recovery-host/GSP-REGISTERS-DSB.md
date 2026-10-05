# GPU register setup and observed command-queue boundary

Date: October 5, 2026. Implementation: `92a403adcb3ed726a74c4d852431bcdb0cb20138`.
Tested/uploaded source tree: `af36cadf084642f67a0715d9120202deb2e48308`.

## Original execution

The original USA executable now executes 64 direct WriteHWRegs requests and four
WriteHWRegsWithMask requests. A logging-only IPC build records every input, mask,
previous value and resulting register. Independent Python replay checks all 68
against the pinned reset image and mask formula; ordinary GCC/Clang production
runs independently reach the same boundary. The first request remains the original
four-byte zero write at relative 0x00401000, mapped to GPU address 0x1EF01000.

After setup, the archived AOT metadata classified legacy DSB 0xEE071F9A at
0x00248404 as CoreAlu. The native system backend already handles this barrier.
Extending the existing local DMB routing predicate to that exact DSB form runs the
existing native sequentially consistent fence and continues the same block. It
does not skip the instruction or alter original code/private generated pages.
The generator's previous DMB fix is unchanged; no DSB regeneration is claimed.

Final fresh startup, with a NEW empty shared archive, explicit empty PTM and the
verified original RomFS, stops here:

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=166
last_ipc_session=gsp::Gpu requested_service= request_header=0x000c0000
```

The request is TriggerCmdReqQueue, still unsupported. The actual shared header at
0x10000800 is 0x00000100: index 0, one pending command, status 0, should-stop 0.
The first 32-byte packet at 0x10000820 is:

```text
00000105 14003790 00007480 00000000 00000000 00000000 00000000 00000000
```

Command ID 5 is CacheFlush, NOT a draw or SubmitCmdList. Its first region is
0x14003790 for 29824 bytes, with the other two regions zero. The packet's byte 1
is an unknown field equal to 1, not its stop byte. No packet is processed, count
is not decremented, the interrupt event remains unsignaled and guest time is zero.
The final shared-page SHA-256 is
d2531c5e39bb7588224934f764b6b1c148b005e6c2d905a58631e0ac49f9699c.

Existing-file startup reaches the same request at round 158, skipping gamecoin
initialization; it is not evidence of gamecoin readback or gameplay save/load.
All seven RomFS metadata reads (4892 bytes) still match original input. Original
guest creation/write/close of gamecoin.dat is retained. No menu, rendering, sound,
controls, completed initializer count, GPU completion or gameplay is established.

## Modeled behavior and limits

Direct header 0x00010082 uses static buffer ID 0. Masked header 0x00020084 uses
IDs 0 and 1. Exact descriptor sizes, full readable spans, address arithmetic and
response writability are checked before mutation. Input/response overlap includes
different virtual aliases of the same shared backing. Zero size dereferences no
input. Up to 0x80 bytes are staged, and the complete batch is checked before any
store. Unsupported bank/active trigger tails cannot partially change a prefix.

Error precedence follows pinned GSP: bad base/alignment, excessive size, then size
alignment. Masked writes compute (old & ~mask) | (data & mask). The modeled GPU
bank has 0x732 words at relative 0x00400000. LCD and other banks remain unsupported.
All five GPU::WriteReg action trigger cases are guarded: fill/transfer bit 0 or
nonzero command-list trigger stops before mutation. Disabled triggers store their
value without generating finish bits or interrupts. Direct MMIO is deliberately
not routed through PICA command-list special-register execution.

The explicit initial image is pinned PicaCore::Regs zero-initialization plus ALL
PicaCore::InitializeRegs assignments, including its compatibility IRQ compare,
framebuffer and shader defaults. This is REFERENCE-HLE policy, not a measured
hardware reset dump or proof of framebuffer/shader execution. State is module-wide
and retained across connections. The parser adds no unobserved rights requirement.

The vendor patch is documented in vendor/triaevum-a9b4477/LOCAL-PATCHES.md. Only
DSB/DMB normal-condition, non-PC register forms are rerouted to the existing fence.
Unknown forms still stop. Condition handling, CPU/exclusive state and in-block
continuation are tested. No clock advancement, event signal or forced wake is added.

ReadHWRegs, queue execution, enabled work triggers, rendering and interrupts are
not implemented. Full memory/cache coherence, asynchronous GPU timing, general IPC
mapping and scheduler parity remain open. The new alias predicate is used for this
register path, not a claim that every older service alias guard is generalized.

## Validation

Full GCC and Clang builds link all 599 private pages. GCC 25/25 CTest, Clang 25/25
CTest and Clang ASan/UBSan 25/25 ROM-free tests pass, with leaks/halt-on-error enabled.
Eleven startup scenarios match byte-for-byte between compilers: fresh, existing,
PTM-off, alternate RTC, no RomFS, no root, missing archive, invalid root, invalid
PTM mode, missing RomFS and wrong RomFS size. All 603 AOT members, code.bin and raw
RomFS are unchanged. All 187787 IVFC blocks verified again. Registry inventory
111043 blocks / 545111 raw words is static, not executed instructions or frames.

New tests cover reset values, masks, disabled/enabled triggers, atomic batch stops,
permissions, descriptors, pointers, shared aliases, full handle tables, module
independence, source reservation preservation and legacy barrier routing. One test
compile used the wrong existing enum name and was corrected before final passing
runs. No test failure was suppressed. An optional disassembly-library import failed;
LLVM's assembler/objdump confirmed the barrier instead. Full logs remain private.

GitHub Actions 37334783231 passed both compiler jobs on implementation 92a403a.
Hosted tests are ROM-free; the original-code executions are the separate local logs.
Public REGISTER-PROOF.json contains summary/queue and per-read hashes; full 68-write
replay, diagnostic trace and reproduction scripts are in register-checkpoint/ in
the downloadable source/evidence archive. Trace objects/binaries are excluded.

## References and next task

Pinned azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac:
- src/core/hle/service/gsp/gsp_gpu.cpp: WriteHWRegs/WithMask parsers and helper.
- src/video_core/gpu.cpp: MMIO bank routing, five action triggers, GPU::Execute.
- src/video_core/pica/pica_core.h/.cpp: register bank and complete InitializeRegs.
- src/video_core/pica/regs_external.h, regs_internal.h, regs_pipeline.h and
  regs_shader.h: offsets, trigger bitfields and reset fields.
- src/core/hle/service/gsp/gsp_command.h: pending queue and CacheFlush layout.

Next implement only justified TriggerCmdReqQueue handling and the actual CacheFlush
packet. Reinspect pinned queue iteration/stop rules and cache semantics first.
Do not decrement or acknowledge unimplemented packets, invent a completion interrupt,
or assume that this first packet draws anything. Then run unchanged original code
against a NEW empty test root to observe its next request.
