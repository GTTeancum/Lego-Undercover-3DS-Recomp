# LEGO Chase Begins — canonical continuation handoff

October 6, 2026. Continue in assistant scratch, NOT the user's PC or Work.
LEGO City Undercover: The Chase Begins (Nintendo3DS USA). Headless native startup
reconstruction, NOT playable. POST AN UPDATED DOWNLOADABLE MARKDOWN AND COMPLETE
SOURCE/EVIDENCE CHECKPOINT EVERY WORK TURN. Only meaningful genuine game visuals;
there is still no rendered logo, title screen, main menu, frame, audio or gameplay.

## Source and preservation

This checkpoint adds semaphore/event notification and the observed audio-startup
pipe request over DSP-LIVE-PENDING, tree d3b4ec4a1d9ede5f72aba7ba812c66b439b20e5f.
Published parent when work began:50b2942120f47d221cf66e662961165cf031bd20,
tree c958adbde76565fe614eb97fdad922895a5f1d91. All intervening pending ICU/live work
is preserved. The final downloadable receipt records this checkpoint's publication,
exact tree, archive, hosted checks and Library save; do not invent those results.
Re-read main and reconcile changes before non-forced publication. Local Git is a
reconstructed index/snapshot, NOT remote history; never force-push it.

The attached live archive was verified:941 manifest files,450 source blobs and exact
pending tree. It and its handoff were also saved to Library this turn. Original
code/AOT archive and prepared RomFS parts were already mounted. Restored source,
re-extracted unchanged generated2/ and reassembled the existing parts with hash
verification. No CCI extraction, unnecessary reupload or user-PC access was needed.
The full baseline reproduced the untouched GetSemaphoreEventHandle at round255.

## Actual original-game progress and current stop

The existing live device boots the original DSP program, preserves its interpreter
and SRAM, and validates16 firmware-written descriptors at pipe-base word0x0C9E.
New successful original calls:

- round255 GetSemaphoreEventHandle:reply0x00160042/Result0/copy0/handle0x000F002D.
- round256 SetSemaphoreMask(0x2000):reply0x00170040/0; preset only, no signal.
- round257 WriteProcessPipe(pipe2,size4):reply0x000D0040/0; actual slot5 write.
- round258 SetSemaphore(0x4000):reply0x00070040/0; real incoming APBP bits.

The source's last two message bytes are normalized in a local copy, not the guest
buffer. The actual normalized payload is zero; nonzero synthetic tests independently
verify copying. Slot5 write-pointer becomes4, and mailbox2 receives slot5. All SRAM
and provenance effects match independent parsing; only the expected pointer/data
bytes are updated. Source bytes and neighbouring storage stay unchanged.

The main thread waits on its original audio event at round259. The first scheduled
DSP slice actually starts, then faults before completion:

```
stop=UnsupportedDspEvent pc=0x002594c4 thread=1 dispatch_rounds=259
cpu_ticks=4962257 core0_instructions=2287384 core1_instructions=0
guest_now_ns=18508160 display_periods=1
DSP completed_steps=10215 attempted_steps=10216 written_words=9216
DSP pc_before=0xB35 pc_after=0xB36
DSP error=Teakra unmodeled MMIO write at672 (0x2A0)
scheduled_slices=0
```

Slot5 read-pointer remains0/write4. The message is QUEUED, NOT acknowledged/consumed.
The entire scheduled16384-call slice did not complete. The partial DSP effects are
retained and the device is sealed. No DSP-to-ARM audio notification is manufactured.
Audio and exported semaphore events stay unsignaled. The original game has NOT yet
signaled the exported event handle; its notifier is tested with synthetic executing
DSP code. Direct SetSemaphore and scheduled execution are actual original-game calls.
There are four threads and31 handles. Original core1 still executes ZERO instructions
before its eligible window. All6MiB VRAM remains zero; do not post blank screenshots.

## Implementation contracts and limits

EventObject can hold an immutable service target. Kernel signal ordering follows
pinned WaitObject:signal, ordinary waiter processing, notifier, Pulse clear. A OneShot
waiter may consume first; repeated signals still notify. Clear/export never notify.
A returned notifier error is a host stop, not a guest Result. Ordinary event/waiter
effects may already exist; no rollback is claimed. The SVC leaves its CPU registers
unchanged on that failure. Service-held signaling propagates errors through GSP
command/display paths and DSP interrupt delivery too.

DSP semaphore target holds a weak reference to the real live interpreter. Expired,
inactive or faulted targets fail without a dangling capture/ownership cycle. The
retained event is lazily created on first successful export; handle/allocation
failure publishes no partial event or consumed free-list/generation state. Repeated
exports/duplicates/reconnects preserve identity. Full IPC readability/writability
preflight still occurs before allocation or mutation.

SetSemaphoreMask decodes the low16bits into the preset. It does not touch the
peripheral or event. Direct SetSemaphore and notifier OR bits into the SAME incoming
APBP semaphore/ICU path. They do not execute firmware, charge time, wake the audio
thread or imply completed audio. Synthetic firmware reads and clears the actual
combined bits, proving this is not disconnected host-only bookkeeping.

WriteProcessPipe supports ONLY the observed header0x000D0082,pipe2,size4,static
descriptor0x10402. It copies readable input, clears local bytes2/3, and invokes the
existing bounded device WritePipe method. The method validates real descriptors,
updates only the CPU-owned pointer and sends the actual slot through mailbox2.
WouldBlock/invalid metadata retain IPC/storage. Larger payloads/other pipes/read IPC,
blocking continuation and broader lifecycle remain explicit unsupported paths.

Live load, same-byte DSP DATA mapping, real boot replies, known-byte tracking,
ICU implementation and diagnostic scheduler are preserved from pending predecessor
work. No new opcode, AOT, DSP vendor instruction algorithm, GPU drawing, input
sampling or filesystem behavior was added. GSP edits only propagate notifier errors.

## Explicit policies, not hardware parity

--dsp-executor live-teakra requires diagnostic-dual CPU. guarded-teakra remains
probe-only; disabled remains staging-only. reference-zero-data supplies only data-gap
reset provenance; known-only still stops on unknown SRAM. empty-config models the
explicit missing-system-block fallback for532 special bytes, not calibrated NAND.
The verified ExHeader independently sets Multi/max30, but does not verify instruction
latency. The ARM model is still one recorded instruction/core/nominal tick. DSP boot
is synchronous/uncharged and scheduled slices retain the pinned timing approximation.
No controller samples, measured hardware cycles, audio output or rendered frame.

## Completed tests and evidence

Full GCC/Clang native builds link all599 unchanged AOT pages. All58 CTest suites
pass with each compiler; all58 ROM-free Clang ASan/UBSan suites pass at-O1 with leak
checks/halt-on-error. Exact configured names match without skips/duplicates. Earlier
56 suites remain; one obsolete unsupported-event fixture now tests unsupported unload.
New ctr_event_notifier_test and ctr_dsp_semaphore_test cover ordering, consumed/repeated
signals, lifetime, actual DSP read/clear opcodes, direct/event OR, presets, protected
responses, failed allocation, handle exhaustion, input normalization and WouldBlock.
Synthetic coverage is not proof of original audio output or original event signaling.

Five ordinary paired startup cases match stdout/stderr/exit/test-file bytes:live,
probe,known,bounded100,disabled.226 paired capture/data/log files match; run.json
host timing receipts are excluded from byte comparison. Logging-only capture runs
independently reach the same stop as normal CLI. Independent Python verifies exact
replies/handle identity/pipe bytes/provenance and unchanged GPU/uploads/GSP/HID/LCD/VRAM.
Code, entire raw RomFS, ExHeader and all603 AOT backup members retain their identities.
No full IVFC recheck, independent CPU oracle, Windows/macOS build or rendered visuals.

Current evidence:semaphore-checkpoint/. verify.py/proof.json, verify_identity.py/
identity.json; paired-{live,probe,known,bounded,disabled}/; paired-capture/{gcc,clang};
make_trace.py/inherited_trace_support.inc/trace_support.inc/trace_dual_runner.cpp;
capture_pair.py/run_pairs.py/run_original.py; final test logs and junit-*.xml.
Raw captures/private ExHeader stay private. Traces add logging only. Builds and
native/trace binaries are excluded from checkpoints and are rebuildable.

setup-notes.txt records finite-call build interruptions, unavailable streaming,
initial new-test include/known-byte expectation corrections, incomplete Debug-ASan
and combined capture attempts, and an initial wrong consumption assertion. Final
proof requires read0/write4. No incomplete/failed attempt is counted as passing.

## Next exact work

Inspect the observed DSP MMIO WRITE0x2A0 (in the BTDMP region) and its actual firmware
value/access context before adding peripheral behavior. The pinned Teakra mmio.cpp
currently wires0x2A2,0x2BE,0x2C2,0x2C6,0x2CA but not0x2A0. Do not replace the stop with
generic register storage, guessed bits, or a fake completed slice/audio interrupt.
Extend the peripheral only with justified semantics and rerun unchanged original
code. Confirm actual message consumption/notification and any core1 execution from
new results, not from the fact that a worker/device exists.

References:Azahar86a9f9236ae42bb5a2b995dbc933d599d8ea07ac, kernel/event.cpp and
kernel/wait_object.cpp/.h under src/core/hle; DSP service dsp_dsp.cpp for event,
preset/direct semaphore and local audio-message normalization. Local Teakra pin
3d697a18df504f4677b65129d9ab14c7c597e3eb, apbp.cpp/mmio.cpp/teakra.cpp. Exact identities
and read scopes:semaphore-checkpoint/references.json. No new hardware/PDF analysis.

## Scratch and reproduction

Root:/mnt/data/lego_recovery/. Current source:repo/. Builds:build-gcc/,build-clang/,
build-asan/. AOT:generated2/. Code:restored/code.bin. RomFS:game/prepared-romfs/romfs.bin.
ExHeader:prepared-launch/exheader.bin. Parts/manifest:romfs-library-roundtrip/.
New semaphore-game-*,semaphore-paired-*,semaphore-capture-* directories are owned
TEST state, not recovered NAND. Never overwrite unknown saves. Pair drivers remove
only their own newly created, size/hash-checked20-byte file between compiler runs.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/dsp-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty --romfs game/prepared-romfs/romfs.bin --gpu-vram-mode reference-zero --display-clock-mode reference-idle --cfg-profile reference-stereo --cpu-mode diagnostic-dual --block-limit 100000000 --exheader prepared-launch/exheader.bin --dsp-special-profile empty-config --dsp-executor live-teakra --dsp-reset-profile reference-zero-data
```

Expected exit3, UnsupportedDspEvent, ARM round259 and MMIO0x2A0. Use clang++ for
Clang; omit AOT for ROM-free sanitizers. Keep explicit options. Capture/normal drivers
refuse existing output folders. Select new paths rather than deleting old evidence.

## Recovery and delivery mandate

GitHub:GTTeancum/Lego-Undercover-3DS-Recomp main. Library:/LEGO-Chase-Recovery/.
The source/evidence archive is complete current indexed source, new private evidence,
prepared launch, manifest/index/verifier and patch. Verify CHECKPOINT-MANIFEST.json
before extraction, and SOURCE-INDEX.json exact Git blobs/modes including tracked
ignored reports. Reconstruct the source tree, never invent remote ancestry.
The prior live-pending archive and handoff were saved separately this turn;50b2942
is already in Library. Do not recursively duplicate or extract history over repo/.

Large inputs have existing separate backups:code.bin;LEGO-Chase-current-AOT-599pages-
2026-10-03.tgz;Prepared-RomFS/ two raw parts+romfs-parts.json;Prepared-Launch/exheader.bin.
Parts402653184+366526464 bytes restore via tools/restore_romfs_parts.py without
overwriting an existing file. Raw RomFS769179648, native view offset4096/size769175552.
Code SHA5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
AOT SHA2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9.
RomFS SHA6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf.
ExHeader SHAd7641f0a3bb89697ca8f0751e88d31703b54aa185ac78f9e74e41169dfcdc004.
No CCI re-extraction or new game upload is required.

Raw firmware/SRAM/ExHeader stay OUT of public GitHub. No code.bin, private AOT, raw
RomFS/parts, CCI, .git, compiled binaries or font files are embedded in the new archive.
Scratch may reset; Library/GitHub/attachments are recovery routes, not permanent
scratch. Record only actually completed publication/hosted tests/round-trip receipts.
Post updated downloadable Markdown and recoverable source/evidence EVERY work turn.
