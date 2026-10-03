# LEGO City Undercover: The Chase Begins — initial inspection

**Date:** September 21, 2026  
**State:** Input verified and executable extracted. No playable recompilation has been built.

## Input verification

The three supplied 7-Zip parts total **564,616,423 bytes**. They were joined in
numeric order. The 7-Zip start-header CRC, next-header CRC, and total archive length
checks passed. Extraction completed without an error.

The extracted CCI image is **1,073,741,824 bytes**.

- Title ID: `00040000000AD500`
- Product code: `CTR-P-AA8E`
- Process name: `LEGOCITY`
- Remaster version: `0`
- Main NCCH starts at image offset `0x4000`.
- Its no-crypto flag is set, the extended header is readable, and its stored SHA-256 matches.
- Main ExeFS superblock and all four populated entries (`.code`, `banner`, `icon`, `logo`) passed SHA-256 verification.
- Main RomFS superblock SHA-256 passed. The complete IVFC block-hash tree was **not** verified.

Image SHA-256:

```text
3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525
```

## Executable

The compressed ExeFS `.code` was successfully BLZ-decompressed to **2,650,112 bytes**.
Its length matches the three page-aligned segments declared by the extended header.
The entry address is **`0x00100000`**.

| Segment | Virtual address | Actual bytes | Page-allocated bytes |
| --- | --- | ---: | ---: |
| Text | `0x00100000` | 2,450,732 | 2,453,504 |
| Read-only data | `0x00357000` | 79,432 | 81,920 |
| Data | `0x0036B000` | 112,240 | 114,688 |

The extended header declares **2,219,880 bytes of BSS** and a **65,536-byte main stack**.
Disassembly of the first startup call identifies a zeroing loop at `0x00100024`.
Its literal bounds are `0x00386670` through `0x005A45D8` (end-exclusive), matching
the declared BSS size. These addresses were read from this executable, not borrowed
from an Ocarina of Time profile.

Decompressed code SHA-256:

```text
5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f
```

The plain NCCH region identifies CTR SDK `4_2_3_200_none` and several Mobiclip components.
These are embedded version markers, not claims that the corresponding SDK is available here.

## Files and asset index

The main RomFS inventory has **51 files** in **2 directories**, counting the root.
It contains 49 `.moflex` cutscene files, plus `lego_city3ds.csv` and `lego_city3ds.fib`.
The cutscene files total **502,707,868 bytes**. The FIB is **259,357,972 bytes** and the
CSV is **1,090,227 bytes**.

The CSV supplies **12,824 asset records** with filename CRC fields, original filenames,
compressed sizes, uncompressed sizes, and offsets. Every indexed compressed range
fits within the supplied FIB container. The listed assets include 3,354 `.btga`,
3,113 `.bfnmdl`, 1,954 `.bfnanm`, 1,361 `.bwav`, and 856 `.blvl` files.

No `.cro`, `.crs`, `.crr`, `.elf`, or `.rel` filenames were found in the visible RomFS
inventory or asset-index extension scan. This is **not** proof that no dynamically
loaded code exists: proprietary payloads and generic `.bin` files were not classified.
FIB asset payloads have not yet been decompressed or independently hash-verified.

## What is ready and what is not

Ready:

- Reassembled, readable, decrypted game image in this working session.
- Hash-verified executable extraction and a bounded, standard-library Python preparation tool.
- A revision fingerprint, segment map, service permission list, and file inventory.
- Seven passing preparation-unit tests, plus successful end-to-end preparation on the supplied image.

Not yet done:

- Whole-program control-flow recovery or generation of translated game functions.
- Integration of a LEGO-specific title module with a 3DS runtime.
- Native game boot, graphics, audio, input, saving, or gameplay validation.
- Windows execution of the source-collection script. It has been reviewed, not run here.

The service names in `profile.json` are the executable's **permission list**, not a
trace of actual runtime calls. The profile format is our inspection manifest;
it is **not** a working TriAevum title recipe.

## Framework handoff

Bulk GitHub download/clone attempts were unsuccessful in this working environment.
`Collect-Framework-Source.cmd` invokes the included PowerShell collector on Windows.
It obtains a fresh TriAevum checkout plus any recursive Git submodules at the revisions
recorded by that checkout, writes its actual commit identity, and packages upload-sized
7-Zip volumes. It does not build or execute downloaded source code.

This source collection does **not** claim to include every SDK, package-manager dependency,
or CMake FetchContent download. Those must be checked against the collected revision.
Your uploaded game image does not need to be uploaded again in this conversation.

## References consulted for the file layouts and framework boundary

- Azahar NCSD/NCCH/ExeFS structures:
  https://raw.githubusercontent.com/azahar-emu/azahar/master/src/core/file_sys/ncch_container.h
- Azahar BLZ/ExeFS handling:
  https://raw.githubusercontent.com/azahar-emu/azahar/master/src/core/file_sys/ncch_container.cpp
- Azahar segment-loading layout:
  https://raw.githubusercontent.com/azahar-emu/azahar/master/src/core/loader/ncch.cpp
- Azahar RomFS metadata layout:
  https://raw.githubusercontent.com/azahar-emu/azahar/master/src/core/file_sys/layered_fs.h
- TriAevum source and title-build contract:
  https://github.com/coccofresco/TriAevum
  https://raw.githubusercontent.com/coccofresco/TriAevum/main/docs/TRIAEVUM_PRECOMPILED_RELEASE.md

All game-specific numbers above come from the supplied image and the generated reports.