# Current reconstruction status

**Date:** October 3, 2026

The original local Recovery J source tree was lost. This repository is being
reconstructed from surviving checkpoint documentation, build evidence, the pinned
public TriAevum framework revision, and the user's original decrypted USA game image.

## Input recovered and verified

The newly uploaded six-part archive has now been decoded without relying on an
external 7-Zip install. Its CCI matches the original project revision exactly.

- CCI bytes: **1,073,741,824**
- CCI SHA-256: `3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525`
- Program ID: `00040000000AD500`
- Product code: `CTR-P-AA8E`
- Process: `LEGOCITY`
- Main NCCH offset: `0x4000`
- ExeFS `.code` compressed bytes: **1,701,108**
- Decompressed `.code` bytes: **2,650,112**
- Decompressed `.code` SHA-256: `5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f`
- Framework baseline: TriAevum `a9b447709d4405848d75352354891059cebb9ff8`

These identities match the surviving September 21 inspection record exactly.

## Source that survived the retry in GitHub

The earlier reconstruction attempt pushed more material before the retry than was
initially apparent. The repository currently contains real reconstruction source,
not only documentation:

- `tools/prepare_game.py` — CCI/NCCH/ExeFS/RomFS validation and preparation.
- `tools/generate_aot.py` — AOT generation recovery tooling.
- `tools/build_process_manifest.py` — process/translation manifest tooling.
- `tests/test_prepare_game.py`
- `tests/test_generate_inventory.py`
- `tests/test_process_manifest.py`
- `config/supported_revision.json`
- A pinned framework slice under `vendor/triaevum-a9b4477/`, including:
  - ARM decode/lift/IR and soft-float Python sources.
  - A32 core/runtime/memory/ALU C++ sources.
  - A32 VFP scalar/transport/binary64 C++ sources.
  - framework provenance metadata.

The LEGO-specific `src/*` directories still contain placeholders only; the
original generated AOT pages and game-specific host/runtime implementation have
not yet been restored there.

## Preparation reproduced again in the current scratch

A fresh local extraction from the uploaded archive independently reproduced the
same CCI and executable hashes above. A rebuilt preparation script also
successfully BLZ-decompressed the ExeFS `.code` to the exact historical
`code.bin` fingerprint. The derived `code.bin` remains local and is not committed.

## Native translation recovery evidence

The surviving September 21 adr-coverage build records a strong whole-program
coverage checkpoint:

- **111,312 ARM blocks**
- **547,756 unique instruction slots**
- **41 explicit traps**
- **0 pending roots**
- output target `LEGOChaseNative`

This checkpoint predates Recovery F/G and therefore is **not** treated as proof of
Recovery J's final block/slot counts. It remains the best recovered quantitative
coverage reference unless later evidence establishes that the same counts carried
forward unchanged.

The surviving Recovery J report independently records **604 generated AOT files**,
all unchanged from Recovery I. Recovery F/G and Focus K01 establish that the later
tree used generated AOT pages. The reconstruction must satisfy the later page-layout
evidence as well as the older coverage evidence before any claim that Recovery J
has been restored.

## Current boundary

The exact game input, executable extraction, reconstruction tooling, tests, and
a useful pinned TriAevum source subset are now safe in GitHub. The major remaining
loss is the 604-file generated AOT tree plus the LEGO-specific native host/runtime,
renderer, services, audio, and desktop frontend implementation.

No current commit is claimed to reproduce Recovery J gameplay yet.

## AOT coverage reconstruction

The surviving `adr-coverage-build2.log` is now correctly treated as an **earlier
intermediate**, not the final Recovery J generator layout. At 14:26 UTC on
September 21 it built 135 `lego_shard_*.cpp` files named across `00040` through
`000D5` and reported 111,312 blocks / 547,756 instruction slots.

Recovery F, produced later that day, and Recovery G/J document a later source tree
with **604 generated AOT files**. The executable text allocation is exactly **599
4-KiB pages**, and Focus K01 explicitly describes compiling four unchanged
"generated AOT pages." This is strong evidence that the final project used a
page-oriented LEGO generation layer that is not present in the pinned generic
TriAevum frontend. The exact five additional generated artifacts and page filename
scheme remain to be recovered.

The pinned TriAevum frontend's `shard_size` setting is an **operation-count
limit**, not a guest page size. The reconstruction wrapper has therefore been
returned to its 0x1000 operation-limit default and is being used only for coverage
analysis while the later LEGO page generator is reconstructed.

A fresh LLVM 17 A32 disassembly of the verified executable was used to rebuild a
control-flow baseline without Capstone. Entry + initializer + observed callback
roots cover 48,120 instruction slots. Adding validated absolute code-pointer roots
reaches **524,873 instruction slots and 106,400 block starts**. The preserved
frontend source has also recovered the actual fixed-point rules for literal pools,
absolute and base-relative switch tables, self-relative pointer tables, constant-PC
targets, and filtered pointer roots. Local LLVM emulation of those rules is being
used diagnostically; it is not claimed byte-identical to the lost Capstone-backed
generator.

A deterministic page-manifest layer is now reconstructed in
`tools/build_aot_page_manifest.py`. It verifies the exact executable and enumerates
all **599 4-KiB text pages**, including per-page hashes generated locally. Its tests
also pin the four distinct pages used by Focus K01. See
`reports/recovery-aot-pages/README.md`.

The next AOT step is to reconstruct the later page-oriented C++ emitter and its
registry/dispatch metadata while continuing to recover the lost title-specific
function/root inventory. The five additional generated artifacts implied by the
604-file Recovery J count are still unidentified.
