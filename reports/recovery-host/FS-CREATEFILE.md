# Shared-extdata CreateFile and observed PTM boundary

Checkpoint date: 2026-10-05. Implementation commit:
`654a8a2c5bcb81164e98589dd79ae6b6cf6d1102`.
The complete tested/uploaded implementation tree is
`ad137053f44d5377cb6c110ffd4b7d9f6bd934ed`.

## Fresh execution, not historical gameplay

Restored the current source from the verified GitHub Actions source snapshot at
`928c148310fa75e6a63461b7e24703e1db787247`, then rebuilt the unchanged 599 private
AOT pages with GCC. The previous CreateFile stop at dispatch round 69 was reproduced.
The new implementation executes the original guest CreateFile request:

```text
08080202 00000000 00000001 00000000 00000004 0000001c
00000000 00000014 00000000 00070002 0036a046
```

This requests `/gamecoin.dat`, UTF-16LE including NUL, 28 path bytes, 20 file bytes,
attributes zero, archive handle 1. The reply is `08080040 00000000` followed by zeros.
The host extends a newly and exclusively created file to the requested size. It
provides no save header, coin balance, step history or application initialization.

Before registering PTM, a diagnostic-only SVC trace showed why the fresh-file
branch entered the original Break handler:

```text
srv: GetServiceHandle ptm:u -> reply 00050040 d0406401 00000000 00000000
fs:USER CreateFile         -> reply 08080040 00000000 00000000 00000000
handle 00000000 header 000c0000 -> transport d8e007f7
handle 00000000 header 000b00c2 -> transport d8e007f7
stop=UnsupportedSvc pc=0x0011fb80 detail=0x0000003c dispatch_rounds=73
```

The generic last-valid-session field still displayed FS during those invalid
handle calls. The trace, not that stale field, identified the failed PTM lookup.
No Break was bypassed. Registering a real `ptm:u` discovery endpoint, with EVERY
command still unsupported, exposes the actual first request:

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=71
r0=0x00058017 r14=0x0012b528
last_ipc_session=ptm:u requested_service= request_header=0x000c0000
ipc_words= 000c0000 00000000 00000000 00000000 00000000 00000000 00000000 00000000
```

Pinned PTM identifies command 0x000C as GetTotalStepCount. No reply value has been
implemented. The earlier invalid-handle trace also contains GetStepHistory (0x000B),
but that is NOT a successfully exercised PTM handler.

## Separate existing-file branch

A second process using the 20-byte file just created by the first process receives
FileAlreadyExists from CreateFile. The original guest takes a different branch:

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=71
last_ipc_session=fs:USER requested_service= request_header=0x08030204
ipc_words= 08030204 00000000 00000003 00000001 00000001 00000002 0000000c 00000001
```

This is OpenFileDirectly, not ordinary OpenFile. Neither operation is implemented.
The file stays 20 zero bytes with SHA-256
`de47c9b27eb8d300dbb5f2c353e632c393262cf06340c4fa7f1b40c4cbd36f90`.
It is new, incomplete test state, NOT a recovered console file or a valid initialized
Play Coin save. A subsequent run does not exercise the fresh-file branch unless a
separate empty, explicitly provisioned test archive is selected.

## Contained implementation and explicit limits

The configured root is pinned with a directory descriptor on POSIX hosts. Numeric
archive directories and guest parents are opened relative to held descriptors with
O_DIRECTORY/O_NOFOLLOW; the leaf uses O_CREAT/O_EXCL/O_NOFOLLOW. Existing files are
not truncated, including simultaneous creation by two host processes. The guest
cannot supply an absolute host pathname, a dot component, or a symlink to follow.
Descriptors close through RAII. Replacing the selected root with a symlink does not
redirect writes: operations remain tied to the originally selected directory.

UTF-16 decoding validates NUL termination, embedded NUL, surrogate pairs, path
components and forbidden host characters. IPC validates exact header/descriptor,
initialization, full readable input, address wrap and writable response before I/O.
Bad guest pointers produce transport InvalidPointer; unsupported request shapes
stop without modifying the original request. Existing leaf objects return
FileAlreadyExists; an absent zero-size request returns UnsupportedOpenFlags.

Host containment is deliberately stricter than firmware/path normalization. The
16 MiB per-create and 4096-byte path bounds are host safety limits, not discovered
3DS capacities. The backend does not implement filesystem quotas, transactions,
attributes, NAND containers, file handles, Read, Write, SetSize, Close or durability
across power loss. Non-POSIX creation stays unsupported; no Windows build is claimed.

Unknown host I/O failures request an explicit IPC host stop instead of fabricating
a guest error or success. A failing resize may leave the just-created zero-length
file. This partial side effect is reported, not hidden by unsafe pathname rollback.
An RLIMIT_FSIZE=0 regression verifies that the process does not terminate through
the noexcept SVC boundary and no successful guest response is committed.

## Validation

GCC and Clang full 599-page native builds succeeded. All 14 CTest suites passed
with each compiler. Clang ASan/UBSan passed all 14 ROM-free suites with leak
checking and halt-on-error enabled. The two added suites cover file creation,
existing-file preservation, concurrent creators, Unicode, malformed IPC, pointer
and response guards, invalid archive handles, symlinks, root replacement, a forced
host resize failure, and PTM discovery/untouched unsupported calls.

GCC and Clang real-run logs are byte-identical for five scenarios: fresh file,
existing file, no root option, configured root missing the archive, invalid root.
No root option still stops at OpenArchive. A missing archive returns the existing
NotFormatted path and the guest Breaks; an invalid root is rejected with exit 2
without directory creation. Kernel guest time remains zero in these runs.

All 603 regular members of the restored AOT archive are byte-identical to their
private backup; 599 are page C++ files. The code.bin SHA-256 remains
`5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f`.
The 545111 raw-word and 111043 registry-block counts are STATIC verification counts,
not executed instructions, frames or completed gameplay.

Fresh full logs, diagnostic source, validation script and file manifests are in
the private source/log checkpoint's `createfile-checkpoint/` directory. No game
binary, original code image or private generated page is added to public GitHub.
No successful application initialization, save, title screen or gameplay is claimed.

## References inspected

`azahar-emu/azahar` pinned to `86a9f9236ae42bb5a2b995dbc933d599d8ea07ac`:
- `src/core/hle/service/fs/fs_user.cpp`: CreateFile IPC layout and OpenFileDirectly.
- `src/core/file_sys/savedata_archive.cpp`: CreateFile existence/size/error semantics.
- `src/core/file_sys/archive_extsavedata.cpp`: fixed-size extdata and shared ID policy.
- `src/core/file_sys/path_parser.cpp`, `errors.h`: paths and Result values.
- `src/core/hle/service/ptm/ptm_u.cpp`: observed command names, not invented outputs.

## Next work

Resolve the observed PTM GetTotalStepCount request using inspected service semantics
and a clearly documented source of step state; do not invent console history. Use
a NEW empty private test archive to exercise the full initialization branch. Keep
the existing-file OpenFileDirectly branch as a separate comparison, not as a bypass.

Hosted confirmation: GitHub Actions `37266660459` completed successfully for both
`runtime-test (g++)` and `runtime-test (clang++)` on implementation `654a8a2`.
These are ROM-free test jobs, not hosted gameplay or full game-data execution.
