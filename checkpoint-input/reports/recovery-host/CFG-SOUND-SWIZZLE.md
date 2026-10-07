# Explicit CFG sound preference and staged PICA swizzle uploads

October 7, 2026. Source extends published main 3089a50280cba142d452ae414630d55b80a9b32f.

## Actual original-game progress

`--cfg-sound-mode stereo` supplies byte 1 for CFG block 0x70001, independent of the
existing camera compatibility profile. The request at round 295 receives the exact
mapped-buffer reply and modifies only its one output byte. Mono=0 and surround=2 are
also explicit choices. Omission preserves the old unsupported read; no recovered
console setting, calibration or working host audio device is claimed.

The original game advances to GPU work at round 357. It has completed 259 scheduled
DSP slices (plus one earlier synchronous mailbox-wait slice), and the original
second-core worker has executed 99 recorded instructions. These counters do not
establish completed audio initialization or gameplay. CPU timing remains diagnostic.

The new GPU list is 23,360 bytes. Swizzle ports are now supported, with raw upload
values, full-width offset increments, 4,096-entry reference storage, per-entry written
flags, and the existing conditional VS-to-GS mirroring. Independent raw-packet replay
matches all 83 staged swizzle words and both written masks from the original list.

The SAME list next stops on a procedural-texture lookup upload at register 0xB0,
byte offset 0x5280. The queue's existing whole-batch validation means this list is
NOT committed: the 83 descriptors are in a disposable plan, not live GPU state.
The queued request, CPU, GPU registers/uploads, queue/epochs and captured memory are
unchanged across that rejection. No drawing or completed GPU interrupt is fabricated.

There is still no visible logo, title screen, main menu or rendered frame. The final
6 MiB video-memory bank remains zero. Audio capture reports 1,042 retained pairs;
there is no playback, and this turn does not certify their content as music or effects.

## Implementation scope

The camera profile and sound selection are independent immutable service inputs.
The sound handler accepts only the exact one-byte mapped-write shape. It uses the
existing permission, alias and prepared-write path; malformed requests, invalid
outputs and allocation failures do not produce a fabricated successful read.
No sound choice silently configures camera defaults or changes DSP/capture policies.

Swizzle words are uploaded, not executed. Capacity follows the pinned reference,
not a new hardware measurement. Out-of-range uploads fail the complete plan; unknown
draws, default attributes, fog/procedural tables and command-list chaining remain
unsupported. Unwritten descriptor slots are explicitly unasserted, not usable zeros.

No original game/AOT bytes, CPU decoder, DSP interpreter, scheduling, filesystem,
HID input sampling or renderer were changed. The former unit test expecting all
swizzle writes to fail was updated; its other unsupported-effect checks remain.

## Verification

Full GCC and Clang builds link all 599 unchanged private AOT pages. All 65 CTest suites
pass with each compiler, and all 65 ROM-free Clang ASan/UBSan suites pass with leak
checks and halt-on-error. Configured and JUnit test names match without skips or duplicates.
New coverage includes all sound choices, cross-profile isolation, exact copyout,
permissions/aliases/epochs/allocation failure, all swizzle ports/masks, mirroring,
full-table bounds, multi-packet persistence and complete-plan rejection.

Six original-startup compiler pairs match stdout/stderr/exit and owned test-file bytes:
stereo, mono, surround, omitted sound mode, no camera profile, immediate DSP boot.
Seven invalid-option pairs reject before game-input access. All 129 diagnostic capture
files match between compilers. These are logging-only captures, not an independent
CPU emulator comparison. Ordinary executables independently reach the same stopping point.

Original code.bin, full raw RomFS, ExHeader and all 603 AOT backup members retain their
verified identities. Full IVFC verification was not repeated. No Windows/macOS build,
audio playback, HID control or rendering test was performed.

## Next work and references

Inspect the original procedural-LUT packet and the pinned register/table contract.
Do not merely accept register 0xB0, discard its words or commit the list's valid prefix.
Continue from unchanged original code and retain every explicit compatibility policy.

Primary source pin: azahar-emu/azahar at 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
CFG cfg.h defines mono/stereo/surround; cfg_defaults.cpp defaults to stereo, but this
host still requires selection. PICA pica_core.cpp defines upload port behavior and
conditional mirroring; regs_shader.h defines offsets; shader_setup.h provides capacity.
Exact blob IDs and read ranges are in the private reference receipt.

Current private evidence: sound-checkpoint/. See the canonical handoff for recovery.
