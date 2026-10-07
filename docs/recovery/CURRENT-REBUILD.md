# LEGO Chase Begins — canonical continuation handoff

October 7, 2026. Continue in assistant scratch, NOT the user's PC or Work.
LEGO City Undercover: The Chase Begins, Nintendo 3DS USA. Native/headless startup
reconstruction, NOT playable. POST UPDATED DOWNLOADABLE MARKDOWN AND A COMPLETE
SOURCE/EVIDENCE CHECKPOINT EVERY WORK TURN. Only show meaningful genuine game output;
there is no rendered logo, title screen, main menu or worthwhile screenshot.

## Current source and recovery

This checkpoint extends main `9fc4085f346d49dbb1c3f549362a049b08505147`, exact base tree
`e4b64d3897ed83ea99ab4fde2176879ef32ea89b`. Publication/hosted results and the exact new
tree are in the appended downloadable receipt. Local Git is a reconstructed snapshot,
NOT remote history. Re-read main, reconcile concurrent work, and never force-push it.

The attached SOUND-SWIZZLE-PENDING archive passed 1,757 manifest and 471 source checks.
Newer main/Library already had procedural LUT uploads and the finite-prefix audio
file sink. That canonical 9fc4085 archive passed 1,078 manifest and 476 source checks,
matching its complete published tree before edits. Archive SHA256:
`a25d4a1cee9d1387a075b2174744d2ca97e14b7efef9bab0917779e2ebd37955`.
The older alternate remains unchanged in attachments and Library as
`Preserved-SOUND-SWIZZLE-PENDING.tgz/.md`; it was not overlaid onto newer source.

Existing code/AOT archive and prepared RomFS parts were mounted. Re-extracted original
603 AOT members and reassembled verified RomFS; no CCI extraction, new user upload or
user-PC access. Baseline full GCC build/all 67 tests and original language stop at
round 569 were reproduced. The prior LUT/upload/audio-file implementation is inherited.

## Actual new progress and next stop

New independent option: `--cfg-language en` (all accepted spellings below).
The original game performs three English language reads, returning byte 1 to its
actual destinations with exact IPC replies. All 64 captured neighbours, guest time,
thread count and handle count remain unchanged across each read. No console locale
is inferred or claimed recovered. Camera/sound settings remain independent.

Language-enabled execution revealed two missing compiled-code locations. First:
`0x00249B80`, a five-instruction original leaf. Second: `0x0024D3D0`, an original
callback-return continuation established by the preceding guest LR computation.
The latter has two basic blocks. Three PRIVATE build-generated blocks (13 words)
now cover these holes; no PC is skipped, return value invented or game word patched.

English full run with the supplementary build option:

```
stop=UnsupportedIpc pc=0x0025947c detail=0x32 thread=7 dispatch_rounds=592
last_ipc_session=dsp::DSP request_header=0x00130082
ipc_words=00130082 142f0d80 00000800 00000000 ffff8001 ...
cpu_ticks=44119742 core0_instructions=14421785 core1_instructions=99
quota_transitions=146 display_periods=9 guest_now_ns=164557222
DSP scheduled_slices=1195 notification_wait_slices=1 completed/attempted=19628032
```

This is DSP FlushDataCache for 2,048 bytes at `0x142F0D80`, current-process pseudo-handle
`0xFFFF8001`. It remains unsupported. Seven threads and 37 handles exist. French reaches
this operation at round 594; Spanish at round 592. Omitted language still stops at
round 569; omitted sound at 295; omitted camera at 235. Keep these branches distinct.

The final English audio file contains 4,786 complete stereo records, all actual FIFO
mask 3 and all-zero startup samples. No fallback frames, music, effects or playback.
VRAM is only the inherited depth clear: 819,200 bytes at offset `0x419400`, repeating
little-endian `0x00FFFFFF`; all other bytes zero. This is NOT a rendered image.
No HID sampling, executed game shader, logo/title/menu or gameplay is demonstrated.

## Language and supplemental-code contracts

CFG language block `0xA0002` is one byte, exact request `00010082 1 A0002 1C pointer`;
reply `00010042 0 1C pointer` with other words zero. Only the explicit language option
enables it. Pinned SystemLanguage values 0..11 map to:
ja, en, fr, de, it, es, zh-cn, ko, nl, pt, ru, zh-tw.
All are component/parse tested; original-game runs here cover en/fr/es, not every locale.
Other config fields are not invented. Existing pointer/descriptor/permission/physical
alias/write-preparation safeguards remain unchanged. Language cannot activate camera
or sound defaults, and neither other setting can activate language.

NEW BUILD INPUT: `-DLEGO_SUPPLEMENT_CODE_BIN=/mnt/data/lego_recovery/restored/code.bin`.
It is intentionally separate from runtime language selection. Without it, even with
a language selected, the original `MissingBlock 0x249B80` stop remains. Do not forget
this option and report a regression. No dynamic missing-PC decoder is installed.

`tools/generate_aot_supplement.py` authenticates complete code.bin, uses the existing
LLVM frontend/classifier, and emits static PRIVATE C++ only for reviewed ranges in
`config/observed_aot_supplements.json`. Generation requires Python, clang and llvm-objdump
at build time. The executable itself has no new runtime package/network dependency.
Every added raw word is checked again against authenticated code at launch. The
original 599 page units/all 603 saved AOT files are byte-identical, still linked.

`host/SupplementalRegistry` creates a sorted, owned block/shard index using existing
immutable operation arrays. Additions must fit original shard bounds and occupy
holes: overlaps, duplicates, unaligned/empty/out-of-range spans are rejected. Base
shards/blocks are never mutated. Existing opcode execution, dispatch, reservation,
missing-PC and Thumb guards remain. The wrapper is neither movable nor copyable;
its base/extra static op arrays outlive the index. No function result is hardcoded.

## Completed validation

Full GCC and Clang builds link 599 original AOT page units plus the private generated
supplement. All 69 configured CTest suites pass with each compiler and under ROM-free
Clang ASan/UBSan, with leak checks/halt-on-error and exact JUnit/configured name equality.
The previous 67 suites are unchanged. The two new suites cover language isolation,
all enum values, exact byte copyout, malformed requests, permissions, aliases, allocation
failure, reservations, registry merge/overlap/ranges/lifetime and synthetic execution.

Six ordinary compiler pairs match stdout/stderr/exit and owned gamecoin/audio files:
en, fr, es, unset language, no sound, no camera. Seven invalid-option pairs reject
before original code is opened; twelve valid CLI spellings parse on both compilers.
All 28 final capture files match byte-for-byte. Independent Python verifies three
language copyouts, final DSP request, exact depth-only VRAM and 4,786 tagged audio records.
Logging builds match ordinary CLI after normalizing ONLY the owned-root path.
This is not an independent ARM/DSP CPU oracle or a complete ARM address-space dump.

Generator checks cover invalid ranges and disassembly disagreement, plus every original
supplemental word. A separate test executes the actual generated leaf against a synthetic
X/Y buffer and confirms it returns X and advances the pointer exactly once without
changing source bytes/flags. It is component evidence, not original gameplay. Both
compiler-generated supplement source files are identical. Code, raw RomFS, ExHeader,
and all 603 original AOT members retain their recorded hashes. No full IVFC recheck,
Windows/macOS, playback, renderer, or measured hardware-timing validation occurred.

Current evidence: `language-checkpoint/` including `proof.json`, `identity.json`,
`matrix-results.json`, `matrix-*/`, `capture-pair/`, `cli-results.json`, complete final
build/configure/test logs and JUnit, compiler versions, generator checks, original-leaf
fixture, and setup notes. All current-turn job results are collected before delivery.
Intermediate language-only/missing-block runs remain separately labeled; do not use
those earlier stops or counts as the final result.

Production changes: CFG enum/read/constructor option, runner constructor wiring,
CLI parsing/diagnostics, supplemental registry header/declaration, reviewed address
manifest, build-time generator/CMake, two tests, and source reports/handoff. No CPU
or DSP opcode implementation, DSP live backend, GPU renderer, filesystem or scheduler
algorithm changed. No original game/AOT word was modified. Private generation and
capture output must stay OUT of public source publication.

## Exact next work

Inspect pinned DSP FlushDataCache's process-handle and address/size contract and the
host's actual memory/coherency model. Current request is above; do not merely invent
a cache or assume a successful external bus transaction. A coherent cacheless model
may need no byte movement, but validate that against current memory and process state.
External DSP AHB/FCRAM access, broader DSP lifecycle, playback, input and rasterization
remain separate unsupported areas. Follow the original next request after justified
implementation; do not force a render, interrupt, cache completion or pointer change.

All earlier opt-ins remain explicit. The language option is NOT the camera profile.
The audio file sink is needed to avoid the old finite in-memory capacity stop. ARM
one-recorded-instruction/core/tick, synchronous DSP boot/waits and scheduled slices
remain diagnostic conventions, not measured hardware latency or physical parallelism.
The title ExHeader verifies Multi/max30 only. Use singular `reference-slice`.

Primary language reference: Azahar `86a9f9236ae42bb5a2b995dbc933d599d8ea07ac`,
`src/core/hle/service/cfg/cfg.h` blob `4c5275a343d62005d305b55668fbede285607132`,
`cfg_defaults.cpp` blob `424664396a50c369dd7401b97372b924e28def2e` (Global/UserRead,
one-byte enum). Its default English is not silently enabled here. Supplement metadata
uses the existing local LLVM recovery frontend and original bytes. No PDF analysis
or new hardware measurement. Next DSP reference is the same pin's dsp_dsp.cpp.

## Scratch and reproducible commands

Root `/mnt/data/lego_recovery/`; source `repo/`; builds `build-gcc/`, `build-clang/`,
`build-asan/`; AOT `generated2/`; code `restored/code.bin`; RomFS
`game/prepared-romfs/romfs.bin`; ExHeader `prepared-launch/exheader.bin`;
raw parts/manifest `romfs-library-roundtrip/`; current work `language-checkpoint/`.
Restored canonical predecessor `restored-9fc4085/`; incoming archive `incoming-9fc4085/`.
Never extract predecessor source over current repo. Scratch can reset.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2 -DLEGO_SUPPLEMENT_CODE_BIN=/mnt/data/lego_recovery/restored/code.bin
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/cache-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty --romfs game/prepared-romfs/romfs.bin --gpu-vram-mode reference-zero --display-clock-mode reference-idle --cfg-profile reference-stereo --cfg-sound-mode stereo --cfg-language en --cpu-mode diagnostic-dual --block-limit 100000000 --exheader prepared-launch/exheader.bin --dsp-special-profile empty-config --dsp-executor live-teakra --dsp-reset-profile reference-zero-data --dsp-transmit-profile reference-stereo --dsp-audio-mode capture --dsp-boot-mode reference-slice --dsp-audio-file "$NEW_ROOT/audio.dspaud"
```

Expected exit 3: DSP FlushDataCache round 592 for English, not completed visual boot.
Clang uses clang++. ROM-free sanitizers omit BOTH AOT and supplement input options;
Debug -O1 with address/undefined instrumentation/frame pointers and matching linker
flags, leak checks/halt-on-error. `build_final.py` records exact commands. Supplemental
C++/JSON are under each build's `private-generated/` and can be regenerated. They
must not be published or substituted for all original AOT inputs.

Capture generation: `python language-checkpoint/make_capture.py gcc` (or clang).
`validate_pairs.py` and capture code require NEW output directories. New
`language-owned.*`/`language-paired.*` roots are owned TEST state, not recovered NAND.
Drivers remove only their own just-created, copied/hash-verified gamecoin/audio files
when pairing compilers on the same path. Never overwrite unknown saves or evidence.

## Durable recovery and mandatory delivery

GitHub `GTTeancum/Lego-Undercover-3DS-Recomp`; Library `/LEGO-Chase-Recovery/`.
The archive contains complete indexed source and current private evidence, small
prepared launch input, parts manifest, source/member index/verifier, exact patch
against 9fc4085 and actual publication/backup receipts. Verify the member manifest
and exact Git source blobs/modes, including tracked ignored reports, before use.
Never reconstruct fake upstream ancestry or force-push a snapshot. Earlier archives
stay separate; current source does not require recursively unpacking their history.

Separate Library inputs: `code.bin`; `LEGO-Chase-current-AOT-599pages-2026-10-03.tgz`
(unpacks generated2/); `Prepared-RomFS/` two raw parts plus manifest;
`Prepared-Launch/exheader.bin`. Parts 402653184+366526464 reassemble with
`repo/tools/restore_romfs_parts.py` into a NEW file. Raw RomFS 769179648 bytes, native
view offset4096/length769175552. Preserve integrity tables. No CCI extraction or
extra game upload is required. Original SHA256 identities:
code `5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f`;
AOT `2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9`;
RomFS `6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf`;
ExHeader `d7641f0a3bb89697ca8f0751e88d31703b54aa185ac78f9e74e41169dfcdc004`.

Raw original code, generated opcode words, DSP/PICA/SRAM/audio/ExHeader captures
NEVER enter public GitHub. Original code/AOT/rawRomFS/CCI, .git and compiled binaries
are excluded from the source/evidence archive. Private metadata/captures remain
private. Append only actually verified CI/publication/saved-file receipts. POST
UPDATED DOWNLOADABLE MARKDOWN AND RECOVERABLE SOURCE/EVIDENCE EVERY WORK TURN.
