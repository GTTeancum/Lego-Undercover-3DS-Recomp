# LCD force-black control — ROM-free verified checkpoint

Implementation: `82cf8dfe9abeeb4f628bab94d1c0abc282079c66`.
Base: `da5c9a9e8aa74567deae519380caa86e06e35247`.
Hosted candidate run: `37404725362`.

## What changed

GSP now handles exact `SetLcdForceBlack` IPC `0x000B0040`. The low byte of its
argument selects false/true, matching pinned `IPC::RequestParser::Pop<bool>`.
It replaces both shared LCD color-fill controls with zero or `0x01000000`:
RGB stays zero and enable is bit 24. These are separate LCD words, not PICA
registers or framebuffer pixel writes. Connected nonowners and unregistered
clients use the same global control, as in the inspected reference handler.

The successful response has header `0x000B0040`, result zero, and zero remaining
words. Existing router response preflight precedes state mutation. Malformed
headers remain unsupported and preserve the request/state. No handle, interrupt,
queue progress, display period, framebuffer pixels or GPU completion is generated.
Read-only `lcd_color_fill_word(0/1)` provides top/bottom diagnostic access;
out-of-range screen indexes return nullopt. Initial zero follows reference-HLE
LCD state construction, not recovered hardware state.

## Verification actually performed

The local execution environment could not start either shell commands or Python.
No original-game process, local build, private-input hash pass or screenshot ran.
The implementation was placed on a separate branch and compiled in GitHub Actions.
GCC Release, Clang Release and Clang Debug ASan/UBSan each passed all 37 ROM-free
CTest suites. Leak detection and halt-on-error were enabled for sanitizers.
No existing suite was removed or changed. The inherited legacy-barrier narrowing
warning remains visible. No native compilation or test failure occurred in this run.

The new suite verifies all 256 low-byte values with zero and nonzero upper bits;
the exact original zero request; enable, disable and repeated calls; shared and
independent services; nonowner and unregistered clients; duplicate handles;
retained event lifetime; exhausted handle capacity; read-only, partial and
write-only command areas; malformed headers; unchanged full PICA register/upload
state; unchanged shared page and epochs; and unchanged nonzero 6 MiB test VRAM.
That synthetic bank never enters an original-game run and is not visual progress.

## Boundary and next work

The LAST VERIFIED ORIGINAL-GAME stop is still the baseline's SetLcdForceBlack call
at PC `0x0025947C`, dispatch round 232, argument zero, after one display period.
The new handler is unit-tested, but the game has not been rerun. Its next stopping
point is therefore unknown. No full 599-page executable build is claimed here.
No useful screenshot, rendered frame, main menu or gameplay is established.

Next: restore this source with the private code/AOT/RomFS backups, rebuild the
full native executable, and run original startup in a new empty shared archive.
Observe the actual LCD response and next stop. Preserve explicit empty PTM,
reference-zero device VRAM and reference-idle display-clock options. Do not infer
that disabling blanking renders a frame or bypass a subsequent request.

## Evidence and recovery

`LCD-HOSTED-VALIDATION.json` records the candidate result. The new ROM-free checkpoint
workflow retains configure/build/test logs and JUnit output, then packages exact
committed source, test evidence and the updated handoff after three successful jobs.
Its archive verifier checks every member; source bytes are checked against Git blobs.
This hosted validation is not a local round-trip. The package receipt records the
final source commit, tree, run, file counts and archive checksum.

The private predecessor `LEGO-Chase-source-checkpoint-da5c9a9.tgz` remains necessary
for its original-game captures. Original game inputs remain in their separate
Library backups. The new public-source/hosted-log package does not contain them.

## Pinned references

`azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac`:
`src/core/hle/service/gsp/gsp_gpu.cpp` (`SetLcdForceBlack`),
`src/core/hle/ipc_helpers.h` (`Pop<bool>`), `src/video_core/gpu.cpp`
(`SetColorFill`), and `src/video_core/pica/regs_lcd.h` (`ColorFill`).
Exact blob hashes are in the canonical continuation handoff.
