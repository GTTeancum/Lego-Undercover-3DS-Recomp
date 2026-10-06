# DSP special configuration and bounded startup protocol

Parent: `3aca88f93a0305d37463525bca2bbd25893c188b`.
Status: locally tested pending source; no publication or new hosted CI this turn.

## What changed

The loader's special data has an explicit host source. The default remains
unconfigured. `--dsp-special-profile empty-config` selects a deliberately empty
system-configuration profile for block 0x70000. The native read of that narrow
profile returns Missing and the documented DSP-loader fallback supplies 532 zero
bytes. This is not recovered calibration, a measured hardware reset, a newly
installed cfg:s guest endpoint or an assertion that the user's console lacks data.
It does not expand the existing guest-visible CFG stereo profile.

`--dsp-special-block FILE` instead reads exactly 532 host-supplied bytes. Length,
open and read failures reject launch, never trigger the missing-CFG fallback.
The bytes' hash is recorded; their authenticity/calibration validity is not asserted.
These options are mutually exclusive and duplicate selection is rejected.

Special staging checks declared type/length, word-address bounds, selected banks
and existing known bytes. Allocation/hash work precedes mutation. Failed staging
preserves both the image and provenance receipt. Only the declared range becomes
known. The ordinary image parser is unchanged; other SRAM gaps remain unknown.
The service still returns an explicit HOST STOP, not a LoadComponent response.

A separate DspBootHandshake component now consumes actual backend-supplied mailbox
words in the reference order: ready word 1 from registers 0, 1, 2, then a distinct
register-2 pipe-base word. Non-one readiness replies are discarded, missing replies
request execution, and polling has a 256-read host bound with resumable state.
Backend failure is terminal. The flag omitting ready replies is handled separately.
It has no firmware executor, generated replies, guest-ready flag or timing effects.
IT IS NOT CONNECTED TO THE LIVE SERVICE. Synthetic mailbox fixtures validate the
protocol logic only. Protocol completion would not alone establish a running DSP;
pipe layout, memory, peripherals and continuing execution remain backend work.

## Original-game result

Both modes still stop at the same unchanged request:

```
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=252
last_ipc_session=dsp::DSP request_header=0x001100c2
```

With the explicit empty profile, the host diagnostic now says ordinary and special
bytes are staged but the firmware executor/boot handshake is unimplemented.
The region at data byte 0x1DE52 (word 0xEF29), length 532, becomes known, with SHA-256
`47bed81b9dd45f914b6f4c65d96eb6803050cbb576f6f4d2e7911d0b35983b51`.
Known data grows from 5256 to 5788 bytes; known program stays 43692. All other unknown
bytes remain unknown. The raw allocation was already zero there: the original
capture verifies the provenance/known-mask change, not a change to its raw value.
Nonzero supplied-block unit fixtures separately verify actual value copying.

All 81 capture files per mode match between GCC and Clang. Comparing default with
empty-config changes only three HOST diagnostic files: data-known mask, staging
metadata and the special-source receipt. Independent Python replay reconstructs
the complete program/data banks and masks from the original component and the
explicit fallback. The added known-mask range is exactly 532 bytes.

The complete captured IPC/component, GPU words/uploads, GSP/HID pages and epochs,
mapped HID bytes, LCD state, 6 MiB VRAM and recorded CPU/thread/time state remain
unchanged across the DSP call. There are four threads, 29 handles and 18447051 ns.
No original DSP instruction has executed. The original core-1 worker still has
zero issued instructions at this point. No logo, frame, menu, audio or gameplay.

## Completed validation

Full GCC and Clang executables link all 599 original AOT page units. All 52 CTest
suites pass on each compiler, and all 52 ROM-free Clang ASan/UBSan suites pass with
leak checking and halt-on-error. Every previous suite remains unchanged. Two added
suites cover special sources, per-byte placement, all bank types, range/layout/
overlap guards, allocation rollback, host-file errors, service non-acknowledgment,
protected replies, ordered mailbox reads, work limits, retries and backend failure.
All 65536 possible pipe-base words are captured without inventing address semantics.

Ten paired CLI/startup cases match compiler stdout, exit codes and owned test files:
default, empty-config, strict CPU, missing CFG, and six argument/file rejection cases.
Code.bin, raw RomFS, ExHeader and all 603 AOT archive members retain their identities.
Full IVFC verification was not repeated. These checks are not an independent CPU
oracle or a hardware-timing validation. No Windows/macOS build was performed.

Early container startup failed temporarily. Streaming builds were unavailable, so
finite build chunks retained their interruption logs. One premature Clang CTest run
reported missing/not-yet-built tests; its incomplete log is retained separately from
the final passing result. No failed final test was hidden. See setup-notes.txt.

## References and next work

3dbrew, DSP Binary: special data source and missing-read fallback (read October 6,
2026). Azahar pin `86a9f9236ae42bb5a2b995dbc933d599d8ea07ac`,
`src/audio_core/lle/lle.cpp`, blob `388fe64ec1a5130a2c93a5dfa04ca84df109b567`:
boot receive order; its special load is TODO. Its existing HLE engine is not present
in this project; the front-end success reply cannot substitute for such a backend.

The pinned Azahar Teakra submodule is `wwylele/teakra` commit
`3d697a18df504f4677b65129d9ab14c7c597e3eb`. Its public API was inspected, not imported
or executed. A source download was unavailable in this environment. No Teakra or
other external runtime dependency was added.

Next implement/integrate a genuine bounded DSP executor or faithful native HLE
backend, with known-memory tracking and observed mailbox/pipe state. Connect the
protocol only to real backend replies. Do not copy unknown storage as initialized
SRAM, manufacture a pipe base, or mark the guest load successful to bypass this stop.
Private current evidence: dsp-boot-checkpoint/. The attached handoff gives recovery.
