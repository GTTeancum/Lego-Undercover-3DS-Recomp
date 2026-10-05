# Guest-driven fixed-size Write and Close — October 5, 2026

Implementation `4783f2280b68be0593d8185215589a4d683cbd31`, tested tree
`207e2a111d65a5180141051f3019b1319adb3c41`, based on e81c90c.

The original game now writes its own 20-byte /gamecoin.dat and closes the actual
file backend. The host does not preload a file header, coin balance or save state.
The logging-only diagnostic's input bytes match the resulting disk file exactly.
Default-clock file SHA-256:
970a8b30f57b772c2c1c5686e634b9e4ab7055b43caec90e64076f81ed08b4b6.

Observed sequence:

```text
Write request: 08030102 00000000 00000000 00000014 00010001 0000014a 0ffff600
Write reply:   08030082 00000000 00000014 0000014a 0ffff600
Close request: 08080000
Close reply:   08080040 00000000
Close backend is open: false; kernel client handle still exists: true
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=79
last_ipc_session=fs:USER requested_service= request_header=0x08030204
```

The final request is OpenFileDirectly, SelfNCCH archive 3, Empty archive path,
Binary 12-byte file path consisting of three zero words, mode 1 and attributes 0.
Pinned archive_selfncch.cpp maps file-path type zero to the application's RomFS.
The incidental one-byte Empty path buffer contains E8, not an actual path string.
No RomFS handle or successful read is fabricated. The next task needs the original
CCI/RomFS restored from the user's existing private Library game-archive backups.

A second process using the guest-created file reaches that same command at round
71, skipping initialization; the file remains unchanged. With PTM mode omitted,
fresh startup still stops at GetTotalStepCount and the created file remains zero.
A different explicit RTC epoch changes the guest's stored date, without a host
save-content patch. Kernel time remains zero in every current startup scenario.

## Scope and error policy

Write validates exact IPC shape and read-only mapped descriptor, caps work at 1 MiB,
preflights the complete declared source and response, then copies input before I/O.
Writes use the owned descriptor. EOF bounds clip safely; beyond EOF returns the
pinned error. The source is not modified, and unrelated exclusive reservations
survive source reads. Aliases between source and IPC reply remain unsupported.

The POSIX backend uses pwrite, retries EINTR and completes short positive writes.
Unknown host errors request an explicit host stop and preserve the original guest
request/registers, while recording the actual partial count. No rollback is claimed.
The pre-write file-size/link check is host policy, not protection against hostile
host administrators or full transactional isolation.

Flags use the low byte for flush. Bit 16 in observed 0x10001 is not assumed to be a
timestamp bit; the reference backend ignores timestamp updates. This host invokes
fsync for flush requests, deliberately stronger than pinned fflush, without claiming
3DS durability or timing parity. Sync errors after writes retain those writes and
stop, rather than return a fabricated successful result.

Close retires descriptor ownership and closes it without deleting the kernel
client handle. Duplicate handles share the endpoint; independent opens remain
independent. Repeated Close success and captured GetSize after closure follow
reference HLE policy, not hardware validation. Closed Write requests remain stops.

Standalone Flush, Read, OpenFileDirectly, SetSize, CloseArchive, subfiles, full IPC
mapping, asynchronous FS, save/load and gameplay remain unimplemented. No Windows
or macOS build, main menu, renderer, audio, controls or gameplay is claimed.

## Validation

GCC full baseline/changed-source rebuild and Clang full builds include unchanged
599 AOT pages. All 17 CTest suites pass in GCC, Clang and Clang ASan/UBSan; sanitizer
runs are ROM-free with leak checking and halt-on-error enabled.

Tests include zero/EOF/64-bit boundaries and sparse files above 4 GiB, exact reply
counts and register preservation, pointers/descriptors/aliases, protected responses,
source immutability, close ownership, pathname replacement, external extent changes
and hard links. RLIMIT_FSIZE fixtures force failures after 0 and 8 bytes; a Linux
TEST-ONLY fsync wrapper checks flag selection and failure after a complete write.
Production executables and original-game traces use real host syscalls, not mocks.

Eight real-game scenarios produce byte-identical GCC/Clang logs: fresh, existing,
PTM mode-off, alternate RTC, no root, missing archive, invalid root, invalid mode.
All 603 regular private AOT backup members and the original code.bin match their
saved bytes. Static registry counts 111043 blocks / 545111 raw words are not frames
or gameplay progress. Full evidence is in private writefile-checkpoint/; public
final logs are WRITE-FRESH-GCC.txt and WRITE-EXISTING-GCC.txt.

## Primary references

azahar-emu/azahar pinned at 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac:
src/core/hle/service/fs/file.cpp (wire format, flags and close),
src/core/file_sys/archive_extsavedata.cpp (fixed-size writes),
src/core/file_sys/disk_archive.cpp (write/flush), src/common/file_util.cpp (fflush),
src/core/file_sys/archive_selfncch.cpp (observed next path selects RomFS).
The code models these limited semantics and explicitly documents host-policy
changes; these references do not establish hardware or complete game parity.

## Hosted CI confirmation

GitHub Actions `37308386943` on implementation `4783f22` completed successfully
for both GCC and Clang jobs. These hosted tests are ROM-free, not game-data runs.
