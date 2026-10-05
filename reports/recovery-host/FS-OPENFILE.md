# Contained OpenFile sessions and GetSize

Checkpoint: October 5, 2026. Implementation commit
`ce38bf7c50731559bb18b92cf72f2f227a4780f5`; complete tested source tree
`195e0be704b8217d55539e6af972e9bacae86a7c`.

## Actual original-code execution

Restored source 26aeb48, verified all 321 checkpoint-manifest files and source tree
66be217d456969583ce484109c7c29c982a4abc7. Private code and AOT hashes matched.
The prior FS OpenFile stop at round 73 was reproduced before editing.

With a NEW empty shared-extdata test root and explicit `--ptm-step-mode empty`,
the unchanged original game now obtains a real file session and its size:

```text
fs:USER OpenFile: 080201c2 00000000 00000001 00000000 00000004 0000001c
                 00000003 00000000 00070002 0036a046
reply:           08020042 00000000 00000010 00060018
fs:File GetSize: 08040000
reply:           080400c0 00000000 00000014 00000000
```

The path is UTF-16LE `/gamecoin.dat`, 28 bytes including NUL. Archive handle 1,
open mode 3, attributes 0. File handle 0x00060018 is a kernel session owning an
actual host file descriptor, not a dummy handle or archive-table ID.

The next untouched original request is:

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=75
last_ipc_session=fs:File request_header=0x08030102
request=08030102 00000000 00000000 00000014 00010001 0000014a 0ffff600
```

This is a 20-byte Write at offset zero, raw flags 0x00010001, mapped input
descriptor 0x14A and address 0x0FFFF600. The logging-only diagnostic captured the
pending input in the private evidence archive. It did not write or seed it.
The on-disk file remains 20 zero bytes from the earlier guest CreateFile.
No file Write/Read, initialized save, title screen, renderer or gameplay success
is claimed. Dispatch rounds are not frames or a completion percentage.

The existing-file startup branch still stops separately at OpenFileDirectly
0x08030204, round 71. Mode-off fresh startup still stops at PTM GetTotalStepCount.
No external file initialization is used to skip either branch.

## Implementation boundary

OpenFile requires an initialized FS connection, explicit root, exact request
shape, UTF-16 type and static descriptor, full readable input and writable IPC
response, nonwrapping addresses, valid UTF-16 and bounded path components.
Only the known read/write/create mode bits are admitted. Empty/create modes
return the reference extdata UnsupportedOpenFlags; supported read, write or
read/write opens all have effective read/write access, matching the pinned backend.
Transaction ID and attributes remain ignored, not modeled filesystem features.

The pinned root and every parent open are descriptor-relative and no-follow.
The leaf is opened without create/truncate flags, checked as regular, and its
identity is compared across check/open. O_NONBLOCK prevents FIFO substitution
from hanging this path; it is cleared on the validated regular descriptor.
Symlinks, multiple hard links and nonregular files are rejected. These containment
choices are host policy, stricter than firmware, not a hostile-host sandbox.

Each OpenFile owns a separate endpoint. DuplicateHandle shares that endpoint;
closing the FS connection does not close file sessions. The last file-session
reference releases its descriptor. GetSize returns the captured 64-bit session
size, following File::Connect/GetSize. No pathname reopen or content access is
performed by GetSize. The actual file IPC Close command remains unsupported;
kernel CloseHandle lifetime is not claimed to implement that distinct operation.

Kernel handle exhaustion returns the existing kernel Result with a null moved
handle and releases temporary descriptors. Unknown host errors stop without
committing a guest response. Missing files never get created by OpenFile.

Timing remains incomplete: pinned FS_USER::OpenFile sleeps for the archive's
measured delay (3085068 ns for extdata). This host implements the synchronous
operation/reply only, not that sleep, worker scheduling or timing parity. Kernel
guest time stays zero; no clock advancement, event signaling or fake wake is added.
Non-POSIX contained opens remain unsupported; no Windows/macOS build is claimed.

## Validation

Full unchanged 599-page GCC and Clang builds succeeded. Final GCC 16/16 CTest,
Clang 16/16 CTest and Clang ASan/UBSan 16/16 ROM-free suites passed. Leak checking
and halt-on-error remained enabled; no sanitizer suppressions were added.

The new suite checks real descriptors/access flags, duplicate/final-close lifetime,
independent opens, size snapshots including a sparse file above 4 GiB, exact replies,
UTF-16/descriptor/pointer guards, file preservation, missing objects, symlinks,
hard links, FIFOs, root replacement, handle exhaustion and host permission failure.
The initial test compile had one mixed-auto declaration, corrected before testing.
An initial RLIMIT_NOFILE=0 error test also broke sanitizer introspection. A separate
minimal system_error reproducer showed the same vptr diagnostic without project
code. The final test induces a real EACCES instead; sanitizer checking was not disabled.
One combined build command timed out during the sanitizer build; it was resumed
and the final complete suites passed. Earlier failed/interrupted logs are retained.

Eight real startup scenarios produce byte-identical GCC/Clang logs: fresh, existing,
mode-off, alternate RTC, no root, missing archive, invalid root and invalid mode.
All 603 regular AOT backup files remain identical; 599 are page C++ files. The
111043 registry blocks and 545111 raw words are STATIC validation inventory.

Source/validation evidence: openfile-checkpoint/ in the private checkpoint.
Public startup logs: OPEN-FRESH-GCC.txt and OPEN-EXISTING-GCC.txt.

## Primary references and next task

Pin: azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
Inspected fs/fs_user.cpp OpenFile; fs/archive.cpp OpenFileFromArchive;
fs/file.cpp Connect, GetSize and Write; fs/archive.h. Earlier pinned extdata
backend, path and error definitions are retained from the CreateFile checkpoint.
All paths are below src/core/hle/service/, except the extdata backend under
src/core/file_sys/archive_extsavedata.cpp.

Next implement the actual bounded fixed-size Write, inspecting disk_archive.cpp
and flush/failure behavior first. Preserve input/output descriptor permissions,
file bounds and true write counts. Do not copy the pending buffer into the file
outside the implemented guest operation. The pinned Write decoder uses flags &
0xFF for flush and flags & 0xFF00 for timestamp updates: the observed 0x10001 sets
flush but NOT that timestamp mask. Its high bit must not be casually mislabeled.
No subsequent Close, Flush, Read or asset request should be assumed before running.

## Hosted CI confirmation

GitHub Actions run `37297822986` on implementation `ce38bf7` completed successfully
for both GCC and Clang jobs. Hosted tests are ROM-free, not game-data execution.
