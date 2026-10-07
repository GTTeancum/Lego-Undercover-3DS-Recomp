# LEGO Chase Begins — canonical continuation handoff

October 6, 2026 (user-local date). Continue in assistant scratch, NOT the user's PC
or Work. Nintendo 3DS USA: LEGO City Undercover: The Chase Begins. Headless native
startup reconstruction, NOT playable. POST UPDATED DOWNLOADABLE MARKDOWN AND COMPLETE
SOURCE/EVIDENCE EVERY WORK TURN. No rendered logo, title screen, main menu or useful
screenshot exists; only present meaningful genuine game visuals.

## Canonical source and preservation

This checkpoint extends published main `5e7b03ed29724a7647c2b86cfedf4e99cb9b70c7`,
exact baseline tree `a4fa09ba5120c094b15b2692f788e3f3cccbadda`. The downloadable
receipt appended after publication records the final commit/tree, hosted results
and actual backup. Local Git is a reconstructed index/snapshot, NOT remote ancestry.
Read current main and reconcile concurrent changes before non-forced publication.
Never replace newer remote source or push fabricated local history.

The attached older DSP-BOOT-POLLING-PENDING archive was verified: 1623 manifest
files, 460 source blobs, tree00a824b2579ec9094d4764a9991e2c5448ca185f. It stopped
on control0x010F and used reference-slices (plural). Newer main already fixed that
with a different reference-slice (singular) API and genuine firmware pipe reply.
The old source is preserved separately, NOT silently merged or overlaid. It was
saved to Library as Preserved-DSP-BOOT-POLLING-PENDING.tgz and .md this turn.
Archive SHA612cb29aa71d82981f035c6c998e99aed18f35970a02a80f81650ef60479f369.

Newer main was recovered from Actions37556875223/artifact11455397169. ZIP
LEGO-source-5e7b03e.zip SHA bf0b778c63117493ee9d1a0678abc6188e6c404fb760ca8e6189f17543e4d2ff.
Inner tar SHA95cb361229b4f4dc14b74c23f56db9777198068eaf3762cf3f88544bf24ce80f.
All462 source paths reconstructed the exact baseline tree. Full GCC baseline/all61
suites and ordinary original pipe-read stop were reproduced before new edits.
Existing original inputs were restored/verified; no user upload, CCI extraction,
user-PC or Work access was needed.

## Actual new original-game result

The baseline already booted the original DSP firmware, consumed the startup message,
wrote its32-byte reply and woke the registered original ARM thread. Those are inherited
results, not new work here. This turn implements ReadPipeIfPossible and address conversion.

- Round261 reads2 bytes to0x0FFFF5A0, returning the actual first firmware word15.
- Round262 reads30 bytes to0x0FFFF5E8. All32 bytes match the firmware response SHA
  4cb606067ddd1281cd0f379eb999f1c0904a427848b9547c23d72123f8f134bc; neighbours stay unchanged.
  CPU-owned slot4 read pointer changes0->2->32. Nothing is substituted or hardcoded.
- The second read needs ONE complete16384-call synchronous DSP quantum to clear the
  incoming mailbox, then sends the actual slot4 notification. No mailbox forced-clear.
- Rounds263..292 translate30 actual words:15 firmware-returned locations and alternate
  banks. Every reply is0x1FF40000+2*word within the SAME retained live DATA mapping.
- Round293 sets the live semaphore; round294 is the ORIGINAL GAME'S first observed
  signal of its exported semaphore event. The real existing notifier delivers it.

Next untouched request:

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x32 thread=1 dispatch_rounds=295
last_ipc_session=cfg:u request_header=0x00010082
ipc_words=00010082 00000001 00070001 0000001c 0ffff658 ...
```

This is GetConfig SoundOutputMode block0x70001, size1. Its request and output/neighbours
remain unchanged. No sound preference has been returned or inferred from the user.

Final counters: scheduled_slices2, notification_wait_slices1, completed/attempted DSP
Run(1) calls81920; core0 recorded instructions2289950/core1 ZERO; CPU ticks4997591;
guest time18639948ns; one display period, four threads,31 handles, current/maxCPU30.
Fourteen FIFO-provenance stereo pairs are zero startup silence; no fallback frames,
music, playback, HID samples, rendered game shader/frame, logo, menu or gameplay.
All6MiB VRAM remains zero. Do not show blank or synthetic screenshots as game output.

## Implementation and bounded contracts

ReadPipeIfPossible supports only exact0x001000C0, channel2, peer0 and low-u16 length
0..128. Upper size bits are ignored as in the pin. It returns all requested bytes
or zero when insufficient data exists; it does not return a shorter partial read.
Static output buffer0 is at TLS+0x180 (command buffer+0x100), NOT TLS+0x100. The
reply is0x00100082/Result0/actual_size/(actual_size<<14|2)/destination, rest zero.

Preflight covers full reply readability/writability, static descriptor type/capacity,
output overflow/writability and backing aliases against reply/entire receive table.
Only private writable output is supported, not shared/device banks. Existing write
preparation reserves metadata before pipe consumption; commit updates actual exclusive
reservation epochs. Invalid buffers/allocation failures cannot consume payload.

The device stages bytes and commits ONLY its CPU-owned read pointer before waiting
for the incoming mailbox, matching pinned LLE ordering. Synchronous waits run actual
firmware in full quanta, at most4 per call (host bound). Failure after pointer commit
retains partial DSP effects and faults the device, without successful copyout/reply.
It is not rollback; retries cannot consume additional bytes. The old nonblocking
ReadPipe default is preserved for existing callers. Postcommit event/copyout invariant
failures likewise cannot be described as rollback.

Synchronous wait quanta leave ARM time and scheduled deadlines unchanged, consistent
with the reference convention but NOT measured hardware timing. The separate counter
avoids claiming them as scheduled slices. Actual emitted notifications use the same
retained-event delivery as scheduled execution; no read-completion IRQ is invented.

ConvertProcessAddressFromDspDram supports exact0x000C0040 within the17-bit DATA word
range and healthy attached live device. It returns0x000C0080/0/base+2*word and does
not dereference/initialize unknown SRAM. Broader address behavior is unsupported.

Changed source: CMakeLists.txt, cmake/LEGOHostRuntime.cmake, host/main.cpp diagnostics,
services/dsp_discovery_service.h/.cpp, dsp_live_device.h/.cpp, new dsp_pipe_service.cpp,
and new ctr_dsp_pipe_read_test.cpp/ctr_dsp_address_test.cpp. No vendor instruction or
peripheral, ARM scheduler, game/AOT opcode, GPU drawing, HID sampler, filesystem or
CFG content changes. All inherited policies and memory/provenance guards remain.

## Completed validation and evidence

Full GCC/Clang builds link all599 unchanged private AOT page units. All63 configured
CTest suites pass under each compiler and all63 ROM-free Clang ASan/UBSan suites pass
with leak checks/halt-on-error. JUnit names equal configured names without skips or
duplicates. The previous61 suites remain intact. Synthetic cases cover actual DSP
mailbox progress/notification, all byte values/lengths/wrap, short reads, four-quantum
bounds, real unknown-memory faults, retained partial effects/retry, protected/aliased
output, allocations, exclusive epochs and all131072 bounded word-address conversions.

Six ordinary paired original runs match stdout/stderr/exit and owned file bytes:
full, immediate boot, known-only, noCFG, guarded probe, strict CPU. All291 final
capture files match GCC to Clang. Independent parsing confirms exact output bytes,
30 conversions, real semaphore event signal, untouched CFG request/output and stable
GPU1842 words/uploads/GSP/HID/LCD/VRAM. Logging-only capture hosts link the production
libraries/AOT; normal CLI independently reaches the same stop. This is not a complete
ARM address-space dump or independent CPU oracle. No Windows/macOS or playback test.
Original code, full raw RomFS, ExHeader and all603 AOT archive members retain identities.
Full IVFC checking was not repeated. Registry111043 blocks/545111 words is static
inventory, not execution/frame/progress counts. Hosted receipts are appended only
after actual verification. Setup/intermediate logs are separate from final passing runs.

Evidence: /mnt/data/lego_recovery/pipe-read-checkpoint/. tests.json and final JUnit/logs;
validate_matrix.py/matrix.json/matrix/; make_captures.py/inherited_capture_support.inc/
current generated capture sources; capture-gcc-final/ and capture-clang-final/;
verify_captures.py/proof.json; verify_inputs.py/identity.json; run_build.py, test_all.py,
run_original.py/run_capture.py; restore.json/references.json/setup-notes.txt.
Native/trace binaries and object files are excluded and rebuildable. All current-turn
jobs completed, not left running for later delivery. Choose NEW evidence/root paths;
never overwrite unknown saves or captures. Pair scripts remove only their own new
hash-verified20-byte gamecoin when repeating on the identical path with another compiler.

## Next exact work

Implement the observed one-byte CFG sound-output block only with a justified explicit
profile/source. Pinned cfg.h defines mono0, stereo1, surround2; cfg_defaults.cpp uses
its own compatibility default. Do not silently expand reference-stereo (currently
stereo-camera block only) into an unrequested user preference or claim recovered NAND.
Inspect the config source/response handling, add precise option and service tests,
then rerun unchanged original code in NEW test state to observe the next request.
Do not guess the next DSP/audio step or mark initialization complete. HID sampling,
external AHB/FCRAM, broader DSP lifecycle, playback and GPU rendering remain separate.

Primary pin: azahar-emu/azahar@86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
DSP service src/core/hle/service/dsp/dsp_dsp.cpp blobf8b23c07925c6b4a9fe36acaa7954f05126441f5;
LLE src/audio_core/lle/lle.cpp blob388fe64ec1a5130a2c93a5dfa04ca84df109b567;
CFG src/core/hle/service/cfg/cfg.h blob4c5275a343d62005d305b55668fbede285607132.
Exact inspected scopes are in references.json. No new hardware or PDF analysis.

## Scratch and reproducible launch

Root /mnt/data/lego_recovery/. Source repo/; builds build-gcc/,build-clang/,build-asan/;
AOT generated2/; code restored/code.bin; raw RomFS game/prepared-romfs/romfs.bin;
ExHeader prepared-launch/exheader.bin; parts/manifest romfs-library-roundtrip/.
Baseline source snapshot-5e7b03e/unpacked/; older pending preserved-boot-pending/.
Historical source is NEVER extracted over current repo/. Work pipe-read-checkpoint/.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 3
ctest --test-dir build-gcc --output-on-failure
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/sound-mode-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty --romfs game/prepared-romfs/romfs.bin --gpu-vram-mode reference-zero --display-clock-mode reference-idle --cfg-profile reference-stereo --cpu-mode diagnostic-dual --block-limit 100000000 --exheader prepared-launch/exheader.bin --dsp-special-profile empty-config --dsp-executor live-teakra --dsp-reset-profile reference-zero-data --dsp-transmit-profile reference-stereo --dsp-audio-mode capture --dsp-boot-mode reference-slice
```

Expected exit3, CFG0x70001 at round295. Omit ONLY boot-mode/value for immediate
comparison. Known-only omits reset option/value; guarded/strict runs require omitting
incompatible live-only modes. Preserve explicit policies. Clang uses clang++.
Sanitizers omit AOT, Debug-O1 with -fsanitize=address,undefined -fno-omit-frame-pointer;
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 and UBSAN_OPTIONS=halt_on_error=1.
The ARM one-recorded-instruction/core/tick and DSP boot/wait/event timing remain
reference diagnostic assumptions, not hardware-accurate cycles or host parallelism.

## Durable recovery and mandatory delivery

GitHub GTTeancum/Lego-Undercover-3DS-Recomp main; Library /LEGO-Chase-Recovery/.
The current archive includes complete indexed source, current private evidence,
prepared launch, RomFS parts manifest, baseline source ZIP, source/member index and
verifier, exact patch against5e7b03e and actual publication/backup receipts. Verify
CHECKPOINT-MANIFEST.json then SOURCE-INDEX.json exact paths/modes/Git blobs, including
ignored tracked reports. A separate extraction/patch application must reproduce the
same tree. Never reconstruct fake upstream ancestry. Historical archives stay separate.

Original private Library inputs: code.bin; LEGO-Chase-current-AOT-599pages-2026-10-03.tgz
(unpacks generated2/); Prepared-RomFS/ two raw parts+manifest; Prepared-Launch/exheader.bin.
Parts402653184+366526464 restore through repo/tools/restore_romfs_parts.py, refusing
overwrite. Raw RomFS769179648 bytes, native view offset4096/size769175552; preserve
integrity tables. No original CCI extraction or additional game upload is needed.
Code SHA5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
AOT SHA2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9.
RomFS SHA6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf.
ExHeader SHAd7641f0a3bb89697ca8f0751e88d31703b54aa185ac78f9e74e41169dfcdc004.
DSP SHA7ea3c44a1c57514bebbebce8a7995f7f3a290170ea3b6c145749ad9358672c97.

Raw original DSP/SRAM/ExHeader/sample captures NEVER enter public GitHub. Original
code.bin/AOT/rawRomFS/CCI, .git, build/native binaries and font files are excluded.
Scratch may reset; attachments/Library/GitHub are recovery paths, not permanent
scratch. Append only actually verified publication/CI/round-trip receipts. POST
UPDATED DOWNLOADABLE MARKDOWN AND RECOVERABLE SOURCE/EVIDENCE EVERY WORK TURN.
