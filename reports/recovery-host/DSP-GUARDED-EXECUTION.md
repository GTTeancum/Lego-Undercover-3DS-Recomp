# Guarded execution of the original DSP firmware

October 6, 2026. Based on published 3aca88f and the restored, previously unpublished
DSP-SPECIAL-PENDING source tree 0db6ecfb3f66abe608883b57ae64cde1c297cc80.

## What actually runs

A statically linked, pinned Teakra interpreter now executes the original 49,716-byte
DSP program in a disposable host-only probe. Staged segment bytes and the explicit
special-configuration source initialize the image. No guest DSP load reply, live
DSP device, ARM-memory bus, pipe service, interrupt schedule or audio sink is created.
The game still stops at LoadComponent, round 252. No rendered logo/menu/frame exists.

New opt-in: `--dsp-executor guarded-teakra`. The default remains inspection-only.
The known-only probe completes 962 Run(1) calls, then rejects an unknown SRAM read at
absolute byte 0x40AC0 (data offset 0xAC0). DSP PC advances 0x291A -> 0x291C on the
faulting attempt. There are 304 firmware word writes and zero received startup words.

With the ADDITIONAL explicit `--dsp-reset-profile reference-zero-data`, unwritten
DATA storage has the pinned interpreter's zero-reset provenance. Program gaps stay
unknown; supplied image bytes override reset state. This is a compatibility policy,
not a recovered hardware reset image or calibration. It cannot replace an unresolved
special-configuration source. Under that policy, the original firmware completes
1424 Run(1) calls and 432 word writes before an unmodeled MMIO read at offset 0x20E,
DSP PC 0xD08 -> 0xD0A. It still produces zero startup words. The pinned ICU notes
label 0x20E/0x210 polarity/source-type registers with uncertainty; that missing device
behavior is not replaced with a storage-only success implementation.

Counts are completed interpreter Run(1) calls, NOT measured instruction latency,
verified hardware cycles, game frames, or ARM-core execution. Both paths retain the
same pending ARM request, 4 threads, 29 handles, time 18447051 ns and zero original
core-1 instructions. All VRAM remains zero. This is real firmware investigation,
not progress past the game's DSP load operation.

## Safety and scope

A per-byte provenance map distinguishes unknown, staged image, explicit reference
data reset and actual firmware writes. Every raw SRAM word access is bounds checked
before access; unknown reads/fetches stop. Default unimplemented MMIO cells and
explicit unsupported getters/setters throw rather than return fallback storage.
This does NOT establish complete correctness of the reference's implemented
peripherals, reserved register bits or instruction set. Hardware reset/register
models elsewhere remain inherited from the pin.

Interpreter assertions are caught host errors rather than process aborts. A fault
may retain partial DSP instruction effects and seals the probe against continuation;
there is no claim of instruction rollback. Successful bounded slices resume exactly.
External-memory/audio callbacks fail closed. Mailbox history is bounded at 256 words.
`--dsp-probe-steps N` caps work at 100000 Run(1) calls per request, and requires the
executor selection. Duplicate/invalid options fail launch. Even protocol-complete
synthetic firmware cannot make the guest LoadComponent call return success.

The retained DspBootHandshake now reads genuine interpreter mailboxes: sequential
ready words on registers 0/1/2, followed by a distinct register-2 pipe word. A small
synthetic DSP program supplies those words through actual instructions in tests.
No host constant response is used. The original firmware has not reached that stage.

## Verification

Full GCC and Clang builds link all 599 unchanged private AOT page units. All 53
CTest suites pass on each compiler. All 53 ROM-free Clang ASan/UBSan suites pass
with leak checking and halt-on-error. The new suite covers real DSP stores/reads,
unknown/half-known memory, reference-data policy, actual mailbox instructions,
bounded continuation, unknown MMIO, moved-cell lifetime, address bounds, allocation
failure and unchanged guest IPC even on synthetic protocol completion.

Four paired original-startup cases match stdout/exit/test-file bytes: known-only,
reference-zero-data, bounded-100 and executor-disabled. Eleven invalid-option cases
match between compilers. Each of the two full probe modes has 84 matching private
capture files and matching trace stdout. Before/after inspection confirms unchanged
ARM request/input/register/thread/time state, 1842 GPU words, uploads, GSP/HID pages
and epochs, mapped HID, LCD controls and the entire 6 MiB VRAM bank.

Independent capture checks validate provenance accounting and that all changed
SRAM bytes were firmware-written. Known-only changes 210 bytes, reference-data
changes 285; distinct firmware-written spans total 588 and 732 bytes respectively.
This is NOT an independent DSP CPU oracle. All original code, raw RomFS, ExHeader
and all 603 AOT archive members retain their identities. No new IVFC pass or
Windows/macOS build is claimed.

## Dependency and recovery

Teakra (MIT) pin 3d697a18df504f4677b65129d9ab14c7c597e3eb was recovered from the
Azahar-pinned public submodule through isolated Actions run 37502002045 / artifact
11429593869. All 130 upstream Git blobs were checked. The build vendors only the
required subset plus license/provenance; it never downloads dependencies at build.
UPSTREAM.json identifies each original blob and five local guard patches. Local
patches do not alter decoded instruction algorithms. The MMIO guard also avoids
capturing temporary Cell objects and checks its index before access.

Private evidence: dsp-exec-checkpoint/. Preserve the prior DSP-SPECIAL-PENDING
archive separately in Library. The final handoff/receipt records actual publication
and saved-file verification; this report alone is not a hosted-CI claim.

Next: inspect the observed ICU register accesses and implement justified peripheral
semantics, then continue the actual startup handshake. Do not acknowledge load
until live execution, pipes, bus and lifecycle contracts exist. Default and strict
comparisons must remain available; never label reference reset as hardware proof.
