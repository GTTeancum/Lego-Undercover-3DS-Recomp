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
- `tools/build_aot_page_manifest.py` — verified 599-page text manifest.
- `tools/generate_aot_pages.py` — reconstructed page-oriented C++ emitter.
- `cmake/LEGOGeneratedAOT.cmake` — build integration with hard 599-page validation.
- preparation, inventory/process, page-manifest, and page-emitter regression tests.
- `config/supported_revision.json`
- A pinned framework slice under `vendor/triaevum-a9b4477/`, including:
  - ARM decode/lift/IR and soft-float Python sources.
  - A32 core/runtime/memory/ALU C++ sources.
  - A32 VFP scalar/transport/binary64 C++ sources.
  - framework provenance metadata.

The LEGO-specific `src/*` directories still contain placeholders only; the
actual generated game AOT page contents and game-specific host/runtime implementation
have not yet been restored there.

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

## AOT page reconstruction

The later page layer now exists as working reconstruction source:

- Exactly **599 4-KiB guest text pages** are emitted as C++ translation units.
- Blocks crossing a guest page boundary are deterministically split at that boundary.
- Empty text pages remain represented in the ordered registry and safely resolve to no block.
- The recovery emitter produces **604 artifacts total**: 599 page C++ files plus five recovery support artifacts. Those five filenames are a reconstruction convention; their historical identities are still unknown.
- Four page-emitter Python tests pass.
- A complete synthetic 599-page output compiles and links successfully.
- CMake integration configures/builds the full page set and links a registry smoke executable successfully.

The emitter reuses the pinned frontend's real fixed-point literal/CFG/pointer logic
when Capstone is available, then repacks the resulting blocks into the page-oriented
layout. It does not invent raw game instructions.

## AOT coverage reconstruction

The surviving `adr-coverage-build2.log` remains an earlier intermediate, not the
final Recovery J layout. At 14:26 UTC on September 21 it built 135
`lego_shard_*.cpp` files and reported 111,312 blocks / 547,756 instruction slots.

A fresh LLVM 17 A32 disassembly of the verified executable rebuilt a conservative
control-flow baseline without Capstone. Entry + initializer + observed callback
roots cover 48,120 instruction slots. Adding validated absolute code-pointer roots
reaches **524,873 instruction slots and 106,400 block starts**. The preserved
frontend source has also recovered the actual fixed-point rules for literal pools,
absolute and base-relative switch tables, self-relative pointer tables, constant-PC
targets, and filtered pointer roots.

The exact frontend dependency remains `capstone==5.0.7`. The correct PyPI
manylinux x86-64 wheel was identified during this turn, but the active container
could not fetch that binary. Therefore no new claim is made that the reconstructed
page emitter currently reproduces Recovery J's exact game-page contents.

## Current boundary

The verified game input, executable preparation, pinned A32 frontend/runtime,
page-manifest layer, page-oriented C++ emitter, registry, and CMake build integration
are now safe in GitHub.

The next critical step is to run the pinned Capstone-backed analysis with the best
recovered title inventory, emit actual page contents, and compare their coverage
against the surviving historical checkpoints. After that, reconstruction moves into
the LEGO-specific native host/runtime, services, renderer, audio, and desktop frontend.

No current commit is claimed to reproduce Recovery J gameplay yet.


## Conservative real-page milestone — October 3, 2026

A second, dependency-independent recovery path now exists in
`tools/generate_aot_pages_llvm.py`. It uses Clang/LLVM only as a decoding aid,
validates the exact supported `code.bin`, and roots control flow from the real
entrypoint, all 296 recovered initializer callbacks, the three observed runtime
callbacks, and 5,313 aligned absolute code-pointer targets.

This path reproduces the previously established conservative **524,873 reachable
instruction slots** and emits a real page-oriented tree from the supplied game:

- **599** generated page C++ translation units.
- **604** total generated recovery artifacts.
- **106,846** emitted blocks after guest-page splitting.
- **589** nonempty pages.
- **3** LLVM-undecoded words.
- **3,819** deliberately unsupported operations, of which 3,816 are SIMD/VFP
  forms that are not assigned fabricated native semantics.

The generated source was validated beyond syntax-only sampling. All 599 real
game-derived page translation units compiled in parallel, the registry/functions
translation units compiled, and the entire set linked into a smoke executable.
That executable reported **599 registry shards** and **300 function/inventory
entries**.

The generated 52 MiB source tree is intentionally not committed as hundreds of
individual files at this stage. Its deterministic generator and validation
fingerprints are committed, so the exact conservative tree can be regenerated
from the verified local `code.bin`. See
`reports/recovery-aot-pages/LLVM-CONSERVATIVE.json`.

This is a genuine native generated-code checkpoint, but **not Recovery J parity**.
The historical Capstone-backed title inventory/fixed-point graph still needs to
replace the conservative absolute-pointer graph before a gameplay-equivalence
claim is possible. The September 21 547,756-slot / 111,312-block figure remains an
earlier comparison checkpoint rather than a final Recovery J identity.
