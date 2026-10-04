# Verified private game-file persistence checkpoint

Verified October 4, 2026 (UTC). This is an asset-preservation checkpoint, not a new game-build or gameplay result.

## Durable storage

The six original split game-archive parts are saved in the user's persistent ChatGPT Library under `/LEGO-Chase-Recovery/Game-archive/`. Each saved part was independently materialized back into scratch and matched to the original upload by SHA-256. Their combined compressed size is 558,084,524 bytes.

The Library also retains:

- `/LEGO-Chase-Recovery/code.bin`
- `/LEGO-Chase-Recovery/LEGO-Chase-current-AOT-599pages-2026-10-03.tgz`
- `/LEGO-Chase-Recovery/LEGO-Chase-real-run-source-2026-10-03.tgz`

These existing backups were retrieved successfully, hashed, and restored this turn. The source archive is an earlier checkpoint, not proof that later uncommitted edits survived.

## Scratch working copies

- Original archive parts and joined archive: `/mnt/data/lego_recovery/game/archive/`
- Extracted CCI: `/mnt/data/lego_recovery/game/image/LEGO City Undercover - The Chase Begins (USA) (En,Fr,Es).cci`
- Prepared executable: `/mnt/data/lego_recovery/game/prepared/code.bin`
- Compatibility symlink: `/mnt/data/lego_recovery/code.bin`
- Restored AOT source: `/mnt/data/lego_recovery/generated2/`
- Restored earlier source checkpoint: `/mnt/data/lego_recovery/source-recovered-2026-10-03/`
- Private manifest, file IDs, restoration helpers and verification logs: `/mnt/data/lego_recovery/checkpoint/`

The archive was extracted again using libarchive. The extracted CCI is 1,073,741,824 bytes and matches the original supported revision.

## Verified hashes

- CCI SHA-256: `3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525`
- code.bin SHA-256: `5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f`
- Saved AOT archive SHA-256: `2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9`
- Saved source archive SHA-256: `a5e882ea917c8af50586c8f04f5fa7bbd5ed93c941e009e32783d407a3da6d04`

The restored AOT archive has 599 page C++ files and 603 regular files overall. Its tar member count is 604 including the directory; that is not a verified count of 604 regular generated artifacts. No new instruction-coverage or gameplay-equivalence claim is made.

## Recovery rule

Scratch is a working copy and may reset; Library snapshots are the persistent recovery source. After a reset, inspect and restore the Library copies before requesting another upload. Do not publish game-image bytes, code.bin or game-derived generated pages to this public repository. Keep private snapshot identifiers in the Library recovery manifest rather than public Git.
