# LEGO Chase Begins — canonical continuation handoff

October6,2026 (user-local date). Work in assistant scratch, NOT the user's PC or Work.
LEGO City Undercover: The Chase Begins, Nintendo3DS USA. Headless native reconstruction,
not playable. POST UPDATED DOWNLOADABLE MARKDOWN AND COMPLETE SOURCE/EVIDENCE EVERY WORK
TURN. Only meaningful genuine game visuals. There is no rendered logo/title/menu/frame.

## Source and recovery

This checkpoint extends published4be383cba5c8b370e5ca45150a098618203ac410,
base tree331f4e9a429b3ec7314a8000d89d94d67869204b. The final receipt appended after
publication records its actual commit/tree and backup. Local Git is a reconstructed
index/snapshot, NOT remote ancestry. Re-read main and use a correct fresh parent,
force=false; never overwrite concurrently published work.

The task arrived with DSP-BTDMP-PENDING, treee4041841799f64d33d3e235153600c910e27f884,
458 source files/743 manifest entries. It was verified and preserved separately.
Newer main had a different explicit transmitter/IRQ/fallback policy, not just reports.
Recovered main from hosted run37551868368, artifact11453495226:
LEGO-source-4be383c-hosted.zip SHA256
cf31b93fd4cc180dd4bdd1918aa50428df849199ced65fc824b0888ae7cf23ff.
All479 inner manifest files and457 source blobs reconstructed the exact main tree.
Current work uses that published source. The older pending variant was newly saved
as /LEGO-Chase-Recovery/Preserved-DSP-BTDMP-PENDING.tgz and .md, not overwritten.

Original code/AOT/parts were already mounted. Extracted unchanged generated2/ and
reassembled prepared RomFS into a new file; restored prepared launch from the previous
checkpoint. No original CCI extraction, user reupload, user-PC or Work access.

## Actual breakthrough: firmware consumes startup message and replies

NEW explicit option: --dsp-boot-mode reference-slice (requires live-teakra).
The original firmware completes boot in actual16384-call polling batches. Ready
words0:1,1:1,2:1 are consumed at16384; the distinct pipe-base word3230/0x0C9E at32768.
These are firmware-generated values, not constants, replay or a fixed title delay.
The same interpreter/SRAM/validated descriptors are then published as the live device.

The first scheduled slice consumes the original four-byte startup message: slot5
read0/write4 becomes read4/write4. The second writes32 bytes into slot4, write0->32,
and emits real peripheral notifications. Captures prove the original main thread
moves WaitSynchAny->Ready/pending_wake through its retained registered EventObject,
then executes again. No host pointer edits, forced wake or fabricated reply bytes.

Current ordinary stopping point:

```
dsp_boot_mode=reference-slice mailbox_poll_calls=16384 hardware_timing=unverified
dsp_audio_mode=capture frames=10 underflow=stop playback=none capacity=4096
dsp_live_loaded=1 data_base=0x1ff40000 scheduled_slices=2 next_deadline_ns=18752595
dsp_probe_completed_steps=65536 attempted_steps=65536 written_words=33231
cpu_ticks=4995105 core0_instructions=2287464 core1_instructions=0
guest_now_ns=18630676 display_periods=1 current/max_cpu=30/30
stop=UnsupportedIpc pc=0x0025947c detail=0x32 thread=1 dispatch_rounds=261
last_ipc_session=dsp::DSP request_header=0x001000c0
ipc_words=001000c0 00000002 00000000 00000002 00000000 ...
ipc_static_buffer0=00008002 0ffff5a0
```

This is ReadPipeIfPossible, channel2/peer0/length2. The command, CPU and destination
(including captured neighbours) remain untouched. Slot4 is read0/write32; no read
response has been supplied. Its32 bytes have firmware-write provenance; first word15,
SHA2564cb606067ddd1281cd0f379eb999f1c0904a427848b9547c23d72123f8f134bc.
No further interpretation or completed game audio initialization is claimed.

Ten captured stereo pairs are zero startup FIFO output, NOT music or sound effects.
Two pairs occur during synchronous boot, eight afterward. Every channel has actual
FIFO provenance; there are zero tagged underrun replacements on this new path.
The original core1 worker still executes ZERO instructions before its eligible window.
VRAM remains allzero. No audible playback, HID samples, rendered game shader/frame,
logo, title screen, main menu or gameplay exists. Do not post silent/blank captures.

## Why the old path stalled and exact fix

Previous immediate polling returned the pipe word after3796 Run(1) calls, before
firmware callback setup. Logging-only traces show mailbox2's slot5 command arriving
at DSP handler0x4DFC; its pipe callback is null and0x4E1D branches to0x4E2D. Firmware
later installs callbacks through0x4E34. The earlier notification is already drained,
leaving payload queued. A diagnostic delayed-delivery experiment changed this path,
but that artificial delay was NOT used as production behavior.

Pinned Azahar RunTeakraSlice executes16384 calls whenever the expected mailbox is
not ready; it polls only after the full slice. ReferenceSlice follows that cadence
and retains unfinished slice state across host budgets. Budget exhaustion does not
poll early. A post-reply fault within the slice prevents successful completion. The
existing real ready/pipe controller, table validation and mapping still gate live load.
Actual FIFO output during boot is retained only through the explicitly selected
bounded consumer; missing consumer faults, never silently discards output.

Immediate mode remains default for historical comparison and still stalls/underruns.
Known-only stops on unknown SRAM; no implicit zero memory added. reference-slice is
an explicit pinned-loader timing policy, not verified hardware instruction latency.
Boot is synchronous/uncharged to ARM time, as before. Live scheduling and diagnostic
ARM one-recorded-instruction/core/tick are unchanged, not cycle-accurate hardware.

The newly reached original firmware writes transmit0x010F. GBATEK's bits8..11 IRQ
nibble is zero=disabled/nonzero=enabled (alternate encodings remain uncertain). Only
observed0x000F/0x010F plus disabled reset5 are supported. FIFO-empty notifications
are now gated by0x010F. Toggling the field cannot drain/reset FIFO or inject IRQ;
empty fallback does not generate one. Reference threshold/4096-call output period,
channel order, held-line and alternate-format behavior remain unverified. No generic
register storage, increased capture capacity or replacement waveform unblocks startup.

## Tests and evidence

Full GCC/Clang native executables link599 unchanged private AOT page units. All61
CTest suites pass with both compilers and all61 ROM-free Clang ASan/UBSan suites pass
with leak checks/halt-on-error. JUnit/configured names match without skips/duplicates.
No old suite removed. New ctr_dsp_boot_slice_test covers exact boundaries, partial
budgets, chunk equivalence, distinct reg2 words, terminal faults, actual boot output,
missing consumer and mode guards. BTDMP tests cover all65536 control and sample words,
gating transitions, untouched FIFO and no false write/flush/underflow interrupt.

Six ordinary startup pairs match stdout/stderr/exit/new test-file bytes: reference,
immediate, known-only, bounded100, missing sink and guarded probe. Six pre-input CLI
rejection pairs match. All333 capture files match across compilers. Independent parsing
checks descriptors/provenance, actual consumption/response/wake, same DATA mapping,
untouched next IPC/output and unchanged GPU1842 words/uploads/GSP page/epochs/HID/LCD/
6MiB VRAM. Changed DSP bytes across scheduled slices carry firmware-write provenance.
This is not a complete ARM-space dump or independent DSP/ARM CPU oracle.

Original code, raw RomFS, ExHeader and all603 AOT backup members retain identities.
No full IVFC recheck, Windows/macOS build or playback test. Initial changed-source
CTest failed obsolete BTDMP assumptions; corrected explicit tests pass. Finite builds
needed resumptions. Setup/prototype/initial logs are preserved and not counted as final.
Hosted CI results are appended only after actual completion.

Current work/evidence: /mnt/data/lego_recovery/dsp-dispatch-checkpoint/.
Final: proof.json/verify.py, identity.json, tests.json, junit-final-*.xml and logs,
matrix/ (six paired receipts), matrix-summary.json, cli/ and cli-proof.json.
make_captures.py/trace_support.inc regenerate logging-only scheduler objects from
current production source; capture-reference-gcc/clang each have333 files. Ordinary
production CLI binaries independently reproduce the same next request.
Early trace_driver.cpp/make_trace.py, immediate/delayed traces and batch prototypes
are separate diagnostic experiments against pre-fix source. Their exact generated
source is retained. Do not treat them as the final original ARM path or copy their
manual message delivery into production. Raw disassembly/SRAM/component stay private.

## Next exact work

Implement observed DSP ReadPipeIfPossible channel2, peer0, length2, static output
descriptor0x8002 at TLS+0x100, pointer0x0FFFF5A0. Inspect the pinned service's actual
reply header/static-buffer contract, length semantics and preflight requirements.
Use the real live device ReadPipe data/pointer/mailbox methods. Validate writable
output, buffer size, address overflow and aliases before consuming any pipe bytes.
Do not substitute a fixed15 or guessed address list, prematurely change pointers,
or force another event. Follow the next actual request after testing unchanged code.
HID sampling, live external AHB/FCRAM, broader DSP lifecycle/playback and rendering
remain separate unimplemented areas. No additional original-game upload is needed.

References: Azahar86a9f9236ae42bb5a2b995dbc933d599d8ea07ac, src/audio_core/lle/lle.cpp
blob388fe64ec1a5130a2c93a5dfa04ca84df109b567 (slice size/boot readiness/pipe base).
MIT Teakra pin3d697a18df504f4677b65129d9ab14c7c597e3eb is retained. Local patch hashes
are in vendor/teakra-3d697a1/UPSTREAM.json. GBATEK no$gba3.03 BTDMP printed365-367,
PDF390-392 zero-based: https://pcy.be/tmp/img/gbatek.pdf. Parsed text was available;
screenshot attempts failed. No page-image inspection or new3DS hardware measurement.

## Scratch and repeatable launch

Root /mnt/data/lego_recovery/. Current repo/. Builds build-gcc/,build-clang/,build-asan/.
AOT generated2/; code restored/code.bin; RomFS game/prepared-romfs/romfs.bin;
ExHeader prepared-launch/exheader.bin; raw parts/manifest romfs-library-roundtrip/.
Older source restored-main-4be383c/ and preserved-btdmp-pending/ stay separate.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 3
ctest --test-dir build-gcc --output-on-failure
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/pipe-read-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty --romfs game/prepared-romfs/romfs.bin --gpu-vram-mode reference-zero --display-clock-mode reference-idle --cfg-profile reference-stereo --cpu-mode diagnostic-dual --block-limit 100000000 --exheader prepared-launch/exheader.bin --dsp-special-profile empty-config --dsp-executor live-teakra --dsp-reset-profile reference-zero-data --dsp-transmit-profile reference-stereo --dsp-audio-mode capture --dsp-boot-mode reference-slice
```

Expected exit3, pending0x001000C0 round261, not an audio/visual completed boot.
Omit only boot-mode/value for immediate-polling comparison. Known-only omits reset;
probe requires omitting live-only boot/transmitter/audio modes. Keep all policies
explicit. For Clang use clang++; sanitizer build omits AOT, uses Debug-O1 plus
-fsanitize=address,undefined -fno-omit-frame-pointer and leak checks/halt-on-error.
Build/test recipes are build.py/test_all.py; bounded build invocations collect their
process groups. Matrix script takes index ranges0..6 and refuses existing case paths.
Captures also require new folders. Newly created dispatch-* roots are owned TEST
state, not recovered NAND. Delete only your own hash-verified new20-byte gamecoin
when pairing compilers. Never overwrite unknown saves or evidence.

## Durable recovery and mandatory delivery

GitHub GTTeancum/Lego-Undercover-3DS-Recomp; Library /LEGO-Chase-Recovery/.
Verify CHECKPOINT-MANIFEST.json and SOURCE-INDEX.json modes/blobs before use. Restore
all indexed files including ignored tracked reports; do not reconstruct fake ancestry.
The full patch applies to exact parent4be383c. Publication/backup identities are in
the appended receipt. Current archive is complete source, current private evidence,
prepared launch, parts manifest, source/member manifests, verifier and exact patch.
Older source archives remain separate references, not recursively copied.

Original inputs remain separately backed up: code.bin; LEGO-Chase-current-AOT-599pages-
2026-10-03.tgz (unpacks generated2/); Prepared-RomFS/two raw parts+romfs-parts.json;
Prepared-Launch/exheader.bin. Part sizes402653184+366526464. Reassemble with
repo/tools/restore_romfs_parts.py, refusing overwrite. Raw RomFS769179648 bytes,
native view offset4096/length769175552; preserve integrity tables. No CCI extraction.
Code SHA5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
AOT SHA2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9.
RomFS SHA6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf.
ExHeader SHAd7641f0a3bb89697ca8f0751e88d31703b54aa185ac78f9e74e41169dfcdc004.
DSP SHA7ea3c44a1c57514bebbebce8a7995f7f3a290170ea3b6c145749ad9358672c97.

Raw firmware/SRAM/ExHeader/audio/disassembly NEVER go to public GitHub. No code.bin,
AOT, raw RomFS/CCI, .git, native binaries/build products or font files are in the
source/evidence archive. Scratch may reset; Library/attachments/GitHub are recovery
routes, not permanent scratch. Append only verified publication/CI/round-trip receipts.
POST UPDATED DOWNLOADABLE MARKDOWN AND SOURCE/EVIDENCE AT THE END OF EVERY WORK TURN.
