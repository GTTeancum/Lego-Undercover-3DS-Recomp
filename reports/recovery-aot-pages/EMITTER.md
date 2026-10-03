# Recovery J page-emitter checkpoint

This checkpoint reconstructs a buildable page-oriented source layer on top of the pinned TriAevum A32 frontend.

Verified locally:

- 599 generated C++ page translation units, one per 4-KiB guest text page.
- Deterministic page split at guest page boundaries, including blocks crossing a 4-KiB boundary.
- Empty text pages still contribute ordered registry shards and resolve to no block.
- Recovery layout emits exactly 604 artifacts: 599 page C++ files plus five support artifacts.
- The five support filenames are a recovery convention and are **not** claimed to match the lost historical filenames.
- Four Python page-emitter tests pass.
- Synthetic output containing all 599 page translation units, registry, and function table compiles and links successfully against a minimal A32 runtime ABI stub.
- The linked synthetic registry reports 599 ordered shards.
- `cmake/LEGOGeneratedAOT.cmake` validates all 599 page translation units plus the five recovery support artifacts and wires the generated C++ into a target.
- A fresh CMake configure/build of the complete synthetic 599-page set passed, including a linked registry smoke executable.

The full game-page generation path reuses the pinned frontend's fixed-point CFG/literal/pointer analysis and then splits its emitted blocks at 4-KiB guest page boundaries.

The exact required dependency is still pinned to **Capstone 5.0.7**. Its correct manylinux x86-64 wheel was identified from PyPI, but this container could not fetch the binary artifact. The actual game-page analysis therefore has not been rerun with Capstone in this checkpoint.

No synthetic generated page C++ is being presented as recovered game code. The checkpoint reconstructs the emitter, registry, layout, and build integration only.
