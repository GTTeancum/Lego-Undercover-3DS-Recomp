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

## Native translation recovery targets

Surviving build evidence records this later whole-program generation checkpoint:

- **111,312 ARM blocks**
- **547,756 unique instruction slots**
- **41 explicit traps**
- **0 pending roots**
- output target `LEGOChaseNative`

The surviving Recovery J report records **604 generated AOT files**, all unchanged
from Recovery I. The reconstruction must converge toward these documented
boundaries before any claim that Recovery J has been restored.

## Current boundary

The exact game input, executable extraction, reconstruction tooling, tests, and
a useful pinned TriAevum source subset are now safe in GitHub. The major remaining
loss is the 604-file generated AOT tree plus the LEGO-specific native host/runtime,
renderer, services, audio, and desktop frontend implementation.

No current commit is claimed to reproduce Recovery J gameplay yet.
