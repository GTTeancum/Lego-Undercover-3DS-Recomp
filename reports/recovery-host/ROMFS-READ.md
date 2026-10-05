# Verified SelfNCCH RomFS access and original metadata reads

Date: October 5, 2026. Implementation commit:
`2b36a39691da2f28fc64a14eac4c3c10ba17ddaa`.
Tested/uploaded source tree: `2156886803df7d81759bf7b01acdcd17f0e7aa72`.

## Real input and actual execution

The six private original archive parts were restored from Library. The extracted
1,073,741,824-byte CCI matches the recorded SHA-256
`3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525`.
The unchanged preparation tool reproduced the exact original code.bin and extracted
the raw RomFS at CCI offset 1851392, with size 769179648 and SHA-256
`6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf`.
No keys, downloaded substitute game data, game patches or empty RomFS were used.

All 187787 blocks in the three IVFC hash levels verified (12 + 1456 + 186319),
in addition to the complete raw image hash. The native view starts at raw + 4096
and spans 769175552 bytes, following pinned NCCHContainer::ReadRomFS. This includes
trailing integrity tables; it is NOT truncated to the logical level-3 data length.

A NEW explicitly provisioned shared archive, explicit empty PTM profile, and the
verified RomFS let unchanged game code execute SelfNCCH OpenFileDirectly and Read.
The observed open uses Empty archive path type 1/length 1, descriptor 0x4802 (ID 2),
and a 12-byte all-zero binary file path with descriptor 0x30002 (ID 0). The Empty
path's incidental 0xE8 byte is not a filename or required NUL terminator.

The first observed read requested the 40-byte filesystem header at offset zero.
That read exposed a real Close request; only then was the reference IVFC Close
policy added. A logging-only trace recorded seven reads totaling 4892 bytes:
three header reads at offset 0/length 40, then 40/12, 52/68, 120/212 and 332/4480.
Every returned byte matches the original RomFS at view_offset + request_offset.
These are filesystem header, directory and file tables, NOT rendered assets.

The final fresh run stops without a response at:

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=90
last_ipc_session=fs:USER requested_service= request_header=0x08630000
```

The next request is FS_USER GetPriority. It remains unsupported. A second process
with the existing guest-created gamecoin file reaches the same request at round 82,
but skips fresh file initialization; this is separate evidence, not save/load proof.
Without --romfs, the old OpenFileDirectly stop at round 79 remains. Without explicit
empty PTM, the earlier GetTotalStepCount stop at round 71 remains.

The original guest still creates, writes and closes its own gamecoin.dat. Its
unchanged default-clock SHA-256 is
`970a8b30f57b772c2c1c5686e634b9e4ab7055b43caec90e64076f81ed08b4b6`.
No gamecoin readback, gameplay save/load, completed initializer count, menu,
renderer, audio, controls or gameplay is established. Dispatch rounds are not frames.

## Implementation and explicit limitations

--romfs is optional and requires the exact pinned size and full SHA before execution.
A streaming SHA-256 implementation avoids loading the full image into host memory.
A separate read-only endpoint pins an O_RDONLY descriptor and never uses the writable
extdata backend. Read/GetSize and observed Close are supported; Write, SetSize,
Flush and other file operations remain explicit stops. GetSize is component-tested
but was NOT observed in this real startup sequence.

Open validates complete input descriptors/spans and IPC response writability before
allocating a genuine moved file-session handle. The transient SelfNCCH open does not
add a shared-extdata mount. The file endpoint retains the verified image, and
kernel duplicate/close ownership remains separate from the shared image lifetime.

Reads preflight the entire declared writable output, including any tail clipped by
EOF, and reject response aliases. A zero-length read dereferences no destination.
The 1 MiB request bound is host policy, not a firmware limit. General mapped-buffer
translation and cross-region spans are not implemented. Staging prevents an
incomplete host read from copying substitute bytes or committing a successful
reply. Normal guest writes invalidate exclusive reservations.

Crossing EOF clips without unsigned overflow. At or beyond EOF returns zero; this
explicit safety policy avoids a reference underflow and is not verified hardware
parity. Host extent/mtime changes cause a stop before staged data is committed.
The pinned descriptor prevents pathname replacement from redirecting reads, but
this is not protection from a malicious host administrator modifying file contents.

Pinned IVFCFile::Close returns false without releasing its shared reader, while
FS::File::Close ignores that return value and replies success. This narrow HLE
behavior is retained and documented, not conflated with writable extdata Close.
The image descriptor closes on final ownership release. No dummy file handle is used.

FS delays, caches, worker scheduling, hardware timing, general archive formats,
ExeFS/update-RomFS support and other service commands are not reconstructed.
Kernel guest time remains zero. No artificial wake, time advance or Break bypass
was introduced. POSIX implementation only; no Windows or macOS build is claimed.

## Validation and recovery

GCC and Clang full native builds include the unchanged 599 private AOT pages.
Both pass all 19 CTest suites; Clang ASan/UBSan passes all 19 ROM-free suites with
leak checking and halt-on-error. Eleven startup scenarios have byte-identical
compiler logs: fresh, existing, PTM mode-off, alternate RTC, no RomFS, no root,
missing archive, invalid root, invalid mode, missing RomFS and wrong RomFS size.
All 603 regular AOT backup members and code.bin remain unchanged.

New tests cover exact requests/replies, descriptor and permission guards, input and
output arithmetic, protected/partial responses, aliases, reservations, shared
ownership, unsupported writes, EOF, image identity/change rejection and handle
exhaustion. SHA tests cover golden vectors, padding and chunk boundaries. The
full original RomFS independently matches both Python and native streaming hashes.

The first OpenFileDirectly implementation used incorrect static descriptor IDs;
the original request trace corrected them. An initial test enum name was also
corrected. All final builds/tests passed. The archive extractor rejected ancillary
Vimm's Lair.txt after fully extracting the CCI; it exited 4 rather than claiming
clean archive completion. The exact CCI hash and subsequent preparation independently
verified the game input. Intermediate logs are retained, not labeled passing.

The EXTRACTED raw RomFS is durably saved in private Library Prepared-RomFS as two
uncompressed consecutive parts plus manifest/helpers. Both parts were downloaded
again, reassembled and compared byte-for-byte to the original extracted image.
A scratch reset no longer requires original 7z/CCI re-extraction for this input.
No game bytes or private test state are added to public GitHub.

## Primary references and next task

azahar-emu/azahar @ `86a9f9236ae42bb5a2b995dbc933d599d8ea07ac`:
`src/core/file_sys/ncch_container.cpp`, `archive_selfncch.cpp`, `ivfc_archive.cpp`,
`ivfc_archive.h`, `romfs_reader.cpp`; prior `src/core/hle/service/fs/fs_user.cpp`,
`file.cpp` and IPC definitions. These are HLE reference policies, not hardware proof.

Next inspect the pinned FS_USER GetPriority and original caller, then implement
its observed request using the actual shared priority state. Inspect its special
uninitialized-priority behavior rather than guessing a constant. Preserve the
current real RomFS reads and separate fresh/existing branches.

## Hosted CI confirmation

GitHub Actions `37312503207` on implementation `2b36a39` completed successfully
for GCC and Clang. Hosted tests are ROM-free, not original-game execution.
