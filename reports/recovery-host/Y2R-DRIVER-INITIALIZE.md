# Native LCD validation and Y2R driver initialization

Implementation: 1cf1c1d1ec6af90854c4dac44d997a511d9ed70d.
Tree: 871749f15b6f11e42326da896feb31703405ed1b. Base: dfa588edbf998155981aa838533b73f12cd82f15.

The previous hosted-only LCD checkpoint was restored and run with the original
599-page executable. LCD succeeds; the missing y2r:u lookup caused the original
Break at round 234. Registering a discovery-only boundary exposed DriverInitialize,
0x002B0000. The final implementation resets real persistent configuration and
clears its retained one-shot event; it does not perform conversion or signal completion.

The pinned reference's SetInputLines(1024) deliberately leaves the old line count.
Initialization also retains src_yuyv and padding. Tests exercise nondefault fields,
not just already-zero construction. A one-session lease follows the pinned limit;
duplicated handles retain it, failed handle allocation releases it, and config/event
objects survive disconnect/reconnect. Unsupported operations preserve the request.

Original startup now reaches cfg:u GetConfig at PC 0x0025947C, round 235:
`00010082 00000020 00050005 0000020c 00594218`. Pinned block 0x50005 is
StereoCameraSettings. Its 32-byte output and IPC remain untouched. Existing-gamecoin
reaches the same request at round 227. No settings or calibration were fabricated.

Logging-only GCC/Clang captures agree across all 52 files per compiler. LCD reply is
0x000B0040/0; Y2R reply is 0x002B0040/0; CFG is unmodified. Both LCD words stay zero.
Every GPU register, upload word, shared-page byte/epoch and the 6 MiB VRAM bank is
unchanged across these calls. Time stays 16713681 ns. Y2R changes width to 1024,
leaves lines zero, and its event stays unsignaled. VRAM is still all zero, so no
meaningful screenshot, rendered frame, main menu or gameplay is established.

Full GCC and Clang native builds passed all 38 suites each; Clang ASan/UBSan passed
38 ROM-free suites. Seventeen startup cases have identical logs/exits/test-file bytes.
All 603 private AOT members, original executable and raw RomFS identities match.
The native opcode backend, scheduler, GPU/LCD code and production IPC are unchanged.
Setup timeouts were resolved by bounded continuation and a new matrix root; notes
retain them. No native test was removed or suppressed. Packaging now checks exact
configured test names rather than a hardcoded count.

Private evidence/scripts/captures: lcd-native-checkpoint/ in the private checkpoint.
Public summary: Y2R-TRACE-PROOF.json. The source/hosted predecessor and older private
captures remain in separately retained historical archives. Next inspect CFG block
layout, permissions and provenance before returning its actual data.

Primary pin: azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac,
src/core/hle/service/cam/y2r_u.cpp/.h and src/core/hle/service/cfg/cfg.h,cfg_u.cpp.

Hosted confirmation: Actions 37407926702 passed GCC and Clang on 1cf1c1d.
These hosted tests are ROM-free, separate from original-game execution in scratch.
