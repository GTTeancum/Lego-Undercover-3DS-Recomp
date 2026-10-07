# LEGO Chase Begins — canonical continuation handoff

October 6, 2026. Continue in assistant scratch, NOT the user's PC or Work.
LEGO City Undercover: The Chase Begins (Nintendo 3DS USA). Headless native startup
reconstruction, not playable. POST UPDATED DOWNLOADABLE MARKDOWN AND COMPLETE
SOURCE/EVIDENCE AT THE END OF EVERY WORK TURN. No logo/title/main menu/rendered
frame or audible game output exists. Only present meaningful genuine game visuals.

## Source and preservation: use this checkpoint, not an older pending variant

This source extends published main b0ba35cbd462ce9f3e0a7345023cd8c1e03a86c0,
whose exact baseline tree is6934e92465d0ee41cab127cfbbae6c16b7e17e4c. The current
commit and tested tree, CI and backup receipts are appended AFTER real publication.
Local Git is a reconstructed snapshot/index, not remote history. Read main before
publishing and use its actual parent without force. Do not overwrite concurrent work.

The task arrived with DSP-SEMAPHORE-PENDING, tree e8bd052cc42832538522f08de3824355ea325e39.
Its1120 manifest entries and455 source blobs were verified and restored separately.
Newer main already contained equivalent semaphore work with a different notifier
API. Its1059 manifest files and453 source blobs matched the published tree. This
checkpoint preserves/extends that newer main; the two notifier designs were NOT
silently merged. The old pending archive/MD were newly saved to Library as
/LEGO-Chase-Recovery/Preserved-DSP-SEMAPHORE-PENDING.tgz and .md. Retain them separately.

No CCI extraction, user reupload, user-PC or Work operation was needed. Existing
code/AOT archive were verified; generated2/ was re-extracted and prepared RomFS
parts reassembled into a new file. Baseline GCC full build/all58 tests reproduced
MMIO0x2A0 in the first scheduled DSP slice. That previous work is inherited, not new.

## New result: actual reference transmitter output, NOT a visible game boot

Two new explicit options:
--dsp-transmit-profile reference-stereo
--dsp-audio-mode capture

The original firmware now passes its transmitter control/setup writes, places16
zero-valued words into the real FIFO and emits8 stereo frames from those16 words.
Two scheduled DSP slices complete. The third stops on strict FIFO underrun:

```
dsp_transmit_profile=reference-stereo irq=reference_fifo_empty hardware_format=unverified
dsp_audio_mode=capture frames=8 underflow=stop playback=none capacity=4096
dsp_live_loaded=1 data_base=0x1ff40000 scheduled_slices=2 next_deadline_ns=18752595
dsp_live_error=BTDMP transmit underrun output is unmodeled
DSP completed47273 attempted47274 written9726; PC0x4BAF
cpu_ticks=5027793 core0_instructions=2287384 core1_instructions=0
one display period; guest_now_ns=18752595
stop=UnsupportedDspEvent pc=0x002594c4 thread=1 dispatch_rounds=259
```

The original startup message remains queued: CPU-to-DSP audio slot5 read0/write4;
DSP-to-CPU audio slot4 read0/write0. Both the game's audio event and retained DSP
semaphore event remain unsignaled. No startup reply, completed message consumption,
audio mixing, music, playback, HID sample, draw, visible logo/menu or gameplay is
established. All6MiB VRAM remains zero. Do not show silent/blank outputs as progress art.

Optional second diagnostic: --dsp-audio-mode capture-reference-silence.
This preserves the pinned fallback for missing FIFO words, but EVERY absent channel
is tagged separately from firmware output. Original run reaches capture capacity4096:
8 genuine FIFO frames +4088 tagged underrun frames, all zero. Completed slices1024;
ARM round274/time143659030ns/core0 instructions2289379/core1 instructions99. The
DSP still loops at0x4BAF and the audio pipes/events have not progressed. The capacity
fault is a guard, NOT the functional startup blocker. Do not raise the limit and
claim progress, mark fallback silence as firmware stores, or pretend99 worker
instructions means gameplay. The strict-capture path still has0 core1 instructions.

Without transmit profile, the earlier unconfigured0x2A0 stop remains. With profile
but no capture consumer, output fails at the first callback. Known-only mode still
faults on unknown SRAM at962 calls. Keep these branches distinct and explicit.

## Bounded reference contract and uncertainties

MMIO0x2A0 is transmit control. Only reset5 and observed0xF are accepted under the
explicit reference profile; clock0 or0x1004 and disabled companion preset4,0x21,0,0,0
are bounded. Reads use documented companion masks. Transmit enable is bit15.
Other format/mode values and active format replacement fail closed. Companion fields
are not fully decoded hardware functions. This is NOT arbitrary MMIO storage.

Teakra notes and GBATEK leave control IRQ fields uncertain. An early prototype used
a speculative bit8 gate; it was removed before final tests. Final reference profile
retains the pinned interpreter's FIFO-empty interrupt after a genuine pop empties
its queue. It does not decode an invented IRQ-enable bit. ICU routes port0/1 to11/12.
Threshold/held-line/arbitrary-format behavior and actual 3DS timing remain unverified.

Real16-word FIFO: signed16-bit values in FIFO order, not newly measured L/R wiring.
Overrun fails, strict incomplete stereo output fails before partial pop. Optional
underflow fills absent channel values with tagged reference silence, never firmware
provenance. Empty fallback alone cannot raise an empty-transition interrupt. Flush
bit2 is write-one; zero writes retain data; lowbits0..1 read/write. Reset clears
hardware fields/queue/timer and retains explicit host policies/callbacks only.

Inherited external-clock reference is4096 interpreter ticks per output, not a newly
measured sample divider. Tick/Skip respect every observable output/underrun/deadline.
Capture stores samples, two-bit FIFO source mask and interpreter attempt count in a
fixed4096-frame buffer. Overflow never silently drops samples. No host playback or
unbounded file/thread output is added. Callback failures can leave FIFO/IRQ effects;
the inherited probe seals the device rather than retrying or faking rollback.

Captured output is connected to the SAME live interpreter/SRAM and firmware-created
pipes. The original load handshake, special532-byte configuration policy, known-byte
tracking, memory mapping, semaphore service, CPU scheduling, GPU and all game/AOT
bytes are retained. No game opcode, task pointer, pipe read pointer, wait result or
GPU completion was patched to force progress. No software-rendered frame exists.

## Validation and current evidence

Full GCC and Clang builds link all599 unchanged AOT page units. All60 configured
CTest suites pass under both compilers and under ROM-free Clang ASan/UBSan. The
prior58 suites remain. New suites test every16-bit sample word, nonzero/signed FIFO
values, exact deadlines and skip equivalence, reset/disable/flush, overrun/underflow,
malformed setups, genuine synthetic DSP instructions, tag provenance, capture limits,
continuation, missing consumers and option constraints. Synthetic tests are not
original-firmware output or independent DSP CPU oracles.

Five complete original-startup pairs and11 CLI rejection pairs match between
compilers. Strict and fallback diagnostic captures have173/211 matching files.
Independent parsing verifies actual captured sample tags, live descriptors, queued
message, unsignaled events and blank VRAM. In the first three slice attempts,
captured ARM CPU/time/IPC/GPU/HID bytes stay unchanged across DSP execution; all
changed DSP SRAM bytes carry firmware-write provenance. No full ARM-space dump.
Code, whole raw RomFS, ExHeader and all603 AOT members were reverified. Full IVFC
block verification was not repeated. No Windows/macOS or audio playback test.

Work/evidence: /mnt/data/lego_recovery/dsp-btdmp-checkpoint/.
Important: tests.json and JUnit/logs; validation-normal.json (5 complete cases),
cli-proof.json (11); capture-strict-capture/ and capture-reference-silence/;
proof.json/verify_final.py; run_validation.py; make_captures.py plus self-contained
inherited_trace_support.inc; references.json/setup-notes.txt. Logging-only trace
main/runner link the same production libraries. Raw traces/disassembly stay PRIVATE.
Early control/bit8 prototypes, compile errors and one interrupted combined matrix
remain distinct from corrected final output. The unfinished case-unconfigured/
directory is not counted; unconfigured-complete is the finished replacement.
The standalone probe_transmitter.cpp supplied the observed startup inputs directly;
its logs are a separate experiment, NOT original ARM startup proof.

## Next exact work

Trace the original DSP firmware's task/interrupt/pipe dispatch after transmitter
initialization. Its self-loop0x4BAF restores interrupt enable before looping. The
reference FIFO-empty IRQ does execute additional firmware writes, but no pipe reply
or refill follows. Investigate pending APBP command/semaphore ordering and firmware
task wake behavior against the pin, without forcing IRQs, changing pointers or
hardcoding a response. An explicit silence policy proves output underrun alone is
not the remaining startup problem. Do not treat capture capacity as the next feature.

Preserve strict/unconfigured and reference comparisons and all current guards.
Inspect the real mailbox/ICU/core flags using logging-only code before changing
semantics. Current private probe_logged.cpp / inspect_wait.py and disassembler
sources are available; prototype logs predate the final reference-IRQ correction.
Create NEW logs when rerunning. Keep HID sampling, FCRAM/AHB, audio mixing/playback,
DSP unload/reload and GPU drawing limitations explicit. No additional upload needed.

Primary refs: wwylele/teakra@3d697a18df504f4677b65129d9ab14c7c597e3eb,
btdmp.cpp blob20a1f09458cff63ef5d1ce3d19b64763b57bcb1a;
btdmp.md blob7daf7f8054e3513993c4edcf6863576fc1574c3d;
Martin Korth GBATEK no$gba3.03 printed365–367 (PDF390–392 zero-based).
Parsed text was accessible; requested web screenshots failed cache retrieval.
No new hardware measurement or image/table screenshot validation is claimed.
Current vendor patch hashes/reference assumptions are in UPSTREAM.json.

## Rebuild and reproduce

Root /mnt/data/lego_recovery/. Current source repo/. Builds build-gcc/, build-clang/,
build-asan/. Private AOT generated2/; code restored/code.bin; raw RomFS
 game/prepared-romfs/romfs.bin; ExHeader prepared-launch/exheader.bin.
Existing raw parts/manifest romfs-library-roundtrip/. Historical source variants:
restored-semaphore-pending/ and restored-main-b0ba35c/. Never extract them over repo/.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 3
ctest --test-dir build-gcc --output-on-failure
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/btdmp-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty --romfs game/prepared-romfs/romfs.bin --gpu-vram-mode reference-zero --display-clock-mode reference-idle --cfg-profile reference-stereo --cpu-mode diagnostic-dual --block-limit 100000000 --exheader prepared-launch/exheader.bin --dsp-special-profile empty-config --dsp-executor live-teakra --dsp-reset-profile reference-zero-data --dsp-transmit-profile reference-stereo --dsp-audio-mode capture
```

Expected exit3, strict underrun after2 complete slices, original ARM round259.
Replace ONLY capture with capture-reference-silence for the tagged comparison.
Never silently enable/drop the explicit CPU/VRAM/PTM/CFG/DSP/reference policies.
For Clang use clang++; for sanitizers omit AOT, use Debug plus
-fsanitize=address,undefined -fno-omit-frame-pointer; leak checks and halt-on-error.
Capture drivers refuse existing output directories. New paired roots contain only
owned test gamecoin data, not recovered NAND. Delete only your own just-created,
byte/hash-verified20-byte coin when matching compiler fresh runs; preserve unknown saves.

## Durable recovery and mandatory delivery

GitHub GTTeancum/Lego-Undercover-3DS-Recomp main; Library /LEGO-Chase-Recovery/.
The new archive contains ALL indexed current source plus current private evidence,
prepared launch, source index, manifest/verifier and an exact patch from b0ba35c.
Verify CHECKPOINT-MANIFEST.json before extraction and reconstruct SOURCE-INDEX.json
modes/blobs including ignored tracked reports. Do not invent Git history or force
an older snapshot onto main. The appended receipt names actual publication/backup.

Separate private backups: code.bin; LEGO-Chase-current-AOT-599pages-2026-10-03.tgz
(unpacks generated2/); Prepared-RomFS/ two raw parts+manifest; Prepared-Launch/exheader.bin.
Parts402653184+366526464 bytes; reassemble with tools/restore_romfs_parts.py, refusing
existing output. Raw769179648 bytes; native view offset4096/size769175552. Preserve
integrity tables. No original CCI extraction is required.
Code SHA5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
AOT SHA2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9.
RomFS SHA6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf.
ExHeader SHAd7641f0a3bb89697ca8f0751e88d31703b54aa185ac78f9e74e41169dfcdc004.
DSP49716-byte SHA7ea3c44a1c57514bebbebce8a7995f7f3a290170ea3b6c145749ad9358672c97.

Raw DSP/SRAM/ExHeader/disassembly and captures must NEVER enter public GitHub.
Original code/AOT/raw RomFS/CCI, .git, native binaries/build objects and font files
are excluded from the new source/evidence archive. Prior checkpoints remain separate,
not recursively copied. Scratch may reset; attachments/Library/GitHub are recovery
routes, not permanent scratch. Append actual CI/publication/round-trip receipts only
after success. POST UPDATED DOWNLOADABLE MARKDOWN AND SOURCE/EVIDENCE EVERY WORK TURN.
