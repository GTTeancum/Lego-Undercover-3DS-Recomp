# Explicit language reads and recovered compiled-code entries

October 7, 2026. Extends canonical main `9fc4085f346d49dbb1c3f549362a049b08505147`.
The newer baseline already implemented the procedural LUT and diagnostic audio file
sink. Those features were recovered and reproduced, not reimplemented in this turn.

## Actual original-game result

`--cfg-language en` enables the game's one-byte language reads, independently of
camera and sound settings. Three English reads return byte 1, with exact mapped-buffer
responses and all 64 captured neighbouring bytes preserved. Time, thread count and
handle count do not change across these service calls. Omission retains the previous
unsupported language request. French and Spanish also reach the next DSP request;
this does not establish translated UI, controller input or completed audio setup.

Startup exposed two missing compiled-code locations: an indirect leaf at `0x00249B80`
and an original callback return at `0x0024D3D0`. A build-time generator adds three
bounded blocks containing 13 original instruction words. LLVM supplies instruction
metadata through the existing recovery frontend. The complete executable hash is
checked before generation, and the launcher validates every added word against the
same verified input. The existing 599 AOT translation units and all 603 saved AOT
members remain byte-identical. There is no runtime decoder, missing-PC success stub,
PC skip, patched return value or modification of the game's instructions.

With language and the compiled supplement, the English run stops at:

```
stop=UnsupportedIpc pc=0x0025947c detail=0x32 thread=7 dispatch_rounds=592
last_ipc_session=dsp::DSP request_header=0x00130082
ipc_words=00130082 142f0d80 00000800 00000000 ffff8001 ...
cpu_ticks=44119742 core0_instructions=14421785 core1_instructions=99
guest_now_ns=164557222 display_periods=9 threads=7 handles=37
DSP scheduled_slices=1195 notification_wait_slices=1 completed/attempted=19628032
```

This is `DSP FlushDataCache`, for 2,048 bytes at `0x142F0D80` and the current-process
pseudo-handle. It is still unsupported; this turn does not return a cache-flush
success or enable the DSP external memory bus. French reaches the same operation
at round 594; Spanish at round 592. No rendered logo, title screen, main menu or
playable output exists. VRAM contains only the inherited depth-clear pattern.
The 4,786 captured English stereo records are actual FIFO-provenance startup zeros,
not music/effects or audible playback. No replacement samples were introduced.

## Contracts

Language is an explicit host selection, not recovered console NAND or inferred host
locale. Supported enum bytes follow the pinned CFG SystemLanguage 0..11. The CLI
accepts ja/en/fr/de/it/es/zh-cn/ko/nl/pt/ru/zh-tw. Selecting a locale is not proof the
USA title includes it. All 12 values are component/argument tested; original-game
comparisons cover English, French and Spanish only.

The existing mapped-buffer output path retains exact shape, size, descriptor,
permissions, backing-alias and allocation preflight. Language cannot enable camera
or sound configuration, and those settings cannot enable language. Unrelated CFG
blocks remain unsupported.

`LEGO_SUPPLEMENT_CODE_BIN` is an explicit build-time private-input option. Without
it, language-enabled startup still diagnoses the first missing compiled entry.
The generator only covers reviewed, aligned, nonoverlapping, single-page bounded
blocks from the authenticated title; it rejects unsupported instruction metadata
and incomplete/control-flow ranges. It never scans missing PCs during execution.
The registry wrapper merges only into empty ranges, retains source-op pointers,
rejects overlaps and invalid extents, and leaves the base registry unchanged.
Both initial and supplemented registries retain the existing missing-PC and Thumb
guards. The supplement is rebuildable private generated C++, not public game code.

## Validation completed locally

All 69 configured CTest suites pass under GCC, Clang and ROM-free Clang ASan/UBSan,
with exact JUnit name-set matching and no skipped/duplicate suites. Full GCC/Clang
executables link all 599 original page units plus the three-block supplement.
The earlier 67 suites remain unchanged. New suites exercise language values,
independence, copyout, buffers, aliases, allocation failure, reservation invalidation,
registry lifetime/overlap/ranges and actual synthetic stepping through added entries.

Six original-startup pairs match stdout, stderr, exit and exclusively owned output
files: English, French, Spanish, omitted language, omitted sound, omitted camera.
Seven invalid CLI pairs reject before reading code; all 12 valid spellings parse on
both compilers. The 28 final capture files match byte-for-byte between compilers.
Independent parsing verifies the three language copyouts, exact final request, depth
clear and complete FIFO-tagged audio file. The capture host matches normal CLI logs
when only its owned-root path is normalized. This is not an independent CPU oracle.

Additional generator validation checks malformed ranges/disassembly and every
original supplemental word. An isolated execution of the actual generated leaf
against a synthetic X/Y buffer confirms one pointer increment and a returned X,
with source bytes and flags unchanged. That fixture is not original gameplay.
Code.bin, the complete raw RomFS, original ExHeader and all 603 AOT members retain
their verified identities. Full IVFC checking, Windows/macOS builds, physical timing,
playback and renderer tests were not performed. Hosted results belong to the final
publication receipt, not these local test claims.

Current private evidence: `language-checkpoint/`. The canonical handoff documents
exact commands, build options, restoration and the next cache-operation investigation.
