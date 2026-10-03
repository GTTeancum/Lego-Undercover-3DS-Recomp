# Recovery Index

This repository is a reconstruction of the lost local LEGO City Undercover: The Chase Begins 3DS recomp working tree.

## Known checkpoints

1. Initial inspection — decrypted USA image verified; executable and asset inventory established.
2. Stage 2 — early translated/native bootstrap work.
3. Recovery F — interactive 3D gameplay reached under native Linux execution.
4. Recovery G — SDL desktop window, keyboard/controller polling, mouse/touch and device audio integration.
5. Recovery I — baseline later used by Recovery J; 604 generated AOT files.
6. Recovery J — cumulative native Linux crash-hunt checkpoint and current primary recovery baseline.
7. Focus K01 — additive shadow-projection diagnostic supplement; does not replace Recovery J.

## Recovery rules

- Prefer byte-identical surviving files over reconstruction.
- Preserve hashes/manifests whenever available.
- Keep checkpoint documentation even when superseded.
- Do not commit original game image, code.bin, RomFS, extracted game assets, saves, or captured game audio.
- Recovered source should be compared against documented manifests/checksums before being treated as canonical.

## Documented Recovery J identity

- Generated AOT files: 604.
- Manifest entries documented by Focus K01: 1,779.
- Recovery J Linux ELF SHA-256: `9f2518d2f893ffd53bc6073169309bbc5a92ae5048d5474412634fcb9b6a827b`.
- Recovery I uploaded ZIP SHA-256 recorded by Recovery J: `ddc189ef30817e142e5a6e6f1e9f5b55333557aab2e99fe71cd6bf00efb71460`.
- Focus K01 records its Recovery J baseline ZIP SHA-256 as `6920cfa152eda0f05e02aa97810540cb0e0e14ee4a028e28098bebf7d4d881be`; retain both provenance records until the exact archive relationship is re-established.

## Next recovery action

Locate/materialize the latest surviving Recovery J source archive and Focus K01 supplement, verify their recorded identities, then populate the source tree without altering recovered files.
