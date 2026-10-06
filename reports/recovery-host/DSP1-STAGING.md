# DSP1 image staging and HID/launch reconciliation

Parent main: `10b48d25fd8443751916d14daf0b622c49d9ca78` (tree
`db674f651060aff0061fc070737d971a8aa7cd61`). Preserved unpublished HID source:
`b81593cb4e9f3e47e4527876325f114f5045b1e2`, based on `4e6c9cd`.

The pending HID implementation has been reconciled with the newer verified
ExHeader launch policy. Shared-page and real event export, read-only mapping,
transactional handle allocation, and the title's maximum CPU value of 30 coexist.
The isolated old Core1Budget component remains tested but is NOT the live scheduler.
The inherited diagnostic-dual timing model is unchanged.

## New DSP work

A bounded DSP1 parser validates magic, declared size, segment count/type, ranges,
word-address conversion, selected banks, overlaps and segment SHA-256. It stages
program and data bytes into separate HOST images with per-byte known masks.
ProgramA and ProgramB address the same program bank. It never treats an unfilled
gap as valid zero SRAM, never maps the images into guest memory, and never executes
DSP instructions. RSA verification is not implemented; segment hashes establish
integrity against this image's table, not signature authenticity. The original
input is separately verified as a byte-identical slice of authenticated code.bin.

The game supplies 49716 bytes at 0x0037A120. All five segment hashes match. Two
program segments contain 43692 bytes; three data segments contain 5256 bytes.
The whole component SHA is
`7ea3c44a1c57514bebbebce8a7995f7f3a290170ea3b6c145749ad9358672c97`.
The exact staged bytes and known masks independently match a Python replay.

Flags are 3: startup-register replies and a special segment are both required.
The special segment targets data word 0xEF29 (byte 0x1DE52), size 0x214 (532 bytes).
Its contents are NOT among the ordinary file segments. That span stays unknown.
3dbrew's DSP Binary research describes a system CFG 0x70000 source and a zero
fallback on a failed configuration read. No such read or failure has been modeled
here; missing data is not silently replaced by zeros. Pinned Azahar LLE explicitly
leaves special-segment handling TODO and waits for real firmware responses before
marking the component loaded. Its HLE LoadComponent merely hashes the input because
an existing HLE audio backend implements the surrounding behavior; this project
cannot borrow that unconditional success without the missing backend.

Valid observed LoadComponent shapes are inspected, then request an explicit host
stop. The original IPC and CPU registers remain untouched. There is no guest
success response, DSP-ready state, memory mapping, event, semaphore, pipe base,
audio sample or extra elapsed guest time. The last valid inspection snapshot is
retained when a subsequent inspection fails; it is diagnostic state, not a live
component. Full reply preflight still precedes the service handler.

All prior unsupported DSP commands remain unsupported. A 1 MiB input cap, odd-byte
and overlap rejection, and full-mask-only IPC are explicit host inspection limits,
not complete firmware validation rules or guest error precedence. Errors including
allocation failures preserve the previously published inspection image.

## Actual run and verification

The combined authenticated original run still stops at DSP LoadComponent:
round 252, PC 0x0025947C, header 0x001100C2, guest time 18447051 ns,
4945873 diagnostic ticks, 2287029 core-0 instructions, zero core-1 instructions.
No new game progress beyond this DSP call is claimed. Four threads/29 handles
remain. The title header now gives maximum30 instead of the pending HID run's
inherited maximum80. With no ExHeader option the inherited branch is preserved.

Full GCC and Clang executables link all 599 private AOT page units. All 50 CTest
suites pass under each compiler; all 50 ROM-free Clang ASan/UBSan suites pass with
leak checking/halt-on-error. The two new suites cover staging, malformed headers,
all segment types, address boundaries, selected banks, hashes, unknown gaps,
allocation rollback, protected responses, snapshot identity and untouched IPC.
All 48 reconciled predecessor suites remain. Six completed paired startup scenarios
match stdout, exits and their newly created file bytes. Two foreground multi-case
attempts reached tool time limits; partial roots/logs remain, and uncompleted cases
are not counted as passing. No native test failure occurred in final builds.

All 80 capture files match GCC to Clang. Before/after DSP inspection, the complete
IPC, input image, GPU register image, PICA uploads, GSP/HID pages and epochs, mapped
HID page, full VRAM and captured thread/register/timing state are unchanged.
Captures use a logging-only alternative runner; ordinary CLI runs reach the same
stop without it. Python independently reproduces both staged banks/known masks.

Code.bin, raw RomFS, all 603 private AOT backup members and the original 2048-byte
ExHeader were reverified. Full IVFC-chain verification was not repeated. No Windows
or macOS build was performed. VRAM is still blank. No meaningful screenshot,
rendered frame, main menu, DSP execution, audio or gameplay has been demonstrated.

## References

Azahar pin: `86a9f9236ae42bb5a2b995dbc933d599d8ea07ac`.
`src/audio_core/lle/lle.cpp`, blob `388fe64ec1a5130a2c93a5dfa04ca84df109b567`:
DSP1 record layout, bank placement, special TODO, startup replies and pipe-base wait.
`src/audio_core/hle/hle.cpp`, blob `05ff9d74e1077595faba67a61a7ce4581a80d374`:
HLE component boundary in an existing audio backend.
3dbrew, DSP Binary: https://www.3dbrew.org/wiki/DSP_Binary (read October 6, 2026).

Next: implement justified special-segment data handling and a DSP boot/backend
that genuinely produces the required responses. Do not mark the image loaded
merely because parsing/staging succeeded. Keep raw firmware and captures private.
