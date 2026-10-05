# GSP CacheFlush queue checkpoint

October 5, 2026. Implementation `811083b6d68d6cd10eaf7f22a07574973d0442e0`, exact tested tree `ef981f540f716197161e2c1f46d0d41446c195ba`.
Base: `05b01f75d05b1ef6762ca44f1ff183860326762e`.
This remains a headless native startup reconstruction, not a playable port.

## Demonstrated original-game sequence

The existing GPU-rights owner's slot-zero queue receives CacheFlush packet:

```text
00000105 14003790 00007480 00000000 00000000 00000000 00000000 00000000
```

The actual TriggerCmdReqQueue request is `000c0000`. The new handler consumes one
CacheFlush packet, changes the shared queue header from `00000100` to `00000001`,
and replies `000c0040 00000000`. The region at 0x14003790 contains 29824 bytes;
its contents compare byte-for-byte equal before and after the call. Its SHA-256 is
824caecf736fbd9febe688ee15b2f1da3913f9c19cf9f5a89e4539e540877c73.
No GPU execution, interrupt, event signal, completion flag or guest time is added.

The original code then enqueues SubmitCmdList, ID 1, at index 1 (0x10000840):

```text
01000101 14003790 00007480 00000000 00000000 00000000 00000000 00000000
```

The command list is the same readable 29824-byte region. Flags and do_flush are
zero; stop byte is zero; unknown packet bytes 1 and 3 are both 1. The queue header
is now `00000101`, index 1 and one pending packet. This packet is NOT consumed or
executed. Both before/after page hashes are
`a8df46c356cf1acbaa3545d5ab705664ba4155dc6b3b5892da97bdae96d513b9`.

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=167
last_ipc_session=gsp::Gpu requested_service= request_header=0x000c0000
host_ipc_error=GSP queue packet requires unimplemented GPU execution
```

Fresh startup uses a NEW empty archive, explicit empty PTM and verified RomFS.
Existing-file startup reaches the same pending command at round 159, skipping
initialization. Existing-file startup is not gamecoin readback or gameplay save/load.
The game still creates, writes and closes its own coin file. Seven original RomFS
metadata reads still total 4892 bytes and match the verified image. These are
filesystem tables, not rendered assets. There is no main menu or gameplay proof.

## Queue implementation policy

The handler selects the GPU-rights owner's queue, not the caller's slot. It uses
the real module-shared backing, a 15-packet ring and the inspected index/count/
status/stop ordering. Each dequeue precedes the conservative native fence; a stop
byte sets STOPPED after the packet. Empty or already stopped queues do not consume
packets. should_stop sets STOPPED without dequeuing. Packet bytes are preserved.

Only CacheFlush is supported. There is no separate CPU/GPU cache or asynchronous
GPU in this host: earlier synchronous writes already affect authoritative backing.
The native sequentially consistent fence is an explicit host ordering policy,
not a GPU transfer, device-cache simulation, interrupt or timing model. The pinned
GPU::Execute CacheFlush case performs no extra action.

Host safety policies are explicit: validate all three nonzero readable spans with
wide address arithmetic; reject unknown/failed status and out-of-range ring fields;
preflight the whole eligible batch before progress. An unsupported later packet
leaves even a valid prefix pending. A stop marker ends the eligible batch. This
atomic diagnostic policy is not a claim of exact hardware failure precedence.

Response writability is checked by the existing router. A new shared-backing
identity predicate rejects an IPC response alias anywhere in the GSP page, including
another virtual mapping. Header writes invalidate the real shared reservation
granule; source and packet reservations remain independent. No handles, threads,
events or guest time are allocated/changed by CacheFlush. Full handle tables work.

No-owner requests remain explicit stops. General physical GPU addressing, active
triggers, PICA command decoding, rendering, cache modeling, asynchronous timing,
failed-queue recovery and non-CacheFlush packet execution are not implemented.
Only six implementation files changed: both CMake files, ctr_memory.h,
gsp_gpu_service.h, new gsp_command_queue.cpp and ctr_gsp_queue_test.cpp. Vendor,
production IPC, scheduler, filesystems and original game inputs remain unchanged.

## Verification and provenance

Full GCC and Clang native builds include all 599 unchanged private page units.
All 26 CTest suites pass with each compiler; Clang ASan/UBSan passes all 26 ROM-free
suites, with leak checking and halt-on-error. Eleven production startup scenarios
have byte-identical GCC/Clang logs. All 603 regular AOT members match their backup,
and original code/raw RomFS SHA-256 values match. Full IVFC-block verification was
not repeated in this checkpoint; the raw hash matches the previously verified image.
Registry 111043 blocks / 545111 words is STATIC inventory, not executed frames.

The logging-only alternate IPC trace is independently checked by make_proof.py:
replay the complete shared-page bytes before/after CacheFlush and the next packet,
compare source bytes and seven RomFS read hashes, verify the unsignaled event and
zero time. Production IPC is unchanged and normal builds reach the same boundary.
Final logs/scripts are in queue-checkpoint/, including final-finished.json,
final-evidence-finished.json, ctest-gcc/clang/asan.txt, validation-summary.json,
trace-game.txt and queue-proof.json. No failed native test was suppressed.

The attached older GSP-REGISTERS-PENDING archive was preserved and all 782 manifest
entries verified. A fresh remote read revealed newer main 05b01f7, which already
contains masked writes and legacy DSB routing. Its source snapshot was downloaded
from Actions run 37335606464, artifact 11355887677, and its complete tree verified.
Those inherited changes are NOT newly implemented in this queue checkpoint. The
pending archive stays separately in Library Preserved-Pending-Registers; never
overlay it on the newer source. A one-space publication transcription difference
was corrected before publishing the exact tested tree. The optional expected_sha
argument was rejected by connector binding; the normal force=false update succeeded.

## Primary reference and next work

Reference: azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
Inspected src/core/hle/service/gsp/gsp_command.h (packet/ring layout),
src/core/hle/service/gsp/gsp_gpu.cpp TriggerCmdReqQueue, and
src/video_core/gpu.cpp GPU::Execute CacheFlush. Queue status comments differ from
the exact STOPPED equality test; unknown statuses remain unsupported here.

Next: inspect the actual SubmitCmdList path, physical-address translation,
PicaCore::ProcessCmdList and special-register execution. Decode/validate the guest's
real list without treating it as passive MMIO or consuming it before its effects
are supported. Do not fabricate GPU completion or unblock the game with an interrupt.

## Hosted CI confirmation

GitHub Actions run `37343431474` on implementation `811083b` completed successfully
for both GCC and Clang jobs. Hosted tests are ROM-free, not original-game execution.
