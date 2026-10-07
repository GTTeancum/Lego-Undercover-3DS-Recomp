# BTDMP transmitter and bounded tagged audio capture

October 6, 2026. Parent main b0ba35cbd462ce9f3e0a7345023cd8c1e03a86c0.
This checkpoint implements a bounded, explicitly selected reference transmitter.
It does not establish a visible game boot, music, audio playback or gameplay.

## Original-game result

With `--dsp-transmit-profile reference-stereo --dsp-audio-mode capture`, original
DSP firmware passes MMIO0x2A0 and its companion setup writes. It writes sixteen
zero-valued words to the actual transmit FIFO. Two scheduled DSP slices complete,
eight stereo frames are captured from those sixteen words, and the next slice
stops on strict FIFO underrun. The FIFO-empty interrupt uses the pinned backend's
actual empty transition; no host audio-completion event is substituted.

At the stop: ARM round259, time18752595ns, core0 issued2287384 and core1 issued0.
DSP completed47273/attempted47274 Run(1) calls, written9726 words, PC0x4BAF.
The audio pipe's CPU-to-DSP read pointer remains0 and write pointer4. The DSP-to-CPU
pipe remains empty. Its retained audio event and semaphore event are unsignaled.
The original audio-startup message has NOT been fully consumed/answered.
All6MiB VRAM remains zero; no visible frame, logo, title screen or main menu exists.

An explicit second comparison uses `--dsp-audio-mode capture-reference-silence`.
It reaches the bounded capture capacity4096: eight real FIFO frames plus4088 tagged
underflow frames. All values are zero. It completes1024 slices but still receives
no audio-startup response; DSP execution remains at its original self-loop0x4BAF.
Core1 executes99 original recorded instructions on this longer diagnostic path.
This is NOT functional audio progress or proof that raising the capture limit would
solve startup. The next work is tracing the DSP firmware task/interrupt/pipe wake
sequence. Never force the read pointer, complete the ARM wait or fill a reply pipe.

## Reference profile and limits

The new transmitter profile is opt-in. Without it, the earlier first-slice stop
at the unconfigured0x2A0 control remains; existing commands do not silently select
new hardware assumptions. Only control reset5 / observed0xF, clock0x1004 and the
five documented companion preset writes4,0x21,0,0,0 are accepted. Companion reads
apply the documented masks. Active format replacement and other presets stop.

GBATEK and Teakra notes leave portions of the control layout uncertain. This does
NOT claim a newly verified IRQ-enable bit: an early speculative bit8 gate was
removed after checking the actual pinned backend. The selected profile retains its
FIFO-empty interrupt behavior. Port0/1 route to ICU11/12. Other format/threshold/
held-level semantics remain unmodeled. No new 3DS hardware test was performed.

The real sixteen-word FIFO preserves order and signed16-bit sample values. Writes
fail on overrun; strict underflow faults before popping an incomplete stereo pair.
The optional reference fallback tags absent channel words rather than calling them
firmware output. Only a genuine pop-to-empty can signal the FIFO interrupt.
Flush bit2 is write-one, bits0..1 are retained; zero writes no longer discard data.
Enable uses bit15. Reset clears configuration, FIFO/status and timer, retaining only
host callbacks and explicitly chosen policies. Sample order is FIFO order, not a
newly measured left/right wiring result.

The inherited external-clock reference is4096 interpreter ticks per stereo output,
not an inferred hardware divider. Tick/Skip now share observable deadlines: skipping
cannot suppress an output, underrun or capacity fault. Missing consumers fail closed.
Consumer/interrupt exceptions can retain partial FIFO/IRQ effects; the existing live
probe seals the faulted device rather than retrying or claiming rollback.

Captured frames remain in a fixed4096-frame host buffer with two signed samples,
a two-bit source mask and the interpreter attempt count. No callback allocation,
unbounded file output, playback thread, silent dropping or generated game content
is introduced. A frame marked as underflow is never represented as a DSP store.
The capture option requires live-teakra and the explicit transmit profile. Timing,
known-memory policies, original firmware handshake and live memory stay inherited.

## Verification

Full GCC/Clang native executables link all599 unchanged AOT units. All60 configured
suites pass with each compiler and all60 ROM-free Clang ASan/UBSan suites pass.
The earlier58 suites remain. New tests cover every16-bit sample value, nonzero and
signed values, FIFO capacity/flush, exact deadlines and skip equivalence, disabled
state, reset, malformed/active setup, genuine synthetic-DSP output, tagged underflow,
consumer/capacity errors, continuation and CLI/runner option guards.

Five ordinary original-startup pairs and eleven CLI-rejection pairs match between
compilers. Strict-capture and reference-silence captures have173 and211 matching
files respectively. Independent parsing checks source tags, all pipe descriptors,
unchanged queued message, unsignaled events and blank VRAM. Each of the first three
DSP slice attempts preserves captured ARM/IPC/GPU/HID state while DSP writes carry
firmware-write provenance. These are diagnostic comparisons, not independent CPU
oracles. No full ARM-space dump or new hardware-cycle measurement is claimed.

Original code.bin, raw RomFS, ExHeader and all603 AOT archive members match their
verified identities. Full IVFC verification was not repeated. No Windows/macOS
build or host audio playback was performed. Hosted publication/tests are recorded
separately in the final receipt, only after observed completion.

## Recovery and provenance

The task arrived with an unpublished semaphore variant, source tree e8bd052c.
It was verified and preserved separately. Fresh main contained equivalent published
work with a different notifier API; this checkpoint extends that newer main rather
than overwriting it or claiming both notifier designs were merged.

Primary references: wwylele/teakra@3d697a18df504f4677b65129d9ab14c7c597e3eb,
src/btdmp.cpp and src/btdmp.md; Martin Korth GBATEK no$gba3.03, printed365–367.
Exact hashes, masks, reference assumptions and unsuccessful PDF image retrieval
are recorded in vendor/teakra-3d697a1/UPSTREAM.json and private references.json.
Private evidence: dsp-btdmp-checkpoint/. The raw original firmware, disassembly,
SRAM/ExHeader captures and test roots belong only in the private recovery archive.
