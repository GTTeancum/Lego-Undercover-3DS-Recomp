# Current reconstruction status

**Date:** October 3, 2026

The original local Recovery J source tree was lost. This repository is being
reconstructed from surviving checkpoint documentation, build evidence, the pinned
public TriAevum framework revision, and the user's original decrypted USA game image.

## Input recovered and verified

The newly uploaded multipart archive was decoded and the contained CCI was checked
against the identity recorded by the original project.

- CCI bytes: **1,073,741,824**
- CCI SHA-256: `3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525`
- Program ID: `00040000000AD500`
- Product code: `CTR-P-AA8E`
- Process: `LEGOCITY`
- Decompressed `.code` bytes: **2,650,112**
- Decompressed `.code` SHA-256: `5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f`
- Framework baseline: TriAevum `a9b447709d4405848d75352354891059cebb9ff8`

These identities match the surviving September 21 inspection record exactly.

## Reconstructed code now present

`tools/prepare_game.py` is a dependency-free preparation/verification tool for
the supported decrypted cartridge revision. It validates the CCI identity,
parses NCSD/NCCH/ExeFS/RomFS bounds, BLZ-decompresses ExeFS `.code`, verifies
the recovered executable identity, extracts ExHeader/code locally, and emits a
profile containing the executable map and permitted service names.

The focused preparation unit tests pass, and an end-to-end run on the newly
uploaded CCI reproduces the original `code.bin` size and SHA-256.

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

The game input and preparation layer are recovered. The original generated AOT
C++ and LEGO-specific native host/runtime source are still being reconstructed.
No current commit is claimed to reproduce Recovery J gameplay yet.
