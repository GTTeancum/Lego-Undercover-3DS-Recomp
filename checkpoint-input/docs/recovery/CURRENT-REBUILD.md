# LEGO Chase Begins — canonical continuation handoff

October 7, 2026. Work in assistant scratch, NOT the user's PC or Work.
Nintendo 3DS USA: LEGO City Undercover: The Chase Begins. Native/headless startup
reconstruction, NOT playable. POST UPDATED DOWNLOADABLE MARKDOWN AND A COMPLETE
SOURCE/EVIDENCE CHECKPOINT EVERY WORK TURN. Show only meaningful genuine game output.
There is still no rendered logo, title screen, main menu or useful screenshot.

## Canonical source and preserved alternate

This source extends main `3089a50280cba142d452ae414630d55b80a9b32f`, exact tree
`a10877a01f4309532ee35088e1532fd35e74f6cb`. Actual publication, source tree, hosted
checks and saved archive are recorded in the final appended delivery receipt.
Local Git is an exact reconstructed index/snapshot, NOT remote history. Re-read main
before publication, reconcile concurrent work and use its true parent without force.

The task arrived with DSP-PIPE-READ-PENDING, tree
`1a44fc742a82dfb89fd3e691d123a46fff286d06`. All 1,078 manifest entries and 465 source
blobs verified. It is separately preserved under preserved-pipe-pending/ and saved as
/LEGO-Chase-Recovery/Preserved-DSP-PIPE-READ-PENDING.tgz and .md. Newer main contained
a different completed pipe implementation (four rather than six bounded wait quanta,
separate service file and address test). It was NOT overwritten with the alternate.

Main's Library archive was restored: 1,186 manifest files, 467 source blobs and exact
tree match. Archive SHA256 c706c5b71cf0ebcf060fa84bb930f0279cb1b924b08331b47e3cbd5483aa40d0.
The full GCC baseline/all 63 suites and original round295 sound-setting stop were
reproduced. Original code/AOT/raw parts were already mounted. AOT was re-extracted,
RomFS reassembled with hash checking, and the ExHeader restored from the checkpoint.
No CCI extraction, new user upload, user-PC or Work operation was needed.

## Actual original-game progress

New explicit option: `--cfg-sound-mode mono|stereo|surround`.
It is independent of `--cfg-profile reference-stereo`, which still means only the
camera compatibility block. Default sound selection is Unconfigured. The sound
setting is a HOST preference, not recovered console NAND/calibration or a host audio
output device. Mono=0, stereo=1, surround=2 follow pinned CFG. Selection enables only
block0x70001,size1,write-mapped descriptor0x1C. Invalid/repeated choices reject launch.

The original stereo request at round295 is:
`00010082 00000001 00070001 0000001c 0ffff658`.
Reply: `00010042 00000000 0000001c 0ffff658`, remaining words zero.
Output is exactly byte1 at0x0FFFF658. The other63 captured neighbouring bytes remain
unchanged, as do DSP SRAM/provenance, GPU uploads/registers, GSP page/epochs and HID.
No event, handle allocation, DSP instruction or time advance occurs inside this read.

Startup then reaches graphics work. The original second-core worker has now issued
99 recorded instructions. DSP has completed259 scheduled slices plus the inherited
one synchronous mailbox-wait slice. These are actual current normal-path counts,
not a gameplay/audio-initialization or hardware-timing claim.

## PICA swizzle support and exact uncommitted next boundary

Swizzle data ports0x2A6..0x2AD (GS) and0x2D6..0x2DD (VS) now retain RAW parameters,
independently of the masked register mirror. Full-width offset registers0x2A5/0x2D5
increment per word. Each stage holds4096 reference-capacity entries with explicit
written bits. VS also updates GS under the existing non-exclusive/no-GS mirror rule;
it does not increment the GS offset. Unknown slots are not valid initialized shader
data. This implements uploads, NOT shader interpretation or rasterization.

The original23,360-byte list contains83 swizzle writes before a later unsupported
procedural-texture lookup at register0xB0/list byte0x5280. Independent Python packet
replay matches both staged tables/masks and offset state. HOWEVER, the existing
whole-list/batch transaction rejects the entire list: those83 words are only in a
disposable plan. They are NOT committed live GPU uploads. No prefix dequeue, IRQ,
completed frame or successful list submission is fabricated.

Final ordinary stereo run:
```
stop=UnsupportedIpc pc=0x0025947c detail=0x32 thread=1 dispatch_rounds=357
last_ipc_session=gsp::Gpu request_header=0x000c0000
host_ipc_error=PICA fog/procedural lookup upload is unimplemented at list byte 0x5280 register 0xb0
cpu_ticks=13444377 core0_instructions=2407614 core1_instructions=99 quota_transitions=32
display_periods=3 guest_now_ns=50144657
DSP completed/attempted Run(1)=4292608; scheduled_slices=259; notification_wait_slices=1
dsp_audio_mode=capture frames=1042 playback=none
```

Across the rejected call, CPU registers/thread state, IPC, live GPU registers/uploads,
GSP queue/epochs, captured DSP/HID memory and time are unchanged. Entire6MiB VRAM
remains zero. No actual GPU draw, logo/title/menu/frame, audible playback or HID
sampling exists. Retained audio count does not certify music/effects; samples were
not newly interpreted for audible content in this turn. Do not post blank captures.

## Tests and scope

Full GCC and Clang builds link all599 unchanged private AOT page units. All65 CTest
suites pass under both, and all65 ROM-free Clang ASan/UBSan suites pass with leak
checks/halt-on-error. JUnit names equal the exact configured set, no skips/duplicates.
New ctr_cfg_sound_test covers choices, independent profiles, immutable session state,
copyout, untouched neighbours, permission/alias/epoch checks and allocation rollback.
New ctr_pica_swizzle_test covers all16 masks/all16 data ports, mirroring, full-table
bounds, packet sequences, persistence and rejection without initial-state mutation.
The old PICA unsupported-effect list no longer expects valid swizzle ports to fail;
its other draw/default-attribute/chaining/LUT guards remain intact.

Six ordinary compiler pairs match stdout/stderr/exit and owned test files: stereo,
mono, surround, no sound choice, no camera profile, and immediate DSP boot. Seven
invalid-option pairs reject before game-input access. All129 logging-only capture
files match between compilers. Independent replay verifies the83 staged swizzles;
this is not an independent CPU emulator or complete ARM address-space dump.

Original code, whole raw RomFS, ExHeader and all603 AOT archive members retain their
verified identities. Full IVFC checking was not repeated. No Windows/macOS, playback
or renderer build/test. No CPU decoder, DSP interpreter/peripheral, scheduler, game
code/AOT, FS or HID sampler changed. All other explicit policies remain unchanged.

Private evidence: sound-checkpoint/. tests.json, junit-{gcc,clang,asan}.xml,
configured-*.json; final-normal.json/log and final-asan.json/log; baseline proof;
pair-{stereo,mono,surround,omitted,no-camera,immediate}/; capture-{gcc,clang}-files/
(129 files each), proof.json/verify_results.py, identity.json/verify_inputs_cli.py,
cli.json, restore.json, references.json, setup-notes.txt. make_capture.py uses the
archived capture_support.inc generation recipe and only instruments copies of main
and the scheduler, not production sources. Final ordinary binaries independently
reproduce the same stop. Native/trace binaries and objects are excluded/rebuildable.

Setup limitations: streaming sessions unavailable; all subprocess builds/tests were
collected in this turn. Recovery receipt key tree/source_tree and a CLI harness's
wrong expected exit1 (actual existing exit2) were corrected. These were helper issues,
not passing tests or new production fixes. Exact notes are retained.

## Next exact work

Implement the observed procedural-texture LUT upload only after inspecting the
original packet and pinned config/table contract. Current raw list is in
sound-checkpoint/capture-gcc-files/357-before-list-0.bin (private). Offset0x5280,
register0xB0 is the first unsupported operation; neither the prefix nor its later
commands have committed. Do not bypass lookup writes or force a GPU IRQ/queue advance.
Preserve the actual swizzle tables and command masks/offsets; retain whole-batch
failure semantics and test new nonzero tables. Then rerun unchanged original code
from NEW empty test state and follow the next observed operation. GPU drawing,
HID sampling, external DSP AHB/FCRAM and broader lifecycle/playback remain open.

Primary pin: azahar-emu/azahar@86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
CFG cfg.h blob4c5275a343d62005d305b55668fbede285607132; cfg_defaults.cpp
blob424664396a50c369dd7401b97372b924e28def2e. PICA pica_core.cpp
blob910ebc2021d8b546a79309ddfd4e080049c43043; regs_shader.h
blobaf31784281a8b3f41d61a8c955aa6b5d704cadad; shader_setup.h
blob84ed49805143f656112a1fa1e4668865e00b2b45. Current searches were navigation;
implementation reads used the pin. No new hardware measurement/PDF analysis.

## Scratch and reproducible run

Root /mnt/data/lego_recovery/. Source repo/. Builds build-gcc/,build-clang/,build-asan/.
AOT generated2/; code restored/code.bin; raw RomFS game/prepared-romfs/romfs.bin;
ExHeader prepared-launch/exheader.bin; parts/manifest romfs-library-roundtrip/.
Baseline source/evidence baseline-3089a50/; untouched alternate preserved-pipe-pending/.
Current work sound-checkpoint/. Do not extract historical source over current repo/.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 3
ctest --test-dir build-gcc --output-on-failure
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/lookup-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty --romfs game/prepared-romfs/romfs.bin --gpu-vram-mode reference-zero --display-clock-mode reference-idle --cfg-profile reference-stereo --cfg-sound-mode stereo --cpu-mode diagnostic-dual --block-limit 100000000 --exheader prepared-launch/exheader.bin --dsp-special-profile empty-config --dsp-executor live-teakra --dsp-reset-profile reference-zero-data --dsp-transmit-profile reference-stereo --dsp-audio-mode capture --dsp-boot-mode reference-slice
```

Expected exit3, pending PICA procedural-LUT at round357. Omit ONLY --cfg-sound-mode
and value for old round295 CFG stop. Sound selection alone cannot enable camera CFG.
Keep every explicit policy visible. For Clang select clang++; sanitizers omit AOT,
Debug with -O1 -fsanitize=address,undefined -fno-omit-frame-pointer plus matching link
flags, ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 and UBSAN_OPTIONS=halt_on_error=1.
Capture/pair scripts require fresh output directories. Delete only their own new,
size/hash-verified20-byte gamecoin when pairing compilers; preserve unknown saves.

## Durable recovery and every-turn mandate

GitHub GTTeancum/Lego-Undercover-3DS-Recomp main; Library /LEGO-Chase-Recovery/.
The checkpoint contains complete indexed source, current private evidence, prepared
launch, source/member manifests, verifier, patch from3089a50 and predecessor receipts.
Verify CHECKPOINT-MANIFEST.json before extraction and SOURCE-INDEX.json exact Git
blobs/modes including ignored tracked reports. Reconstruct the index, not fake
remote ancestry. Raw DSP/SRAM/list/capture/ExHeader bytes NEVER enter public GitHub.

Separate existing Library inputs: code.bin; LEGO-Chase-current-AOT-599pages-2026-10-03.tgz
(unpacks generated2/); Prepared-RomFS/two parts plus romfs-parts.json;
Prepared-Launch/exheader.bin. Parts402653184+366526464 reassemble via
repo/tools/restore_romfs_parts.py, refusing overwrite. Raw769179648 bytes, native
view offset4096/length769175552; preserve all integrity tables. No CCI extraction.
Code SHA5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
AOT SHA2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9.
RomFS SHA6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf.
ExHeader SHAd7641f0a3bb89697ca8f0751e88d31703b54aa185ac78f9e74e41169dfcdc004.

No original code/AOT/rawRomFS/CCI, .git, compiled objects/binaries or font files are
embedded. Older checkpoints remain separate, not recursively copied. Scratch can
reset; attachments/Library/GitHub are recovery routes, not permanent scratch.
Record actual publication, hosted CI and backup receipts only after success.
POST UPDATED DOWNLOADABLE MARKDOWN AND COMPLETE SOURCE/EVIDENCE EVERY WORK TURN.
