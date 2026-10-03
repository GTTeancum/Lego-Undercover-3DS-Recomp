# Recovery J AOT page-layout checkpoint

This checkpoint reconstructs the page boundaries used as the target for the later
Recovery F/J source tree without inventing the lost generated C++ contents.

Verified against the exact recovered `code.bin`:

- Text base: `0x00100000`
- Declared text bytes: **2,450,732**
- Allocated text: **599 × 4,096-byte pages**
- First page: `0x00100000`
- Last page: `0x00356000`
- Declared bytes in the last page: **1,324**
- Recovery J documented generated AOT files: **604**
- Difference between documented generated-file count and text-page count: **5**
- The identities of those five non-page generated artifacts are **not yet recovered**.
- The original generated-page filename scheme is **not yet recovered**.

Focus K01's four observed code addresses map to four distinct pages:

| Observed address | Page |
|---|---|
| `0x001C1D78` | `0x001C1000` |
| `0x00227300` | `0x00227000` |
| `0x0023F168` | `0x0023F000` |
| `0x002CA7BC` | `0x002CA000` |

`tools/build_aot_page_manifest.py` hashes all 599 pages from a locally prepared,
verified `code.bin`. The page hashes are intentionally generated locally rather
than committing proprietary executable data.

This page manifest is now the structural target for reconstructing the later
LEGO-specific page emitter. The earlier 135-file adr-coverage build remains useful
control-flow evidence but is not treated as the final Recovery J output layout.
