# LEGO Chase Begins — canonical continuation handoff

October 7, 2026. Continue in assistant scratch, NOT the user's PC or Work.
LEGO City Undercover: The Chase Begins, Nintendo 3DS USA. Native/headless startup
reconstruction, NOT playable. POST UPDATED DOWNLOADABLE MARKDOWN AND A COMPLETE
SOURCE/EVIDENCE CHECKPOINT AT THE END OF EVERY WORK TURN. Only show meaningful genuine
game output. No rendered logo, title screen, main menu or useful screenshot exists.

## Canonical source and preservation

This checkpoint extends published `0799795e30b277ad1a6cc7c05ac624c3110171eb`, exact
baseline tree `a8888c11916da750bef68d10c3a67eecd3cca3e4`. The appended delivery receipt
identifies final publication, exact tested tree, hosted tests and backup locations.
Local Git is a reconstructed index/snapshot, NOT upstream history. Re-read remote
main before publishing, reconcile concurrent changes, and never force-push a snapshot.

The attached SOUND-SWIZZLE-PENDING variant was verified first: 1,757 manifest files,
471 indexed source blobs, tree836c9e7fcc37c512b3860a1827e84cfad22428da. It is preserved
unchanged separately in scratch and Library as Preserved-SOUND-SWIZZLE-PENDING.tgz/.md.
Its source was NOT overlaid on the newer canonical implementation. The newer Library
archive 0799795 verified 878 manifest entries/471 source blobs and the exact tree.
Its archive SHA256 is d4fca7d264bf245bb458c0c3f2d2d0c122fb776a8114305e35c6a4a93decef11.
The old alternate archive SHA256 is 75f642afc56c86cee10cf1b07b9969e6d01c247787852f83ed40cb6eca021a5f.

Baseline full GCC build/all65 tests reproduced the uncommitted LUT request at round357.
Original code/AOT archive and prepared RomFS parts were mounted; AOT was re-extracted
and RomFS restored into a NEW file with hash checking. Prepared ExHeader came from
the canonical checkpoint. No CCI extraction, user reupload, user-PC or Work access.

## Actual original-game progress

The original 23,360-byte graphics initialization list at round357 now commits fully:
5,683 register writes;83 swizzle words;512 procedural-table words;one requested P3D
relay entry and the real queue advance. Four procedural banks receive128 words each.
Independent packet replay matches all1,842 register words and all new LUT/swizzle
words/known flags. The previous83-word staged prefix is now genuinely committed,
not merely a disposable plan. This is GPU initialization, NOT shader execution/drawing.

The game continues into original file reads. The old bounded memory audio consumer
fills at round492; that is a diagnostic capacity limit, not a missing game function.
New explicit `--dsp-audio-file NEW_FILE` consumes every actual emitted frame to disk
without increasing the memory prefix, changing emulated time or discarding samples.
It requires explicit live capture/reference-transmitter/diagnostic-dual selections.

With that file sink the exact next stop is:

```
stop=UnsupportedIpc pc=0x0025947c detail=0x32 thread=7 dispatch_rounds=569
last_ipc_session=cfg:u request_header=0x00010082
ipc_words=00010082 00000001 000a0002 0000001c 0e01ff18 ...
cpu_ticks=42554166 core0_instructions=12856209 core1_instructions=99
quota_transitions=140 display_periods=9 guest_now_ns=158717957
DSP scheduled_slices=1148 notification_wait_slices=1 completed/attempted=18857984
```

This is CFG language block0x000A0002, one byte to0x0E01FF18. The request, CPU and
output plus captured neighbours are untouched. Seven threads/37 handles exist.
Do not claim a language value, completed audio initialization or gameplay.

Audio file:4,598 complete stereo records, all actual FIFO source mask3 and all-zero
startup samples. Memory retains the first4,096 records as a diagnostic prefix.
No fallback frames, music/effects or audible playback on the observed new path.
VRAM is NOT wholly zero anymore: its ONLY nonzero bytes are an819,200-byte depth
clear at offset0x419400, repeating little-endian0x00FFFFFF. This is not an image.
No rendered frame/logo/menu, game-shader execution or HID samples have been demonstrated.
Never present clear patterns or silent samples as worthwhile screenshots/audio.

## Procedural lookup upload contract

Configuration register0xAF: cursor bits0..7, table selector bits8..11. Tables are:
0 noise128;2 color-map128;3 alpha-map128;4 color256;5 color-difference256.
Data ports0xB0..0xB7 consume raw parameters regardless of their register byte mask.
The masked register mirror still updates independently. Table address is cursor
modulo capacity; cursor increments/wraps at8bits, preserving selector and other bits.
Explicit per-entry written flags distinguish uploaded zero from unknown storage.
Reserved selectors stop the disposable plan, rather than inventing a table.

Whole-list and whole-queue failure atomicity remain: a valid upload prefix followed
by a draw, chain, fog upload or unsupported queue packet does not alter live tables,
registers, queues or interrupts. Existing default/immediate-attribute guards remain.
No renderer or procedural-texture evaluator is implemented here.

## Diagnostic audio file contract

The optional DspAudioSink receives each real DspCapturedAudioFrame synchronously.
The original finite in-memory mode remains unchanged when no sink is selected.
With a sink, the first4096 frames remain inspectable; emitted_audio_frames is the
separate total. Every later accepted frame must be written by the sink. Failure
seals the DSP with possible partial FIFO/external-file effects; no retry/rollback
or replacement silence is claimed. Attaching a sink does not enable reference silence.

--dsp-audio-file is exclusive-create only. Existing files and symlinks are refused,
not overwritten. No parent directories are invented. Stdio is flushed each record;
short writes, flush/close errors and the64MiB record-data guard are explicit failures.
Partial output files are retained as evidence. This is not fsync/power-loss durability.
The file is closed after runner.Run and close failure is reported. No background
thread, host playback, mixer, sample-rate conversion or invented waveform is added.

Format DSPAUD1:32-byte header, followed by16-byte little-endian records. Header is
8-byte `DSPAUD1\0`,u32 version1,u32 record-size16,then16 zero bytes. Record:
s16 channel0,s16 channel1,u8 FIFO-source mask,three zero bytes,u64 interpreter attempt.
No measured sample rate/physical L-R wiring is asserted; this is deliberately NOT WAV.
Both source-mask bits are retained, including any EXPLICIT reference-silence policy.
The strict observed run uses mask3 throughout. Host I/O affects wall-clock speed,
not guest cycle charges. Windows code path exists but has NOT been tested.

## Validation and evidence

Full GCC/Clang native builds link all599 unchanged private AOT page units. All67
CTest suites pass on both compilers and under ROM-free Clang ASan/UBSan with leak
checking/halt-on-error. Earlier65 suites remain. New tests cover all upload ports,
masks/selectors, cursor/table wrap, raw nonzero values, known flags, continuation,
whole-batch rollback, retained guards, exclusive output creation, exact tagged record
encoding, real executor delivery beyond the prefix, strict underrun and terminal
sink failure. Linux test wrappers inject partial fwrite, fflush and fclose failures.

All194 final capture files match GCC to Clang. Independent Python replays the full
command list's register effects, LUT and swizzle state, queue/P3D append; checks the
next request/output unchanged; and verifies the exact depth clear and complete
4,598-record file against the retained prefix. This is NOT an independent CPU
emulator or complete ARM-address-space dump. No Windows/macOS, hardware-timing,
playback or renderer test. Original-input and ordinary-matrix receipts are included.
Six ordinary startup compiler pairs and six invalid-option pairs match. Original code,
whole raw RomFS, ExHeader and all603 AOT members were reverified unchanged. Full IVFC
block validation was not repeated. Hosted results and exact delivery identities
are appended only after verification.

Current evidence /mnt/data/lego_recovery/lut-checkpoint/: proof.json and
verify_captures.py; tests.json/configured-*.json/junit-final-*.xml/test-final-*.log;
matrix.json and pair-*/verified.json; capture-pair/{gcc,clang}; run_pair.py,
validate_all.py, make_capture.py/inherited_capture_support.inc; identity.json;
build/configure logs; references.json; setup-notes.txt. Alternate capture sources
only add logging, link production libraries and independently reproduce the normal
CLI stopping point. Original firmware/list/SRAM/sample captures stay PRIVATE.

A combined retest tool call was interrupted during ASan after adding stdio fault
tests; it is not a completed pass. The separately collected final ASan run passed
all67. All initiated jobs are collected before delivery. Direct codeload DNS and
streaming container sessions were unavailable; Library source recovery succeeded.

## Next exact work

Establish an explicit language profile/source for CFG block0xA0002 and implement
its one-byte read with correct output descriptor/permission/alias preflights. Do
not infer language from camera/sound settings or pretend to recover console NAND.
Rerun unchanged code from new owned state and follow the actual next request.
Keep the file sink selected to avoid mistaking old finite-capture capacity for a
startup blocker. Do not increase caps, drop samples, force GPU completion or change
game/firmware code to fabricate progress. Meaningful visuals remain the goal.

All earlier profiles stay explicit: PTM empty history, reference VRAM/display,
camera CFG, sound choice, verified ExHeader Multi/max30, diagnostic-dual CPU,
special missing-config fallback, reference DATA reset/transmitter, reference-slice
boot and capture. The singular `reference-slice` is canonical. ARM one recorded
instruction/core/tick, synchronous DSP boot/waits and slice scheduling remain
reference diagnostic conventions, not measured hardware latency or parallelism.

Primary PICA pin: azahar-emu/azahar@86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
regs_texturing.h blobeb190faff9a4d3c4c042ca1457b90ade8340de1c;
pica_core.h blob62b04956bbf4795e09437b5463ee28904b29a30a;
pica_core.cpp blob910ebc2021d8b546a79309ddfd4e080049c43043.
No new hardware measurement/PDF analysis. The sink is a host facility, not firmware.

## Scratch and exact reproduction

Root /mnt/data/lego_recovery/. Source repo/. Full build-gcc/,build-clang/; ROM-free
build-asan/. Private AOT generated2/; code restored/code.bin; raw RomFS
 game/prepared-romfs/romfs.bin; ExHeader prepared-launch/exheader.bin; raw parts and
manifest romfs-library-roundtrip/. Current evidence lut-checkpoint/.
Canonical baseline restored-0799795/; alternate preserved-sound-pending/ remain
separate. Never extract historical source over current repo/.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/language-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty --romfs game/prepared-romfs/romfs.bin --gpu-vram-mode reference-zero --display-clock-mode reference-idle --cfg-profile reference-stereo --cfg-sound-mode stereo --cpu-mode diagnostic-dual --block-limit 100000000 --exheader prepared-launch/exheader.bin --dsp-special-profile empty-config --dsp-executor live-teakra --dsp-reset-profile reference-zero-data --dsp-transmit-profile reference-stereo --dsp-audio-mode capture --dsp-boot-mode reference-slice --dsp-audio-file "$NEW_ROOT/audio.dspaud"
```

Expected exit3, CFG language request round569. Without ONLY file option/value the
finite memory consumer stops at round492. Omitting sound stays at round295. Keep
profiles distinct. Use clang++ for Clang; omit AOT for sanitizer builds, Debug-O1
with -fsanitize=address,undefined -fno-omit-frame-pointer and matching linker flags.
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1; UBSAN_OPTIONS=halt_on_error=1.

Capture/pair drivers refuse existing output directories. New lut-owned.*,lut-first.*, 
lut-stream-first.* and lut-baseline.* are owned test roots, not console NAND. Drivers
only delete their own just-created hash-verified20-byte gamecoin/audio file to pair
compilers on identical paths. Preserve unknown saves and evidence.

## Durable recovery and every-turn mandate

GitHub GTTeancum/Lego-Undercover-3DS-Recomp; Library /LEGO-Chase-Recovery/.
Verify CHECKPOINT-MANIFEST.json before extraction, then SOURCE-INDEX.json exact
paths/modes/Git blobs, including tracked ignored reports. Complete source/current
private evidence, prepared launch, parts manifest, exact patch and verifier are in
the new checkpoint. Historical archives stay separate, not recursively embedded.
Restore source indexes as snapshots, never invent upstream ancestry or force-push.

Separate original Library inputs:code.bin;LEGO-Chase-current-AOT-599pages-2026-10-03.tgz
(unpacks generated2/);Prepared-RomFS/two raw parts+romfs-parts.json;
Prepared-Launch/exheader.bin. Parts402653184+366526464 restore through
repo/tools/restore_romfs_parts.py into a NEW file. Raw RomFS769179648 bytes,
native view offset4096/size769175552. Preserve integrity tables. No CCI extraction.
Code SHA5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
AOT SHA2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9.
RomFS SHA6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf.
ExHeader SHAd7641f0a3bb89697ca8f0751e88d31703b54aa185ac78f9e74e41169dfcdc004.

Raw original PICA lists, DSP/SRAM/sample captures and ExHeader NEVER enter public
GitHub. No code.bin/private AOT/rawRomFS/CCI/.git/build binaries are embedded in the
source/evidence archive. Scratch may reset; attachments/Library/GitHub are recovery
routes, not permanent scratch. Record publication, CI and saved-file verification
only after success. POST UPDATED DOWNLOADABLE MARKDOWN AND RECOVERABLE SOURCE/EVIDENCE
AT THE END OF EVERY WORK TURN.
