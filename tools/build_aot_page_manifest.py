#!/usr/bin/env python3
"""Build a deterministic 4-KiB AOT page manifest for LEGO Chase recovery.

Recovery F/J document 604 generated AOT files and Focus K01 explicitly compiles
four generated AOT pages. The executable text allocation is 599 4-KiB pages.
This tool records those 599 guest pages without guessing the lost historical
source filenames or the identities of the five additional non-page artifacts.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

BASE = 0x00100000
TEXT_BYTES = 2_450_732
PAGE_SIZE = 0x1000
TEXT_PAGES = 599
TEXT_ALLOCATED_BYTES = TEXT_PAGES * PAGE_SIZE
EXPECTED_CODE_BYTES = 2_650_112
EXPECTED_CODE_SHA256 = "5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f"
RECOVERY_J_GENERATED_FILES = 604
K01_ADDRESSES = (0x001C1D78, 0x00227300, 0x0023F168, 0x002CA7BC)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def page_start(address: int) -> int:
    if not (BASE <= address < BASE + TEXT_ALLOCATED_BYTES):
        raise ValueError(f"address outside text allocation: 0x{address:08X}")
    return address & ~(PAGE_SIZE - 1)


def build_manifest(code: bytes) -> dict:
    if len(code) != EXPECTED_CODE_BYTES:
        raise ValueError(f"code.bin size mismatch: {len(code)}")
    digest = sha256(code)
    if digest != EXPECTED_CODE_SHA256:
        raise ValueError(f"code.bin SHA-256 mismatch: {digest}")

    pages = []
    for index in range(TEXT_PAGES):
        offset = index * PAGE_SIZE
        address = BASE + offset
        data = code[offset : offset + PAGE_SIZE]
        if len(data) != PAGE_SIZE:
            raise ValueError("truncated text page")
        remaining = max(0, TEXT_BYTES - offset)
        declared = min(PAGE_SIZE, remaining)
        pages.append(
            {
                "index": index,
                "recovery_id": f"page_{address >> 12:05X}",
                "guest_address": f"0x{address:08X}",
                "file_offset": offset,
                "allocated_bytes": PAGE_SIZE,
                "declared_text_bytes": declared,
                "page_sha256": sha256(data),
            }
        )

    k01 = []
    for address in K01_ADDRESSES:
        start = page_start(address)
        index = (start - BASE) // PAGE_SIZE
        k01.append(
            {
                "observed_address": f"0x{address:08X}",
                "page_index": index,
                "page_guest_address": f"0x{start:08X}",
                "recovery_id": pages[index]["recovery_id"],
            }
        )

    return {
        "format": "lego_chase_aot_page_manifest_v1",
        "source": {"code_bytes": len(code), "code_sha256": digest},
        "layout": {
            "text_base": f"0x{BASE:08X}",
            "declared_text_bytes": TEXT_BYTES,
            "allocated_text_bytes": TEXT_ALLOCATED_BYTES,
            "page_size": PAGE_SIZE,
            "page_count": TEXT_PAGES,
        },
        "recovery_j_evidence": {
            "documented_generated_aot_files": RECOVERY_J_GENERATED_FILES,
            "documented_text_page_count": TEXT_PAGES,
            "generated_file_count_minus_text_pages": RECOVERY_J_GENERATED_FILES - TEXT_PAGES,
            "non_page_artifact_names_recovered": False,
            "historical_page_filename_scheme_recovered": False,
        },
        "focus_k01_pages": k01,
        "pages": pages,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("code", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    manifest = build_manifest(args.code.read_bytes())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(
        f"AOT pages: {manifest['layout']['page_count']} x "
        f"{manifest['layout']['page_size']} bytes"
    )
    print("K01:", ", ".join(item["recovery_id"] for item in manifest["focus_k01_pages"]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
