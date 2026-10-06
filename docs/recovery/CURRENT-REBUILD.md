# LEGO Chase Begins — canonical continuation handoff

October 6, 2026. Work in assistant scratch, NOT the user's PC or Work.
LEGO City Undercover: The Chase Begins (3DS USA). Native/headless reconstruction,
NOT playable. Post updated downloadable Markdown and a recoverable source/evidence
checkpoint EVERY work turn. Only genuine meaningful game visuals; none exist here.

## Source and preservation

This checkpoint adds guarded, host-only execution of the original DSP firmware.
Published parent before this work: 3aca88f93a0305d37463525bca2bbd25893c188b, tree
368c2b830b8a19a3f10fd2d2fded9ed2672fc440. The attached DSP-SPECIAL-PENDING archive
was restored first: 795 manifest files, 393 source blobs, exact pending tree
0db6ecfb3f66abe608883b57ae64cde1c297cc80. All that pending work is retained.
Publication and Library verification are recorded in the appended delivery receipt.
Local Git is a reconstructed index/snapshot, NOT remote history. Re-read main and
use a fresh parent/force=false; never overwrite concurrently published work.

The prior pending archive/MD were saved separately in /LEGO-Chase-Recovery/ this
turn. Do not recursively duplicate history or extract older source over repo/.
Original code, private AOT and prepared RomFS parts were already mounted. No user
upload, original CCI extraction, user-PC or Work operation was necessary.

## Actual progress and limitations

New `--dsp-executor guarded-teakra` runs real original DSP instructions in an
isolated synchronous HOST PROBE. It does NOT acknowledge the guest LoadComponent
operation. The guest still stops at the same round 252 and PC 0x0025947C with
header0x001100C2, input49716 bytes at0x0037A120. Request and guest state stay untouched.

Known-only mode completes962 Teakra Run(1) calls, then faults on unknown SRAM byte
0x40AC0 (data offset0xAC0), PC0x291A ->0x291C. There are304 firmware word writes and
zero mailbox replies. Unknown allocation zeros are never accepted as valid input.

Additional explicit `--dsp-reset-profile reference-zero-data` assigns only DATA
bank gaps the pinned interpreter's zero-reset provenance. Program gaps remain
unknown; staged ordinary/special bytes take precedence. This is not recovered
console reset state or calibrated data. It cannot replace unresolved special CFG.
With this option, original firmware completes1424 Run(1) calls and432 word writes,
then stops at unmodeled MMIO offset0x20E, DSP PC0xD08 ->0xD0A. Error prints decimal526.
The original firmware has produced ZERO ready/pipe replies in either mode.

Run(1) counts are completed interpreter calls, not measured hardware latency or
necessarily one retired instruction each. Faulting attempts may have partial DSP
register/memory effects: faults seal the probe permanently, not roll it back.
No ARM memory bus, guest DSP SRAM mapping, live DSP schedule, pipe handling,
component-ready flag, input sampling or audio output is provided. The synthetic
mailbox test executes genuine DSP instructions but is NOT original boot proof.

ARM state remains cpu_ticks4945873, core0_instructions2287029, core1_instructions0,
guest_now_ns18447051, one display period, four threads,29 handles, maximum/current
application CpuTime30. The original core1 worker has still issued zero instructions.
VRAM remains entirely zero. No visible logo/title/menu, executed game shader or
playable gameplay has been demonstrated. Do not present blank/test screenshots.

## Execution backend and guards

Teakra MIT pin: wwylele/teakra@3d697a18df504f4677b65129d9ab14c7c597e3eb,
the submodule of Azahar@86a9f9236ae42bb5a2b995dbc933d599d8ea07ac. Container DNS blocked
direct downloads. An isolated public-source recovery workflow succeeded:
branch work/dsp-teakra-source, commitfe9d8a7a52b039b076b47d59468f07a29ac5a66e,
run37502002045, artifact11429593869. This branch does not itself update runtime main.
ZIP SHA f7864b967bd3c7350ebaeb4e91982409bdb44fcad9053f2e3915bc50d8ca18f9.
Tar SHA500803363bca8db5798f7a2e1cd75a003499bfc165a3b4a1c3a39adf96f7e815.
All130 upstream blobs verified; upstream tree2cb32e8deda4483e72091241874697101c7f7407.

repo/vendor/teakra-3d697a1 contains the offline build subset, MIT license, original
blob identities and local patch list in UPSTREAM.json. No network build dependency.
Five patches add raw-word range/access guards, an observer setter, caught assertions,
and strict default-unimplemented MMIO/getter/setter behavior. Moved default Cell
objects do not retain dangling lambda captures; MMIO indices are checked at access.
Decoded instruction algorithms remain the pin. No claim of complete peripheral,
reserved-bit, hardware-reset or DSP instruction correctness is made.

DspExecutionProbe owns512KiB memory and per-byte provenance (unknown/image/reference
reset/firmware-write). Unknown reads/fetches stop before consumption. Firmware
writes mark bytes known. External-memory or audio callbacks fail closed. Mailbox
history is256 words; steps capped100000 per Advance call. Valid paused slices resume;
a terminal fault never retries partial state. This is an isolated probe, not a
rollback-capable device transaction. Allocation failure does not replace a prior probe.

DspBootHandshake reads actual backend mailboxes in order: reg0=1,reg1=1,reg2=1,
then a DISTINCT reg2 pipe word. No host-generated ready response exists. Even a
ProtocolComplete probe still makes the service stop without a successful IPC reply.
`--dsp-probe-steps N` requires executor selection; range1..100000. Duplicate/invalid
options reject launch. No executor is the original default inspection-only behavior.

Prior special configuration remains explicit: --dsp-special-profile empty-config
models an empty host source for block0x70000 and its documented532-byte fallback;
--dsp-special-block FILE requires exactly532 supplied bytes and never silently falls
back on file errors. These options are exclusive. They do not change guest CFG stereo.
All prior PTM/VRAM/display/CFG/CPU/ExHeader options remain explicit and unchanged.

## Tests and evidence

Full GCC and Clang builds link599 unchanged private AOT page units. All53 suites
pass with each compiler; all53 ROM-free Clang ASan/UBSan suites pass with leak checks
and halt-on-error. Existing52 suites remain unchanged. New suite uses real DSP
opcodes for SRAM reads/writes and mailbox replies, known/unknown/reset policies,
resumption, partial faults, lifetime/range/allocation guards and no guest success.

Four paired original cases pass: guarded-known-only, reference-zero-data, bounded100,
and executor-disabled. Eleven paired invalid-option cases pass. Every capture byte
matches across GCC/Clang:84 files per probe mode, plus complete trace stdout. Before/
after DSP call comparisons preserve IPC/input/ARM CPU/thread/time, GPU1842 words,
uploads, GSP/HID pages and epochs, mapped HID, LCD and full6MiB VRAM. Independent
provenance checks prove changed SRAM bytes were written:210 bytes change in known-only,
285 in reference-data. Distinct written bytes588/732. This is not an independent CPU
oracle or a complete ARM address-space dump. No new guest boot progress is claimed.

Code.bin, raw RomFS, original ExHeader and all603 regular AOT members reverified.
Full IVFC checking not repeated. Registry111043 blocks/545111 words remains STATIC
inventory, not frame counts. No Windows/macOS build. Final GCC/Clang/sanitizer suites
passed after the MMIO lifetime fix. Setup/prototype/initial logs are separate from
final results. Streaming sessions unavailable; bounded ordinary build calls used.

Evidence: dsp-exec-checkpoint/. baseline*, build-final-*/test-final-* logs,
build-others.json, asan-final.json, run_cases.py/case-*, verify_options.py/
cli-validation.json, capture.py/trace_support.inc/trace_dual_runner.cpp,
capture-guarded-{gcc,clang},capture-reference-{gcc,clang},verify_captures.py/proof.json,
verify_inputs.py/identity.json,teakra-recovery.json,setup-notes.txt. Alternate trace
runner only adds logging; production CLI independently reproduces the same stops.
All raw original component/SRAM/provenance captures are PRIVATE.

## Next exact work

The pinned Teakra src/icu.md and src/mmio.cpp leave0x20E/0x210 interrupt polarity/
source type uncertain. Inspect the original firmware accesses and a primary hardware
reference/test before adding state/interrupt semantics. Do not replace the stop with
silent generic register storage. Continue bounded real firmware execution toward the
actual ready/pipe replies. A completed probe handshake alone is still not sufficient
for guest success: live bus, pipe, scheduling and lifecycle contracts remain necessary.
Keep known-only and explicit reference-reset comparisons and all current safeguards.

## Scratch and repeatable run

Root /mnt/data/lego_recovery/. Source repo/. Builds build-gcc/,build-clang/,build-asan/.
AOT generated2/; code restored/code.bin; RomFS game/prepared-romfs/romfs.bin;
ExHeader prepared-launch/exheader.bin; parts/manifest romfs-library-roundtrip/.
Upstream teakra-upstream/; exact recovery files teakra-recovery/;
ZIP /mnt/data/LEGO-Teakra-source-3d697a1.zip. Current work dsp-exec-checkpoint/.
Previous dsp-boot-checkpoint/ contains predecessor proof, not newly run results.
New dsp-exec-* and dsp-exec-capture.* roots are owned TEST state, not console NAND.
Never overwrite unknown saves/captures. Paired drivers remove only their own new,
size/hash-verified20-byte gamecoin to repeat on the same path.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 2
ctest --test-dir build-gcc --output-on-failure
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/dsp-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty --romfs game/prepared-romfs/romfs.bin --gpu-vram-mode reference-zero --display-clock-mode reference-idle --cfg-profile reference-stereo --cpu-mode diagnostic-dual --block-limit 100000000 --exheader prepared-launch/exheader.bin --dsp-special-profile empty-config --dsp-executor guarded-teakra --dsp-reset-profile reference-zero-data
```

Expected exit3, DSP MMIO0x20E stop, still ARM round252. Omit reset option/value for
known-only fault. Omit both new options for predecessor inspection-only comparison.
Use clang++ for Clang; omit AOT path for sanitizers. Diagnostic ARM execution remains
one recorded instruction/core/nominal tick, not hardware-accurate timing.

## Durable recovery

GitHub GTTeancum/Lego-Undercover-3DS-Recomp main; Library /LEGO-Chase-Recovery/.
Verify CHECKPOINT-MANIFEST.json before extraction and SOURCE-INDEX.json for all exact
Git paths/modes/blobs (including ignored reports). Check patch against parent3aca88f.
The new archive is complete source plus current private evidence and prepared-launch,
not compiled code/AOT/ROM. Preserve predecessor DSP-SPECIAL-PENDING and3aca88f archives
separately in Library. Source-tree restoration must not invent remote ancestry.

Large private inputs already backed up: code.bin, LEGO-Chase-current-AOT-599pages-
2026-10-03.tgz (unpacks generated2/), Prepared-RomFS/ two raw parts+manifest,
Prepared-Launch/exheader.bin. RomFS parts402653184+366526464; restore with
repo/tools/restore_romfs_parts.py, never overwrite existing output. Raw769179648 bytes,
native view offset4096/length769175552. No original CCI extraction needed.
Code SHA5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
AOT SHA2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9.
RomFS SHA6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf.
ExHeader SHAd7641f0a3bb89697ca8f0751e88d31703b54aa185ac78f9e74e41169dfcdc004.
DSP component SHA7ea3c44a1c57514bebbebce8a7995f7f3a290170ea3b6c145749ad9358672c97.

Raw component/bank captures and ExHeader NEVER go to public GitHub. Scratch may reset;
Library/attachments/GitHub are recovery paths, not permanent scratch. Actions artifacts
expire after30 days. Append actual publication/CI/backup receipts only after success.
Post updated downloadable Markdown and source/evidence EVERY work turn.
