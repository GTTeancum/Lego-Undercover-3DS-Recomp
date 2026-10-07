# Procedural lookup uploads and lossless diagnostic audio consumption

October 7, 2026. This source extends canonical 0799795e30b277ad1a6cc7c05ac624c3110171eb.
Work was performed in assistant scratch, not the user's PC or Work.

## Original-game result

The original 23,360-byte graphics initialization list now completes at round 357.
It performs 5,683 register writes, including 83 swizzle words and 512 procedural
lookup words. Four tables receive 128 words each: color map, alpha map, color and
color difference. Independent raw-packet replay matches all 1,842 register words,
both swizzle banks/known flags, all five procedural banks/known flags, the queue
advance and one requested P3D relay entry. These effects COMMIT to the live GPU
state; they are no longer merely a prefix in a rejected plan. No draw is implemented.

Without a new audio-file option, startup next reaches the existing 4,096-frame
capture capacity at round 492. This is an artificial diagnostic-storage limit, not
a missing game operation. The original memory-capture behavior remains available.

With explicit `--dsp-audio-file NEW_FILE`, every emitted stereo pair and its FIFO
provenance are synchronously written to a new diagnostic file. The first 4,096
records remain available as an in-memory prefix, but every later record is consumed
by the file sink rather than dropped. No waveform, interrupt, FIFO word or timing
change is synthesized. Strict underrun remains an error. Existing paths are never
overwritten. Short writes, flush/close errors and a 64 MiB record-data guard fail
explicitly; partial external output can remain and is not retried as a rollback.

The unchanged original game now stops at round 569 on thread 7:

```
stop=UnsupportedIpc pc=0x0025947c detail=0x32 thread=7 dispatch_rounds=569
last_ipc_session=cfg:u request_header=0x00010082
ipc_words=00010082 00000001 000a0002 0000001c 0e01ff18 ...
```

This is the one-byte CFG language block 0x000A0002. Request, CPU state and destination
plus captured neighbours remain untouched. No language preference is guessed.
There are seven threads and 37 handles. Core0 has issued 12,856,209 recorded
instructions and core1 99. Guest time is 158,717,957 ns, with nine display periods;
DSP has completed 1,148 scheduled slices plus its inherited synchronous pipe slice.
These are diagnostic execution counts, not measured hardware timing or progress percent.

The file contains 4,598 stereo records, all genuine FIFO-sourced startup silence.
There is no music, sound-effect or audible-playback claim. VRAM is no longer wholly
zero: its ONLY nonzero content is the original 819,200-byte depth-clear span filled
with 0x00FFFFFF. That is not a rendered frame. No visible logo, title screen, main
menu, HID sample producer, shader execution or playable gameplay is demonstrated.

## Implementation bounds

Procedural table selector is bits 8..11 of register 0xAF; cursor is bits 0..7.
Selectors 0/2/3 address 128-entry noise/color-map/alpha-map tables; 4/5 address
256-entry color/color-difference tables. Raw upload words from ports 0xB0..0xB7
are independent of the masked register mirror. Each write addresses the table
modulo its size and advances the eight-bit cursor with wrap. Selector and other
configuration bits are preserved. Written flags distinguish uploaded zero values
from unknown entries. Reserved selectors reject the disposable plan. Fog, draw,
command-chain and default/immediate-attribute guards remain intact.

The audio file format is DSPAUD1, not WAV: no sample rate or physical channel map
is asserted. A 32-byte header is followed by 16-byte little-endian records:
signed16 channel0/channel1, one-byte FIFO-source mask, three zero padding bytes,
and a 64-bit interpreter attempt counter. The source mask survives both strict
and explicitly selected reference-silence policies. File consumption adds no
emulated cycles; synchronous host I/O may affect host speed. Stdio flushing is
not a power-loss durability/fsync guarantee. Playback remains unimplemented.

## Validation

Full GCC and Clang builds link all 599 unchanged private AOT page units. All 67
CTest suites pass on each compiler and under ROM-free Clang ASan/UBSan with leak
checking and halt-on-error. Previous 65 suites remain. New tests cover all data
ports/masks/selectors, cursor/table wrap, exact known flags, nonzero payloads,
whole-batch rollback, retained GPU guards, exclusive output creation, record
encoding, finite bounds, actual executor delivery beyond the prefix, strict
underrun preservation and terminal sink failure. Linux test-only stdio wrappers
exercise partial write, flush and close failure paths without touching devices.

All 194 final capture files match between GCC and Clang. Replay checks actual
committed upload state, queue/P3D effects, the untouched next CFG request and exact
depth-clear bytes. File records match the retained prefix and all source masks.
This is not an independent CPU emulator or a complete ARM address-space dump.
Six ordinary startup pairs and six invalid-option pairs match. Original code, full
raw RomFS, ExHeader and all603 AOT members retain their verified identities. Full
IVFC checking was not repeated; identity receipts are in the private checkpoint.
No Windows/macOS, hardware timing, playback or renderer test was performed.

## Provenance and continuation

PICA primary pin: azahar-emu/azahar at 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
`regs_texturing.h` blob eb190faff9a4d3c4c042ca1457b90ade8340de1c;
`pica_core.h` blob 62b04956bbf4795e09437b5463ee28904b29a30a;
`pica_core.cpp` blob 910ebc2021d8b546a79309ddfd4e080049c43043.
The sink is a new host diagnostic facility, not a recovered console peripheral.

Next establish an explicit language profile/source for CFG block 0xA0002, implement
its one-byte read with existing output/alias preflights, and follow unchanged code.
Do not infer language from the camera/sound settings or claim recovered NAND.
Raw firmware, PICA lists, SRAM, sample captures and ExHeader remain private.
