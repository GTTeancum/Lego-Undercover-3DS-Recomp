# LEGO Chase Begins — canonical continuation handoff

Updated October 6, 2026. Work in assistant scratch, NOT the user's PC or Work.
Nintendo 3DS USA: LEGO City Undercover: The Chase Begins. Native/headless startup
reconstruction, NOT playable. Post updated downloadable Markdown and recoverable
source/evidence at the end of EVERY work turn. Only show genuine meaningful game
visuals; there is still no rendered logo, title screen, main menu, audio or gameplay.

## Canonical source and reconciliation

The current checkpoint combines newer main `10b48d25fd8443751916d14daf0b622c49d9ca78`
(tree `db674f651060aff0061fc070737d971a8aa7cd61`) with the complete pending HID work
(tree `b81593cb4e9f3e47e4527876325f114f5045b1e2`, parent `4e6c9cd`). New work adds
DSP1 validation/staging only. The final downloadable receipt identifies publication,
source tree, backup paths and any actually observed hosted CI. Do not infer publication
from an unfinished local Git commit; the local repository is a reconstructed index,
not full upstream history. Use fresh remote parents and force=false/expected_sha.

Both predecessor archives were verified before use: pending HID 743 manifest entries,
379 source blobs; main 10b48d2 561 entries, 368 source blobs. Three-way reconciliation
conflicted only in the CMake test list and old handoff. The union of all 48 predecessor
suites was preserved. The newer launcher/ExHeader validation was not overwritten.
Older pending-only HID reports were moved into private historical evidence; their
original source/patch/archive is preserved unchanged separately, not lost.
The isolated ctr_core1_budget component is still tested, but is NOT the live quota
implementation. No scheduler, opcode, private AOT or GPU drawing implementation was
added in this turn. This is not new multicore or HID input-sampling work.

## Actual current original-game stop

With the command below, the original game obtains HID's real shared page/five events,
maps the SAME page read-only at 0x10002000, and requests its DSP program. The optional
verified ExHeader sets the actual application maximum to 30 (instead of inherited80).

```text
cpu_ticks=4945873 core0_instructions=2287029 core1_instructions=0 quota_transitions=0
display_periods=1 guest_now_ns=18447051
app_cpu_time_current=30 maximum=30 core0_only=0 core1_enforcement=diagnostic_windows
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=252
last_ipc_session=dsp::DSP request_header=0x001100c2
host_ipc_error=DSP1 image verified/staged only: special-segment data and firmware boot handshake unresolved
ipc_words=001100c2 0000c234 000000ff 000e00ff 000c234a 0037a120 00000000 00000000
```

LoadComponent has NOT succeeded. The call, CPU registers, original input and guest
memory remain untouched. The only new state is a HOST inspection snapshot. Four
threads and 29 handles remain. Core1's original worker has executed ZERO instructions
at this stop because its eligible window has not begun. No DSP program instructions
have executed. The full VRAM bank is still zero; there are no quality screenshots.

## DSP image: verified ordinary segments, unresolved special data and boot

The input is 49716 bytes at guest0x0037A120, exactly code.bin[0x27A120:0x286354].
SHA: 7ea3c44a1c57514bebbebce8a7995f7f3a290170ea3b6c145749ad9358672c97.
There are five ordinary segments: 43692 program bytes and 5256 data bytes. All five
stored SHA-256 hashes match. Per-byte known masks accompany separate 0x40000-byte
host program/data banks. ProgramA/B share the program bank; target addresses are
16-bit words. Unknown allocation storage is NOT valid zero SRAM or calibration.
The parser does not authenticate the RSA signature; whole original code identity
and segment integrity are distinct checks.

Flags3 requires initial replies and a special segment: data word0xEF29, byte0x1DE52,
length0x214 (532 bytes). Those bytes are NOT in the ordinary segments. The range is
explicitly unknown. 3dbrew's DSP Binary documentation describes CFG system block
0x70000 as its source and zero fallback if the read fails. This host has NOT modeled
that system read/failure and does not substitute zeros. Pinned LLE leaves that step
TODO. After loading it waits for register0/1/2 to each yield1, then register2 to yield
a pipe-base address. These responses require a working DSP/backend, not constants.
The reference HLE loader only hashes the image because an existing HLE audio engine
supplies its surrounding semantics; that is not permission to bypass this backend.

StageDsp1Image validates header/length/count/types/source/target ranges, selected
banks, overlaps and hashes. It publishes a complete unique host image only after
validation. Failure, including allocation failure, preserves the old output. Gaps
and special bytes remain unknown. The service copies readable input, stages it,
then requests an explicit host stop WITHOUT writing a guest reply or advancing time.
A later failed inspection retains the last valid diagnostic snapshot; it is not
live loaded firmware. Other DSP commands remain unsupported. The bounded 1MiB cap,
odd-byte and overlap rejection, and full-mask-only IPC are HOST POLICY, not full
firmware error behavior. No DSP SRAM mapping, pipes, semaphore signals, audio samples,
component-ready flag, or initialized DSP state is fabricated.

## Scope retained

HID exports actual retained objects with transactional six-handle allocation and
read-only client permissions. It still has NO periodic input producer/controller
samples/sensor calibration. Empty HID storage is unpopulated state, not sampled
neutral input. Strict CPU mode remains default. Diagnostic-dual is one recorded
A32 instruction per logical core per nominal tick, core0 before core1, not measured
ARM11 instruction latency or hardware parallelism. Title ExHeader now independently
verifies Multi scheduling/max30, but does not validate timing. All explicit PTM,
VRAM, display, CFG and CPU options below must remain visible.

## Tests and evidence actually completed

GCC and Clang full native builds link all 599 unchanged private page units.
All 50 CTest suites pass under each compiler. All 50 ROM-free Clang ASan/UBSan suites
pass with leak checks/halt-on-error. Both new suites cover parser boundaries, hash
failures, all segment types, known/unknown bytes, allocation rollback, protected
responses, invalid IPC and no guest mutation. No inherited suite was removed.

Six completed paired original-startup scenarios match logs, exits and newly created
file bytes: authenticated, inherited ceiling, strict CPU, noCFG, noRomFS, noPTM.
Two larger foreground batches exceeded the 45-second call limit. Partial roots/logs
remain; no uncompleted case is counted as passing. Those setup limits are recorded
in dsp-checkpoint/setup-notes.txt. Final native suites had no failing tests.

All 80 final capture files match GCC to Clang. Full before/after IPC/input, GPU words,
uploads, GSP/HID pages/epochs, mapped HID, VRAM and captured CPU/thread/time state
are identical across DSP inspection. Independent Python replay reproduces all bytes
and known masks in both staged banks. Logging-only alternate runners capture this;
ordinary uninstrumented executables independently reach the same stop.
Code.bin, raw RomFS, original ExHeader and all603 AOT members match their backups.
Full IVFC verification was not repeated. Registry111043 blocks/545111 words is static
inventory, not new execution counts or frames. No Windows/macOS build was performed.

Current private evidence: dsp-checkpoint/. build_other.py/build-results.json,
ctest-first-gcc.txt/ctest-clang.log/ctest-asan.log, validate_matrix.py/matrix-results.json,
make_trace.py/trace_support.inc/trace_dual_runner.cpp, capture.py/capture-gcc/clang,
verify_capture.py/proof.json, identity.json, setup-notes.txt. All raw firmware/bank
captures are PRIVATE. Public report: reports/recovery-host/DSP1-STAGING.md and
DSP1-STAGING-PROOF.json. Prior hid-checkpoint/ and launch-checkpoint/ remain separately
restored in scratch and in predecessor backups; they are not newly run proof.

## Next exact work

Implement the required special-segment source/policy and a DSP backend that genuinely
performs the boot handshake, or a faithful native HLE replacement for that behavior.
Do not simply acknowledge LoadComponent, assume a pipe base, or execute unknown SRAM
as zero. Use the existing captured image rather than asking for another game upload.
A parsed/staged image is not a completed DSP load. After justified implementation,
run the unchanged original again in a NEW private archive and follow its actual next
request. Keep special-data provenance, input sampling and timing limitations explicit.

References: Azahar @86a9f9236ae42bb5a2b995dbc933d599d8ea07ac,
src/audio_core/lle/lle.cpp blob388fe64ec1a5130a2c93a5dfa04ca84df109b567 (DSP1 layout,
ordinary placement, special TODO, startup replies and pipe base);
src/audio_core/hle/hle.cpp blob05ff9d74e1077595faba67a61a7ce4581a80d374;
3dbrew DSP Binary https://www.3dbrew.org/wiki/DSP_Binary read October6,2026.
Mutable search results were navigation only; implementation reads used the pin.

## Scratch and reproduction

Root /mnt/data/lego_recovery/. Source repo/; builds build-gcc/, build-clang/, build-asan/.
Private AOT generated2/; code restored/code.bin; RomFS game/prepared-romfs/romfs.bin;
parts/manifest romfs-library-roundtrip/; ExHeader prepared-launch/exheader.bin.
Current evidence dsp-checkpoint/. Untouched pending source preserved-pending-hid-source/;
pending archive restore pending-hid-restore/; newer main restore restored-main-10b48d2/.
New dsp-original.*, dsp-capture.*, dsp-matrix.* directories are owned TEST state,
not recovered console NAND. Never remove unknown saves or overwrite captures.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/dsp-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty --romfs game/prepared-romfs/romfs.bin --gpu-vram-mode reference-zero --display-clock-mode reference-idle --cfg-profile reference-stereo --cpu-mode diagnostic-dual --block-limit 100000000 --exheader prepared-launch/exheader.bin
```

Expected exit3 and the pending DSP call shown above. Omitting ExHeader retains
maximum80; omitting CPU mode requires also omitting ExHeader and retains the strict
processor1 CreateThread stop. Use clang++ for Clang; omit LEGO_AOT_DIR for ROM-free
sanitizers. Default one-million diagnostic instruction budget stops earlier.

## Durable recovery and every-turn mandate

GitHub GTTeancum/Lego-Undercover-3DS-Recomp main; Library /LEGO-Chase-Recovery/.
Always restore the newest exact source/index, not an older pending implementation.
Check CHECKPOINT-MANIFEST.json with verify_checkpoint.py and restore SOURCE-INDEX.json
modes/blobs including ignored tracked reports. Never force-push reconstructed history.

Original backups: code.bin; LEGO-Chase-current-AOT-599pages-2026-10-03.tgz;
Prepared-RomFS/ two parts and manifest; Prepared-Launch/exheader.bin.
RomFS parts402653184+366526464 bytes; use repo/tools/restore_romfs_parts.py, which
refuses overwrite. Raw RomFS769179648 bytes, native view offset4096/size769175552.
No original CCI extraction is necessary. SHA identities:
code5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f;
AOT2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9;
RomFS6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf;
ExHeaderd7641f0a3bb89697ca8f0751e88d31703b54aa185ac78f9e74e41169dfcdc004.

The exact old HID-HANDLES-PENDING archive/MD were newly saved to Library during this
turn. The 10b48d2 source/evidence archive was already there. Preserve both, separately;
their original history need not be recursively duplicated in each new backup. The
new checkpoint includes complete current source, new raw evidence, prepared launch,
source index, manifests, patch and explicit predecessor hashes/locations. Large game
inputs, private AOT, raw RomFS, .git, native executables and build products are excluded.
Raw program/bank captures and ExHeader must never enter public GitHub.

Scratch may reset; attachments/Library/GitHub are recovery routes, not permanent
scratch. No unobserved publication/CI/round-trip is a success. Append actual delivery
receipts after publication and verify saved files when possible. Post the updated
Markdown and source/evidence checkpoint at the end of EVERY work turn.
