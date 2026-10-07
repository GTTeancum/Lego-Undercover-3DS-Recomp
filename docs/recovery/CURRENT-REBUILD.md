# LEGO Chase Begins — canonical continuation handoff

October 7, 2026. Continue in assistant scratch, NOT the user's PC or Work.
LEGO City Undercover: The Chase Begins, Nintendo 3DS USA. Native/headless startup
reconstruction, NOT playable. POST UPDATED DOWNLOADABLE MARKDOWN AND A COMPLETE
SOURCE/EVIDENCE CHECKPOINT EVERY WORK TURN. Only show meaningful genuine output;
no rendered logo, title screen, main menu or worthwhile screenshot exists.

## Source and recovery

The commit containing this source extends published main
`b6ea4709af883666f9b0eec9cb0af74b5926f7e2`, exact baseline tree
`3167ec7ebe8f69f5abfae950cfb757743ddac98f`. The appended downloadable receipt names
actual publication, tested tree, hosted checks and durable backup. Local Git is a
reconstructed snapshot/index, NOT remote history. Re-read main and reconcile any
intervening work before non-forced publication. Never force-push snapshot ancestry.

The provided b6ea470 checkpoint passed 764 manifest and 484 source-blob checks and
reconstructed the complete baseline tree before edits. Its archive SHA256 is
`d25508e307537b23abb176e9f236ede8702b0a0debb6c1e42c40b3ec8ae11e8d`.
Baseline full GCC build/all69 suites and the original DSP cache stop at round592
were reproduced. Original code/AOT and prepared RomFS parts were already present;
prepared ExHeader was restored from the checkpoint. RomFS was reassembled and its
whole hash checked. No CCI extraction, user reupload or user-PC operation was needed.
The predecessor remains separately preserved; do not recursively duplicate history.

## Actual new original-game progress

Eight DSP FlushDataCache calls and one GSP FlushDataCache call now succeed, covering
86,808 bytes. Target bytes and 64 captured neighbours per call remain identical;
exact reply shape, guest time, handle/thread counts remain verified. This memory
model is cacheless and already coherent. No copying or simulated cache work is
required for the supported private-memory spans. External DSP AHB/FCRAM remains
unsupported, and success does not assert an external bus transaction or device work.

The original game then reaches previously unsupported VFP short-vector settings.
The first operation at 0x00258AF0 has a three-lane FPSCR setting but a scalar-bank
destination. Bounded VFPv2 single-precision support now executes that original routine
and later vectors. Final original trace contains58 calls:4 scalar-bank calls,
6 three-lane calls and48 four-lane calls,214 lane operations. No game instruction,
AOT word, supplemental range or result was patched to force progression.

Independent Python rational binary32 replay checks ALL32 before/after register
words for every call. It covers the actual finite round-to-nearest operands and
separate multiply/add rounding. The original FPSCR/CPSR values remain unchanged
on this observed path. This is not a general independent ARM emulator or a claim
of full VFP exception/overlap/hardware-timing coverage.

## Exact current next stop

```
stop=UnsupportedIpc pc=0x0025947c detail=0x32 thread=1 dispatch_rounds=843
last_ipc_session=gsp::Gpu request_header=0x000c0000
host_ipc_error=PICA default/immediate attributes are unimplemented at list byte 0x380 register 0x232
cpu_ticks=85142780 core0_instructions=14847201 core1_instructions=99
quota_transitions=299 display_periods=19 guest_now_ns=317564398
DSP scheduled_slices=2447 notification_wait_slices=1 completed/attempted=40140800
```

The original pending command list is1,376 bytes. Its first default/immediate
attribute selection is index2, register0x232 at byte0x380. It also contains12 packed
attribute data-port words. The ENTIRE queue request remains uncommitted: live GPU
registers, upload state, GSP shared page and VRAM are unchanged by rejection. No
attribute, draw, queue advance, P3D notification or completed frame is fabricated.

Seven threads/37 handles remain. English and Spanish stop at round843; French845.
Without language the old round569 stop remains; without sound295; without camera235.
The original core1 worker has still issued only99 instructions, not gameplay.

The audio file holds9,794 complete records, all actual FIFO source-mask3 and zero
startup samples, with no fallback frames. It is not music, effects or playback.
VRAM contains only the inherited819,200-byte depth clear at offset0x419400, repeating
little-endian0x00FFFFFF; every other byte is zero. This is not a rendered picture.
No HID sample producer, executed game shader, logo/title/menu or gameplay exists.
Do not present depth-clear patterns or silent captures as worthwhile media.

## Cache contract and safeguards

DSP0x00130082 and GSP0x00080082 decode two u32 values and exactly one copied process
object (descriptor0). Current-process pseudo-handle0xFFFF8001 and actual duplicate
handles resolve to the SAME ProcessObject. Wrong/stale types return transport
InvalidHandle; another process object stops unsupported. Nonzero ranges must lie
within one readable private backing, respecting protected alias sources. Cross-region,
overflow, unmapped, shared and device spans reject before target callbacks or writes.
Zero size performs no range work after process checks. DSP requires its healthy
attached live device; GSP maintenance does not require/acquire GPU ownership.

Full normal reply preflight and private readable/writable reply validation run before
reply creation. Success modifies only the IPC response: no target read/copy/write,
reservation epoch invalidation, cache allocation, device callback, DSP instruction,
guest-time charge, interrupt, mapping or ownership change. Existing coherent alias
backing is retained. InvalidateDataCache remains unsupported. These bounded range
and failure policies are host containment, not measured firmware error precedence.
A successful flush never means external AHB/FCRAM or renderer cache behavior exists.

## VFP short-vector contract and limitations

New helper in the existing scalar backend expands supported single-precision vector
forms into the SAME bit-exact scalar arithmetic. Eight-register banks wrap circularly;
FPSCR length/stride apply; S0..S7 destinations force one scalar operation. Fm in
S0..S7 broadcasts, while Fn advances. Supported arithmetic is MLA/MLS/NMLA/NMLS,
MUL/NMUL,ADD/SUB/DIV and ABS/NEG/SQRT. Existing comparisons/conversions stay scalar.
Supported strides are1/2; reserved stride encodings and repeated-bank vectors reject.

Shifted cross-lane source/destination overlap is conservatively rejected before
mutation; identical per-iteration overlap is allowed. Enabled exception traps,
double vectors and unimplemented opcode forms retain their stop. Candidate state
holds all lane effects until success; sticky flags accumulate through scalar calls,
LEN/STRIDE controls are restored, and PC advances once. Numeric scalar algorithms
are unchanged. This is deliberately not full VFPv2 or hardware vector timing.
Original game and saved AOT bytes are untouched. Vendor LOCAL-PATCHES.md/provenance.json
record the new wrapper separately from the upstream blob and historical local edits.

## Tests, captures and scope

Full GCC/Clang builds link599 original AOT page units plus the existing three
build-generated authenticated supplemental blocks. All72 configured CTest suites
pass with both compilers and ROM-free Clang ASan/UBSan, with leak checks/halt-on-error.
JUnit names exactly match configured names, without skips or duplicates. Earlier69
suites remain unchanged. Three new suites cover DSP/GSP cache contracts and VFP
banks/stride/scalar rules, supported arithmetic, overlap/fault containment and flags.

Six ordinary compiler pairs match stdout/stderr/exit/owned files:en,fr,es,unset
language,unset sound,unset camera. All69 final capture files match byte-for-byte.
Independent checks verify nine exact cache replies/unchanged target spans, all58
VFP calls, failed queue atomicity, parsed original list, complete audio and depth-only
VRAM. Logging-only IPC/VFP/main objects link the same production libraries/AOT;
normal CLI reproduces the same stop after normalizing only the owned-root path.
This is not a full ARM/DSP address-space dump or independent CPU emulator.

Original code.bin, whole raw RomFS, ExHeader and all603 saved AOT members match their
recorded hashes. GCC/Clang supplement C++ is identical. Full IVFC checking was not
repeated; no Windows/macOS, hardware-timing, renderer or playback test ran.
A foreground build was interrupted by its tool timeout, then completed using a
tracked finite driver. Streaming sessions were unavailable. Every initiated job
was collected. Initial/intermediate stops and logs are not final test evidence.

Source changes: cache predicate in ctr_memory; DSP/GSP handler declarations/dispatch;
new dsp_cache_service.cpp; existing vendor a32_vfp_scalar.cpp vector wrapper;
CMake/test lists; three tests; vendor patch metadata and reports. No DSP peripheral,
ARM scheduler, original opcode bytes/AOT, GPU renderer, FS, CFG or HID sampler changes.

## Exact next work

Inspect the captured pending list in cache-checkpoint/capture-final/gcc/
pending-list-0-0.bin and pinned PICA default-attribute/index/packed-value behavior.
Implement real stored attributes and known-state/cursor semantics with proper bounds
and whole-list/queue failure behavior. Do not remove draw/chain guards, assume these
are rendered vertices, inject pixels or signal completion for an unexecuted list.
Rerun unchanged original code in NEW owned state and follow its actual next request.
No additional original-game upload is needed. Useful visuals remain the goal.

All earlier modes remain explicit. Audio file output avoids the old finite-prefix
capacity stop. ARM one recorded instruction/core/nominal tick, synchronous DSP
boot/waits and scheduled DSP slices are diagnostic conventions, not measured
hardware latency or physical parallelism. ExHeader verifies Multi/max30 only.
Use singular --dsp-boot-mode reference-slice. Do not silently select old fallback
silence or drop reset/special/CFG/VRAM/PTM/CPU policies.

Primary cache references: Azahar86a9f9236ae42bb5a2b995dbc933d599d8ea07ac,
src/core/hle/service/dsp/dsp_dsp.cpp blobf8b23c07925c6b4a9fe36acaa7954f05126441f5;
src/core/hle/service/gsp/gsp_gpu.cpp blob6f915e4d6a5d27853321d0a233103afb9d877bc0.
VFP: ARM VFP11 TRM DDI0274H sections2.7/3.4.2, tables2-7/2-8/3-7, official URL
https://documentation-service.arm.com/static/5e8e227c88295d1e18d377ac . Parsed text
was read; requested screenshots/direct download failed. No page-image inspection
or new hardware measurement is claimed. Exact read scopes are in references.json.

## Scratch and complete reproduction

Root /mnt/data/lego_recovery/. Source repo/. Full builds build-gcc/,build-clang/;
ROM-free build-asan/. AOT generated2/; code restored/code.bin; raw RomFS
 game/prepared-romfs/romfs.bin; ExHeader prepared-launch/exheader.bin;
parts/manifest romfs-library-roundtrip/. Current work cache-checkpoint/.
Historical predecessor restored-b6ea470/ remains separate; never overlay it on repo/.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2 -DLEGO_SUPPLEMENT_CODE_BIN=/mnt/data/lego_recovery/restored/code.bin
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/attributes-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty --romfs game/prepared-romfs/romfs.bin --gpu-vram-mode reference-zero --display-clock-mode reference-idle --cfg-profile reference-stereo --cfg-sound-mode stereo --cfg-language en --cpu-mode diagnostic-dual --block-limit 100000000 --exheader prepared-launch/exheader.bin --dsp-special-profile empty-config --dsp-executor live-teakra --dsp-reset-profile reference-zero-data --dsp-transmit-profile reference-stereo --dsp-audio-mode capture --dsp-boot-mode reference-slice --dsp-audio-file "$NEW_ROOT/audio.dspaud"
```

Expected exit3, pending attributes at round843. Clang uses clang++. Sanitizers omit
BOTH AOT/supplement options, Debug-O1 with address/undefined/frame-pointer compiler
flags and matching linker flags, ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 and
UBSAN_OPTIONS=halt_on_error=1. Exact commands: final-*-steps.json. Omitting supplement
input retains earlier MissingBlock; do not call it a new regression. Generated
supplement C++ remains PRIVATE and regenerates from authenticated code.bin at build.

Evidence: cache-checkpoint/tests.json,configured-*.json,junit-final-*.xml,
final-*-test.log; matrix-results.json,pair-*/verified.json; capture-final/{gcc,clang}/;
proof.json/verify_results.py; identity.json/verify_inputs.py; make_capture.py and
trace_ipc.cpp/trace_vfp.cpp/capture_main.cpp; references.json/setup-notes.txt.
run_job.py collects bounded build/test stages; all .done receipts are0. Capture
builders use current production sources. validate_pair.py requires NEW output names;
unknown saves/captures must never be overwritten. Only the driver's own just-created,
byte/hash-checked gamecoin/audio files are removed to pair compilers on the same path.
cache-owned.* and cache-paired.* are TEST state, not recovered console NAND.

## Durable recovery and every-turn mandate

GitHub GTTeancum/Lego-Undercover-3DS-Recomp; Library /LEGO-Chase-Recovery/.
New archive contains complete indexed source/current PRIVATE evidence, prepared
launch, parts manifest, source/member index, verifier, exact parent patch and
actual receipts. Verify CHECKPOINT-MANIFEST.json and SOURCE-INDEX.json paths/modes/
Git blobs, including tracked ignored reports. Independently restore tree/patch;
never invent upstream ancestry. Historical archives remain separate, not recursively
embedded. The downloadable appended receipt gives actual commit and saved locations.

Large originals have existing separate Library backups: code.bin;
LEGO-Chase-current-AOT-599pages-2026-10-03.tgz (unpacks generated2/);
Prepared-RomFS/two parts+romfs-parts.json; Prepared-Launch/exheader.bin.
Parts402653184+366526464 restore via repo/tools/restore_romfs_parts.py into a NEW
file; raw769179648 bytes, native offset4096/size769175552. Preserve integrity tables.
No CCI extraction or new game upload is required. Original SHA256 identities:
code5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f;
AOT2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9;
RomFS6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf;
ExHeaderd7641f0a3bb89697ca8f0751e88d31703b54aa185ac78f9e74e41169dfcdc004.

Raw game instruction/VFP/PICA/DSP/SRAM/audio/ExHeader captures NEVER enter public
GitHub. Code.bin, private AOT/supplement C++, rawRomFS/CCI, .git and compiled binaries
are excluded from the source/evidence archive. Scratch can reset; artifacts/Library/
GitHub are recovery routes, not permanent scratch. Append only observed publication,
CI and persistence receipts. POST UPDATED DOWNLOADABLE MARKDOWN AND COMPLETE
RECOVERABLE SOURCE/EVIDENCE AT THE END OF EVERY WORK TURN.
