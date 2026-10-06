# CFG stereo configuration — original startup advances

Baseline: `237def70338cb07641a9daec4898b320f78a724b`.
Tested implementation tree: `cd08e620bac867620213031bd50fc50aff715e58`.
Implementation commit is recorded in the continuation handoff after publication.

## Executed request

The original game successfully reads block 0x00050005 at 0x00594218:
request `00010082 00000020 00050005 0000020c 00594218`,
reply `00010042 00000000 0000020c 00594218` with remaining words zero.

The new `--cfg-profile reference-stereo` option selects only the pinned HLE's
eight-binary32 stereo default. It is NOT recovered console calibration, a measured
hardware reset value, a full CFG save, or guessed zero data. Default execution
keeps CFG unconfigured and preserves the old untouched round-235 stop.
The reference itself does not fully identify the eight fields; no field meanings
beyond source order are invented here.

Data SHA-256: `eb666a350f0f714ab7ca73c9ee5cdace24f90cb79b7afa30fd03be193b52c5b6`.
An independent Python binary32 serialization matches all 32 output bytes. Twenty-five
bytes change from the original destination; all 64 captured neighbour bytes remain
unchanged. The original ARM wrapper at 0x0012B050 builds a write-only mapped descriptor,
calls SendSyncRequest and reads the service Result. No original instruction changes.

## New boundary

Fresh startup reaches APT:U `SetAppCpuTimeLimit` at dispatch round 238:
`004f0080 00000001 0000001e 00090020 00000000 ...`.
Only words 1 and 2 are request arguments (1, 30); word 3 is trailing stale buffer
content, NOT a translated handle. The request is unsupported and untouched.
Existing-gamecoin reaches round 230. Guest time remains 16713681 ns after one display
period. No CPU limit has been applied and no success reply has been manufactured.

All 78 GCC/Clang trace files match. The full 1842-word GPU state, upload state,
4096-byte shared page, its epochs, and 6 MiB VRAM are unchanged across the CFG call
and rejected APT call. The APT service connection increases the open-handle count
from 18 to 19 before its call. VRAM remains all zero; no useful screenshot,
main menu, shader execution or gameplay has been demonstrated.

## Scope and safety

Only exact command 1, 32-byte size, block 0x00050005 and write-only mapped descriptor
0x20C are supported. Other blocks, sizes, descriptors and commands remain explicit
host stops. The table is immutable. This retains the existing shared CFG endpoint
model; no claim of complete firmware session-limit behavior is added.

Full destination and response permissions are checked; wrap, unmapped, read-only,
fragmented and response-alias outputs do not write data. Shared outputs remain
unsupported host policy. The existing PrepareDeviceWrite/CommitDeviceWrite path
preallocates private reservation metadata and performs allocation-free byte commit;
it invalidates written reservations without changing neighbours. No loader writes,
mapping fabrication, NAND creation, scheduler change or calibration persistence.

## Verification

Both GCC and Clang full native builds link all 599 unchanged AOT pages. All 39
ROM-free CTest suites pass with GCC, Clang and Clang ASan/UBSan, with leak checking
and halt-on-error. Twenty paired original-startup cases match in stdout, exit codes
and newly created test-file bytes, including strict and malformed CFG modes.
All original code, raw RomFS and 603 AOT backup members are byte-identical.
Full IVFC block checking was not repeated; the raw RomFS hash matches its prior
verified image. Static registry counts are not executed CPU instructions or frames.

No native test failed. Setup-only issues (container shell failure, corrected manifest
key/ANSI summary parsing, bounded build chunks and unrelated child-Python warmup)
are recorded in private cfg-checkpoint/setup-notes.txt. No earlier suite was removed.

## Primary reference and next work

Pin: azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
CFG GetConfig/GetConfigBlock/permissions/size checks: cfg/cfg.cpp
(blob 6eb7302a942c8b34eaa7268eb0411a916a195020).
Exact defaults and Global permissions: cfg/cfg_defaults.cpp
(blob 424664396a50c369dd7401b97372b924e28def2e).
APT command identification: apt/apt_u.cpp
(blob 57539fbee163a7fbb0119222cd15abf36ea478cb).
APT handler: apt/apt.cpp
(blob e40bd9c82633aeac18b6116c11a931068bab0b75).
Paths have prefix src/core/hle/service/.

The APT handler forwards to PM:APP UpdateResourceLimit(CpuTime, value), not a
standalone success stub. Next inspect that resource-limit state and its kernel
relationship before implementing the observed call. PM/kernel behavior has NOT
been implemented or fully inspected in this checkpoint. Then rerun original code,
rather than predicting the next request.

Private scripts/logs/captures and the complete 20-case validation receipt are in
cfg-checkpoint/. Public independent byte/state summary: CFG-PROOF.json.

## Hosted confirmation

GitHub Actions run 37410669875 on implementation 1bc3c7d passed both GCC and Clang
jobs. These are ROM-free suites, separate from the full local original-game runs.
