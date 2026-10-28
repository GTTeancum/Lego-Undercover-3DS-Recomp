# October 28, 2026 — native color draws and framebuffer crop

## Current outcome and next stop

The native runtime now completes the original draw that stopped checkpoint
`5563791c095e1ff8c76f8d79cdd3cf76764322c5` at dispatch round 843. It also completes
the next unsupported framebuffer transfer, reached at round 1109. No original
game instructions, uploaded shader program, AOT pages, assets, or input files
were patched to obtain this progress.

The latest ordinary original-game run, with the existing launch profile and
`--host-event-limit 16384`, reports:

```
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=8 dispatch_rounds=1203
last_ipc_session=fs:USER request_header=0x080c00c2
guest_now_ns=501570401
request_words=080c00c2 00000004 00000001 00000001 00004002 0e022d3c
```

This is `OpenArchive` for archive ID 4 (SaveData), an Empty path of declared size
1 in static buffer 0. The captured payload byte is `0x3c`, not NUL; the Empty
path type governs its semantics, so it must not be interpreted as a filename.
The current filesystem implementation supports a specific
shared-extdata OpenArchive path, not a title save-data archive. The next task is
to implement a distinct, contained title-save backend and its real missing-save
and handle semantics. Do not widen the shared-extdata ID check or invent a
successful save mount. The private checkpoint includes a pinned-reference brief
and the observed request/path capture.

The default 4,096-event diagnostic budget first stopped at round 1087 with
`HostEventLimit`. Raising only that finite host budget to 16,384 exposed the real
subsequent transfer and filesystem stops. This is a diagnostic budget change,
not a CPU timing fix, skipped operation, or hardware speed measurement.

No visible game scene, menu, music playback, or gameplay is demonstrated. The
verified startup color buffers are black; native drawing and device transfers
are now real operations, but these results do not establish useful presentation.

## Implemented drawing path

`src/services/pica_rasterizer.{h,cpp}` adds an independently authored, bounded
software color stage. It consumes the existing fetched/shaded vertex outputs and
known register state, maps output semantics, assembles fresh triangle lists or
strips, performs viewport conversion and pixel coverage, interpolates colors with
perspective correction, applies the supported TEV operations and blending, and
stages channel-masked tiled RGBA8 writes.

Supported TEV operands are primary color, stage constants, and previous stage;
operations are unscaled Replace and Modulate with the implemented direct
operands. Additive blending supports Zero, One, SourceAlpha, and
OneMinusSourceAlpha factors. LogicCopy is supported when blending is disabled.
The framebuffer uses 8-by-8 Morton tiles, ABGR bytes, and the supported vertical
orientation. Unknown consumed fields and unsupported states stop explicitly.

Coverage and arithmetic follow the pinned Azahar software model. Arithmetic
uses separate binary32 rounding boundaries, with explicit finite normal/zero
checks and nearest host rounding. This is not measured hardware float24 or
subpixel parity. User clipping, clip-volume crossings, scissor tests, indexed
fetch, geometry shaders, textures, fog, other fragment tests/formats, unsupported
blend/TEV operations, and the other framebuffer orientation remain guarded.
Unused register storage is not evidence that a consumed field is known.

The decoder supplies an optional draw staging callback only when the GSP queue
owns a device transaction. Calling the decoder alone still rejects draw
execution. Primitive state begins empty according to the reference and resets
on topology/restart writes. A nonempty draw requires another explicit reset
before a later draw; retained strip/fan or partial-list state is not fabricated.

The queue keeps a lazy independent copy of established VRAM for preflight.
Earlier device writes are visible to later draws, and later transfers can read
staged draw output. FCRAM dependencies that are not forwarded remain explicit
stops. A later bad list, packet, or IRQ preflight discards all staged draw bytes
and state. On success, actual device patches commit before the associated P3D
interrupt. A draw does not invent an IRQ when its list did not request one.
Existing synchronous timing and event semantics remain in force.

Host bounds are 4,096 fetched vertices and 1 MiB of source reads per draw; the
existing shader bounds remain. A raster framebuffer is at most 1 MiB, each draw
has at most 4 Mi candidate-pixel tests, and a complete eligible queue accepts at
most 16 draws and 16 Mi candidate-pixel tests. Remaining queue raster work is
preflighted before fragment loops. These are containment limits, not hardware
capacity claims.

## Unscaled framebuffer transfer

`DisplayTransfer` flag `0x1004` now supports a left/top crop from tiled RGBA8
physical VRAM to linear RGB8 physical VRAM. It preserves the input tile-row
stride, drops alpha, and copies BGR channels without filtering or horizontal
halving. Odd or partial-tile output dimensions are valid. Source and output
spans are bounded independently; alignment, overlap, capacity, and atomic
staging checks remain mandatory.

The observed packet is source `0x1f084800`, input 256 by 400, destination
`0x1f038400`, output 240 by 320, flags `0x1004`. It produces 230,400 bytes and
requests a real PPF completion. Existing scaled `0x01001004` and RGBA4 `0x4400`
paths remain supported. No screen-clear substitution is used in either stage.

## Verification and evidence

All 77 runtime suites pass locally, including the two new raster/draw suites and
expanded transfer coverage. Authored nonzero fixtures check triangle coverage,
shared-edge ownership, perspective interpolation, TEV and blending, masks,
framebuffer tile boundaries, work limits, late-failure atomicity, real queue
commit and waiter/IRQ behavior, draw/transfer ordering, and complete rollback.
Transfer fixtures include odd crops and the original 256-by-400 to 240-by-320
shape, with independent bytes that distinguish copying from averaging.

Independent private raster checks preserve all four original shader colors,
including alpha `0x3f7fffff`, and calculate every covered sample. The original
first draw covers 192,000 pixels with two triangles and 384,000 candidate-pixel
tests. Nearest-byte color conversion produces alpha 255, followed by TEV alpha
238. Every byte of the 6,291,456-byte VRAM bank is compared for both the captured
input and an arbitrary seeded background, including all masked/uncovered bytes.
The seeded first-draw case changes 573,732 RGB bytes. The captured original case
changes zero because its target was already black; this is not evidence of a
substitute no-op.

The initial integrated capture verifies all 121 queue traces, the actual first
draw's queue retirement and P3D event, and atomic rejection at the subsequently
unsupported transfer. It records seven committed draws totaling 28 vertices,
504 shader instructions, 14 triangles, and 1,228,800 covered pixels. Final
proofs rerun the raster and transfer stages against the final production build,
and the final integrated capture demonstrates the transfer commit before the
new filesystem stop. That final capture verifies all 147 queue traces and
records 11 draws, 44 vertices, 792 shader instructions, 22 triangles, and
1,766,400 covered pixels. Both observed `0x1004` transfers (queues 121 and 147)
match the full-VRAM oracle and exact dequeue/PPF bytes. Exact proof paths,
hashes, counters, source identity, and
hosted GCC/Clang/sanitizer outcomes are in the private continuation receipt.

Several generated executables in the recovered cloud build cache had invalid
headers or modes; they were regenerated and checked before the final local
suite. The existing filesystem permission test ran with normal process
permissions so its isolated child could drop privileges. No test was disabled
or weakened. Hosted workflows rebuild public source in clean environments.

Original code.bin, ExHeader, and RomFS still match their saved SHA-256 values.
The original 599-page AOT archive and authenticated three-block supplement are
unchanged. Public Git contains only source, authored tests, and documentation;
game inputs, raw GPU captures, audio, and private proofs stay in the recovery
checkpoint. Source is preserved by the real GitHub revision and hosted source
snapshot; the private archive carries source identity metadata and evidence.

## Reproduction and continuation

Use the saved private input manifest and the real repository history. From the
recovery directory with `repo/`, the separately saved game inputs, prepared
RomFS, and restored AOT pages in their documented locations:

```bash
cmake -S repo -B build-current -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DLEGO_AOT_DIR="$PWD/restored-aot/generated2" \
  -DLEGO_SUPPLEMENT_CODE_BIN="$PWD/recovered-inputs/LEGO-Chase-Recovery/code.bin"
cmake --build build-current --parallel 4
ctest --test-dir build-current --output-on-failure
python3 evidence/run_original.py continuation build-current/LEGOChaseNative \
  --host-event-limit 16384
```

Rebuild native diagnostic helpers using the private checkpoint instructions;
compiled artifacts are deliberately excluded. Private replay and capture tools
verify immutable original inputs and distinguish offline staging from live
queue commit. New runs use fresh owned test-state directories. Do not reuse
unknown saves or substitute a cleared image for actual draw execution.

Primary reference: [Azahar at 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac](https://github.com/azahar-emu/azahar/tree/86a9f9236ae42bb5a2b995dbc933d599d8ea07ac),
`pica/primitive_assembly.cpp`, `pica/output_vertex.cpp`, PICA register definitions,
`renderer_software/sw_rasterizer.cpp`, `sw_clipper.cpp`, `sw_framebuffer.cpp`,
`sw_texturing.cpp`, `sw_blitter.cpp`, and common color/vector helpers. The private
reference manifests record exact URLs and content hashes.

Continue in the assistant cloud workspace with ordinary build tooling. Keep
chat replies to one screen. Do not launch Codex tasks, operate the user's PC,
or change Dot's project. Preserve checkpoints and advance the next verified
blocker without repeated approval prompts.

---

## Prior checkpoint record — attributes and staged vertex execution

The following section is historical evidence for the predecessor checkpoint;
its draw stop and missing-raster descriptions have been superseded above.

# October 28, 2026 continuation update

## Verified restoration and current stop

The saved checkpoint `d83daf2a1a39ac6478dd8b7993af6827102de59a` was restored and
matched against the current Git history: all 490 source blobs and modes match.
The checkpoint verifier passed all 836 manifest files. The executable, ExHeader,
599-page AOT archive (603 regular files), both prepared RomFS parts, and combined
769,179,648-byte RomFS matched their recorded hashes. No new game upload or CCI
extraction was needed. Original game bytes and generated AOT pages are unchanged.

A clean GCC native build with the authenticated three-block supplement reproduced
the previous `round843 / pc0x0025947c / attributes0x232 at byte0x380` stop exactly.
After adding default-attribute support, the unchanged original run now reports:

```
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=843
last_ipc_session=gsp::Gpu request_header=0x000c0000
host_ipc_error=PICA draw execution is unimplemented at list byte 0x4e8 register 0x22e
```

This advances processing within the same 1,376-byte GPU list. It does not advance
the guest past the queue request: the existing complete-queue rejection policy
keeps every staged attribute/register/IRQ change uncommitted. No rendered frame,
logo, title screen, menu, music playback, or gameplay is demonstrated.

## New default-attribute implementation

`src/services/pica_startup.{h,cpp}` now models the three-word packed float24 FIFO,
raw component values, corresponding IEEE32 container bits, known/written state,
full-width cursor, and stored default attributes 0 through 14. All three data ports
feed the same FIFO; partial vectors persist across successful list boundaries.
Index writes reset the FIFO; masked and zero-mask index writes are supported after
a complete index has established its value. Completing attribute14 advances to15
without wrapping. Index15 vertex submission remains an explicit rendering stop.

Containment policy: new data ports require full byte mask0xF; an unknown selected
index, unsupported index, or corrupt FIFO count stops before data-port effects.
The pinned emulator's upload-mask TODO does not establish hardware partial-mask
semantics. The supported 0..14 range follows that emulator; Citro3D's public API
exposes twelve default attributes. None of this claims complete hardware timing
or float24 arithmetic fidelity. Draw, chain, and fog guards remain in force.

Focused GCC checks passed: `ctr_pica_attributes_test`, `ctr_pica_startup_test`,
`ctr_pica_proctex_test`, `ctr_pica_swizzle_test`, `ctr_pica_vertex_fetch_test`,
`ctr_pica_vertex_shader_test`, and `ctr_gsp_submit_test`. They cover independently specified
packed values, cursor/FIFO behavior, known-state/bounds policies, and a later
GSP-packet failure preserving already committed state and suppressing staged IRQs.
The native original-game run is separate evidence from these ROM-free tests.

## Native vertex loading

`src/services/pica_vertex_fetch.{h,cpp}` adds a staged, read-only non-indexed
vertex loader and vertex-shader input mapping. It decodes signed bytes, unsigned
bytes, signed shorts and float32 arrays, including component alignment, padding
descriptors, separate loader offsets/strides, uploaded default attributes and
input-register permutations. Per-byte register knowledge prevents allocated zero
storage or mask-zero writes from establishing unknown configuration fields.

Reads are restricted to readable private linear FCRAM or an explicitly supplied
VRAM bank. Shared/device callbacks, unknown configuration, duplicate loaders,
retained attributes, indexed draws and geometry shaders remain unsupported.
The explicit host bounds are 4,096 vertices and 1 MiB of source reads per stage.
Failure preserves the caller's previous output plan and all source state.

The original first draw uses four vertices with a 32-byte stride: float3 position,
two padding descriptors, unsigned-byte4 color, and four uploaded defaults. The
new loader performs eight attribute reads totaling 64 bytes. These are shader
inputs from a discarded command-list prefix; they do not constitute draw execution.

## Native vertex-shader execution and original-data proof

`src/services/pica_vertex_shader.{h,cpp}` adds the bounded MOV, MUL, DPHI, MADI,
CALLU and END instruction families. The stage tracks known components through
input, temporary and output registers, applies operand swizzles/signs and write
masks, follows conditional calls, and compacts enabled outputs in register order.
Unknown program words, descriptors, uniforms or consumed components are explicit
failures; no missing instruction is replaced with an implicit END. A selected
boolean's byte must be established, even if the upload-observed flag is set.

The numerical model follows the pinned reference's float32 containers, with a
separate rounding boundary for each multiply and addition. It does not claim
hardware float24 precision. Arithmetic requires nearest host rounding and finite
normal/zero operands/results; subnormal, underflow and nonfinite arithmetic stop.
MOV preserves raw bits. Relative addressing, geometry shaders, unsupported
opcodes, ambiguous call boundaries and calls deeper than three remain unsupported.
Host bounds are 1,024 executed instructions per vertex and 1,048,576 per plan.
Source state is immutable and a failed stage preserves the prior output plan.

The complete private original capture verified all 78 queue traces. Its first-draw
prefix contains the four default uploads and the four vertices described above;
the rejected queue and diagnostic replay leave registers, uploads, shared GSP
memory, VRAM, requester event state and guest time unchanged. The final VRAM image
matches the previous depth-fill-only capture. An earlier diagnostic attempt had
one empty trace file and is explicitly marked failed/superseded; the complete
capture uses checked atomic JSON writes and sequence-completeness verification.

An offline helper then calls the actual production shader stage using those
captured vertices and the unchanged uploaded shader program/descriptors/uniforms.
Each vertex executes 18 instructions; all four produce five output vectors.
All 80 output component bit patterns match an independent exact-rational oracle
that rounds after every operation to binary32, ties to even. It independently
checks 72 executed instructions, 144 multiplies and 96 additions. This is genuine
shader execution in a read-only replay, not a committed GPU draw or rendered frame.

The private checkpoint contains `evidence/captured-final-1z19nwkg/capture`, its
`attribute-proof.json`, `evidence/shader-replay-x5bf1hxx/shader-proof.json`, the
raw captures, authored replay/verification tools, and focused build/test receipts.
Hosted checks and exact publication identity are recorded in the downloadable
continuation receipt after publication. No raw captures are in public Git.

## Immediate next work

Implement primitive assembly and rasterization, starting from the already verified
first-draw shader outputs. The captured configuration calls for a triangle strip,
output-semantic mapping, clipping/viewport conversion, texture-disabled TEV
combining, alpha blending and masked RGB writes into the tiled RGBA8 framebuffer.
The viewport is 480 by 400; the framebuffer is 512 by 400. Depth/stencil writes
and tests are disabled for this draw. The private
`evidence/first-draw-raster-requirements.json` records decoded fields and their
known-byte provenance; treat it as configuration evidence, not pixel results.

The current recovered source has register/upload storage, vertex loading, bounded
shader execution, fills and display transfers, but no complete draw pipeline.
Do not acknowledge a draw or retire the queue until the required work is implemented.
Private capture evidence must distinguish staged prefix attributes from live state.

## Fresh recovery and reproduction

Extract the new source/evidence archive into a new owned directory after its
`verify_checkpoint.py` check passes. The archive contains `repo/` as an exact
source snapshot, without Git metadata. For continued development, clone this
GitHub repository with its real history, check out the receipt's commit, and
compare its tree with `SOURCE-INDEX.json`; do not manufacture snapshot ancestry.
Use the saved snapshot if GitHub is unavailable, while preserving the recorded
commit/tree identity for later reconciliation.

Materialize the separately saved inputs named in `evidence/recovery-inputs.json`
under `recovered-inputs/LEGO-Chase-Recovery/`, retaining their subfolders. Verify
code, ExHeader and AOT hashes from `evidence/restored-inputs.json`; the RomFS
restoration tool verifies each part and the combined hash. No original CCI
extraction or user re-upload is needed. Current helper layout:

```
repo/
evidence/
recovered-inputs/LEGO-Chase-Recovery/code.bin
recovered-inputs/LEGO-Chase-Recovery/Prepared-Launch/exheader.bin
recovered-inputs/LEGO-Chase-Recovery/Prepared-RomFS/romfs-parts.json
recovered-inputs/LEGO-Chase-Recovery/Prepared-RomFS/LEGO-Chase-USA-romfs.bin.part001
recovered-inputs/LEGO-Chase-Recovery/Prepared-RomFS/LEGO-Chase-USA-romfs.bin.part002
restored-aot/generated2/
romfs.bin
build-current/
```

After checking the recorded AOT archive hash, extract its regular `generated2/`
members into `restored-aot/`, rejecting absolute paths, parent traversal and links.
Run these commands from the containing recovery directory with Python 3.12,
CMake, Ninja, a C++20 compiler, Clang and `llvm-objdump` installed. Clang/LLVM are
used only to regenerate the authenticated three-block AOT supplement at build time:

```bash
python3 repo/tools/restore_romfs_parts.py \
  recovered-inputs/LEGO-Chase-Recovery/Prepared-RomFS/romfs-parts.json romfs.bin
cmake -S repo -B build-current -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DLEGO_AOT_DIR="$PWD/restored-aot/generated2" \
  -DLEGO_SUPPLEMENT_CODE_BIN="$PWD/recovered-inputs/LEGO-Chase-Recovery/code.bin"
cmake --build build-current --parallel 4
ctest --test-dir build-current --output-on-failure
python3 evidence/run_original.py restored-native build-current/LEGOChaseNative
python3 evidence/startup_capture/build_capture.py --vertex-fetch
python3 evidence/run_original.py restored-capture evidence/startup_capture/LEGOChaseCapture --capture
```

Verify the fresh printed capture directory with `verify_capture.py --require-vertex`
and pass its first-draw candidate JSON to `run_shader_replay.py`; exact commands
and scope are in `evidence/startup_capture/README.md`. The archived complete capture
can also be replayed offline without rerunning the game. The native stop remains
expected exit 3 at round 843. Each run wrapper creates fresh owned state; it does
not reuse unknown saves. Old absolute paths in receipts are provenance, not
mandatory locations. Compiled executables/libraries/objects and generated AOT are
rebuilt; capture bytes, startup audio records, tools and verification are retained.

## Reference pins

- Azahar `86a9f9236ae42bb5a2b995dbc933d599d8ea07ac`,
  `src/video_core/pica/pica_core.cpp`: default-index reset, FIFO ports, full-width
  cursor increment, and default/immediate split.
- Same pin, `src/video_core/pica/packed_attribute.h`: four float24 values in three
  words; `src/video_core/pica_types.h`: lossless container conversion.
- Same pin, `src/video_core/pica/vertex_loader.cpp`, `regs_pipeline.h`,
  `regs_shader.h` and `shader_unit.cpp`: array layout, padding, component formats
  and shader input-register mapping. The new loader is an independent bounded
  implementation, with unsupported reference cases rejected explicitly.
- [Azahar pinned source](https://github.com/azahar-emu/azahar/tree/86a9f9236ae42bb5a2b995dbc933d599d8ea07ac),
  `src/video_core/shader/shader_interpreter.cpp`, `pica_types.h` and
  `pica/shader_unit.cpp`: arithmetic ordering, conditional calls and output mapping.
- [nihstro pinned instruction format](https://github.com/neobrain/nihstro/blob/f4d8659decbfe5d234f04134b5002b82dc515a44/include/nihstro/shader_bytecode.h):
  opcode families, operand widths, swizzle fields and flow-control encoding.
- Citro3D `9f21cf7b380ce6f9e01a0420f19f0763e5443ca7`, `source/base.c` and
  `source/immediate.c`: independent fixed-attribute upload and packing behavior.

## Operating and recovery rules

Continue in assistant cloud scratch with ordinary compiler/build tooling. Do not
invoke Codex or operate the user's PC or Dot. Keep user-facing replies brief.
Preserve private game data and raw captures outside public Git. Use the real Git
history and non-forced publication; never replace it with snapshot ancestry.
Save an updated handoff and a recoverable private source/evidence checkpoint after
meaningful work. The prepared inputs below remain the recovery source of truth.

The following October7 handoff is preserved as the verified predecessor, including
input hashes and launch flags. Its old attribute stop and scratch paths are historical;
the current stop is the draw request above. Use the new checkpoint receipt for
current paths, source revision, checks and saved evidence.

---

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
