# Verified title launch policy and reconciled dual-core startup

Implementation: `274806aef6fdbba3dec5abbc17ee7c59a894f672`.
Exact implementation tree: `dbd7c64d873de0da2a2c9678e54bb244c8297b5d`.
Parent: `4e6c9cdd3ea33554eaa0591c1bb71c11780c91cb`.

## What changed and what was preserved

Work started from the private CORE1-BUDGET-PENDING checkpoint based on 095af71.
An independent scheduler variant was developed and tested. Before publication,
remote main advanced to 4e6c9cd with overlapping diagnostic-dual implementation.
That newer source was downloaded, checksum/tree verified and retained as canonical.
Only the title-launch verification and resource-ceiling addition was applied to it.
The alternative source, its tests/captures and the earlier pending checkpoint remain
private recovery material. They were not silently overwritten or claimed as merged.

The original CCI was streamed from the existing private archive parts. Its full
1073741824-byte SHA matched the established image, and its NCCH ExHeader hash was
checked. The recovered 2048-byte ExHeader has SHA-256
`d7641f0a3bb89697ca8f0751e88d31703b54aa185ac78f9e74e41169dfcdc004`.
Its CPU descriptor is 0x009E: Multi scheduling and maximum CPU value 30. Program ID
is 00040000000AD500, main ideal processor 0, priority 48, application category 0.
This resolves the title's launch ceiling; it does not measure instruction latency.

New optional `--exheader FILE` requires `--cpu-mode diagnostic-dual`. The launcher
checks the complete header's size and SHA before using its fields. It initializes
the SAME kernel application resource object before launch, with maximum 30 and
current 0. The original APT request subsequently sets current 30. Live replacement,
wrong identities, missing/short files and incompatible modes are rejected. A
plausible synthetic header is not accepted as the original. Without this option,
existing behavior retains the inherited maximum 80 and the default strict mode.

Exactly six implementation paths changed: CMakeLists.txt, host/launch_header.h,
host/main.cpp, runtime/ctr_kernel.h, runtime/ctr_dual_core.cpp and the new
ctr_launch_header_test.cpp. Core selection, quotas, instruction stepping, memory,
IPC, GPU and HID algorithms remain those of parent 4e6c9cd. The optional kernel
configuration updates the resource object; it is not a separate diagnostic number.

## Actual original-game result

```text
cpu_ticks=4839693 core0_instructions=2180849 core1_instructions=0 quota_transitions=0
display_periods=1 guest_now_ns=18051022
app_cpu_time_current=30 maximum=30 core0_only=0 core1_enforcement=diagnostic_windows
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=248
last_ipc_session=hid:USER request_header=0x000a0000
```

The original code maps/protects its stack alias, creates its processor-1 worker,
and sets that worker's priority to 49. There are four threads and 22 handles.
The worker remains Ready at 0x00104DF4, TLS 0x1FF82600, with ZERO executed
instructions. Its first application window is at tick 5180070; the main thread
reaches HID first. Do not present synthetic dual-core execution as proof this
particular worker ran. GetIPCHandles remains unsupported and its request untouched.
No input page, controller data, event handles or successful HID reply is fabricated.

The complete 6 MiB VRAM bank remains zero. There is no useful screenshot, logo,
rendered frame, main menu, executed shader, audio or gameplay. Display periods and
initialization transfers are not frames. The inherited one-recorded-A32-instruction
per-core-per-tick model is explicit diagnostic timing, not cycle-accurate hardware.

## Final canonical verification

Full GCC and Clang native builds link all 599 unchanged private AOT pages. All 45
CTest suites pass with each compiler; all 45 ROM-free Clang ASan/UBSan suites pass
with leak checking and halt-on-error. All 44 parent suites remain, plus the new
launch-header test. Ten paired command-line/startup scenarios match stdout, exit
codes and newly created test-file bytes. These include authenticated/inherited
launch, strict mode, bad identity/size/path, incompatible or malformed options,
and the inherited default instruction-budget stop.

Six paired logging-only capture files match byte-for-byte: CPU/thread/quota state,
all 1842 GPU words, serialized uploads, the GSP page, original IPC and complete
VRAM. Raw GPU/page/request/VRAM captures also match the alternative implementation
at this common stop. That is a consistency check, not an independent ARM oracle.
Original code.bin, raw RomFS and all 603 AOT archive members retain their verified
identities. Full IVFC-block verification was not repeated. No Windows/macOS build.

The alternative also passed 45 suites, but its suite composition is different.
Its 15-case matrix is NOT combined with the canonical 10-case matrix or substituted
for canonical validation. A semantically identical PTM zero-print expression was
normalized while matching the uploaded source; both native targets and all 45
GCC/Clang suites were rebuilt/retested afterwards. Setup/reconciliation records and
all relevant logs are retained privately in launch-checkpoint/ and the alternative.

## Next work and reference

Implement the observed HID GetIPCHandles from its real shared-memory/event contract,
then rerun unchanged code and observe when the actual processor-1 worker first runs.
Keep input sampling, event signaling and busy-CPU timing limitations explicit.
Do not return dummy handles or fabricate controller state merely to pass the call.

Reference pin: azahar-emu/azahar at 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
ExHeader layout: src/core/file_sys/ncch_container.h. Launch resource initialization:
src/core/hle/kernel/resource_limit.cpp. Priority handling: kernel/svc.cpp and errors.h.
HID command identity: src/core/hle/service/hid/hid_user.cpp (0xA GetIPCHandles).
Raw title bytes and private captures are excluded from public GitHub.

## Hosted confirmation

GitHub Actions run 37478931314 on implementation 274806a passed both GCC and Clang
jobs. These are ROM-free hosted suites, separate from full local original-game runs
and the locally completed Clang ASan/UBSan suites.
