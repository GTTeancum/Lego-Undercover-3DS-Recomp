#!/usr/bin/env python3
"""Generate LEGO Chase A32 C++ shards with the pinned TriAevum frontend.

The original LEGO-specific wrapper was lost. This reconstruction deliberately
uses the exact A32 frontend/runtime sources from the recorded TriAevum commit
and a minimal title-specific inventory rooted at the real process entry point.
It does not claim byte-identical output to the lost Recovery J generator until
the documented generation counts are reproduced.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VENDOR = ROOT / "vendor" / "triaevum-a9b4477"
EXPECTED_CODE_SHA256 = "5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f"
BASE = 0x00100000
TEXT_BYTES = 2_450_732
TEXT_PAGES = 0x257
TEXT_ALLOCATED_BYTES = TEXT_PAGES * 0x1000
DEFAULT_SHARD_SIZE = 0x4000
HISTORICAL_BLOCKS = 111_312
HISTORICAL_INSTRUCTION_SLOTS = 547_756
HISTORICAL_EXPLICIT_TRAPS = 41
HISTORICAL_PENDING_ROOTS = 0
INITIALIZER_DISPATCHER = 0x001336CC
INITIALIZER_TABLE_BEGIN = 0x0036A1A8
INITIALIZER_TABLE_END = 0x0036A648
INITIALIZER_COUNT = 296
OBSERVED_CALLBACK_ROOTS = (0x002DEB78, 0x002FAC60, 0x003261EC)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def initializer_roots(code: bytes) -> list[int]:
    roots: list[int] = []
    for slot in range(INITIALIZER_TABLE_BEGIN, INITIALIZER_TABLE_END, 4):
        offset = slot - BASE
        if offset < 0 or offset + 4 > len(code):
            raise ValueError("initializer table lies outside code.bin")
        raw = struct.unpack_from("<I", code, offset)[0]
        displacement = raw - 0x1_0000_0000 if raw & 0x8000_0000 else raw
        target = (slot + displacement) & 0xFFFF_FFFF
        if target % 4 or not (BASE <= target < BASE + TEXT_BYTES):
            raise ValueError(
                f"initializer slot 0x{slot:08X} resolves outside text: 0x{target:08X}"
            )
        roots.append(target)
    if len(roots) != INITIALIZER_COUNT:
        raise ValueError(f"expected {INITIALIZER_COUNT} initializer roots, got {len(roots)}")
    return roots


def write_inputs(work: Path, code: bytes) -> tuple[Path, Path, list[int]]:
    work.mkdir(parents=True, exist_ok=True)
    init_roots = initializer_roots(code)
    roots = [BASE, *init_roots, *OBSERVED_CALLBACK_ROOTS]
    unique_roots = list(dict.fromkeys(roots))
    inventory = work / "lego_function_inventory.csv"
    with inventory.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["entry", "size", "name"])
        writer.writeheader()
        writer.writerow({
            "entry": f"0x{BASE:08X}",
            "size": str(TEXT_BYTES),
            "name": "LEGOCITY_text",
        })
        for index, entry in enumerate(unique_roots[1:]):
            writer.writerow({
                "entry": f"0x{entry:08X}",
                "size": "4",
                "name": (
                    f"initializer_{index:03d}"
                    if entry in init_roots
                    else f"observed_callback_{entry:08X}"
                ),
            })
    audit = work / "lego_boundary_audit.csv"
    audit.write_text("entry\n", encoding="utf-8")
    return inventory, audit, init_roots


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("code", type=Path, help="verified prepared code.bin")
    parser.add_argument("output", type=Path, help="generated source directory")
    parser.add_argument("--shard-size", type=lambda x: int(x, 0), default=DEFAULT_SHARD_SIZE)
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()

    if not VENDOR.is_dir():
        parser.error(f"pinned frontend missing: {VENDOR}")
    code_hash = sha256(args.code)
    if code_hash != EXPECTED_CODE_SHA256:
        parser.error(f"wrong code.bin revision: {code_hash}")

    sys.path.insert(0, str(VENDOR))
    try:
        from oot3d_pack import a32_cpp_aot
    except ModuleNotFoundError as exc:
        if exc.name == "capstone":
            parser.error("Capstone 5.0.7 is required (python -m pip install -r requirements-dev.txt)")
        raise

    code_bytes = args.code.read_bytes()
    work = args.output.parent / "generation-inputs"
    inventory, audit, init_roots = write_inputs(work, code_bytes)
    result = a32_cpp_aot.generate(
        code_path=args.code,
        inventory_path=inventory,
        boundary_audit_path=audit,
        native_blocks_path=None,
        output=args.output,
        base=BASE,
        executable_size=TEXT_ALLOCATED_BYTES,
        expected_sha256=EXPECTED_CODE_SHA256,
        shard_size=args.shard_size,
    )

    counts = result.manifest.get("counts", {})
    current_blocks = int(counts.get("blocks", -1))
    current_slots = int(counts.get("generated_ops", -1))
    summary = {
        "format": "lego_chase_aot_reconstruction_v1",
        "code_sha256": code_hash,
        "shard_size": args.shard_size,
        "recovered_roots": {
            "initializer_dispatcher": f"0x{INITIALIZER_DISPATCHER:08X}",
            "initializer_table_begin": f"0x{INITIALIZER_TABLE_BEGIN:08X}",
            "initializer_table_end": f"0x{INITIALIZER_TABLE_END:08X}",
            "initializer_count": len(init_roots),
            "observed_callback_roots": [f"0x{x:08X}" for x in OBSERVED_CALLBACK_ROOTS],
        },
        "current": {
            "blocks": current_blocks,
            "instruction_slots": current_slots,
            "shards": counts.get("shards"),
            "unknown_cfg_stops": counts.get("unknown_cfg_stops"),
            "pointer_roots": counts.get("pointer_roots"),
        },
        "historical_checkpoint": {
            "blocks": HISTORICAL_BLOCKS,
            "instruction_slots": HISTORICAL_INSTRUCTION_SLOTS,
            "explicit_traps": HISTORICAL_EXPLICIT_TRAPS,
            "pending_roots": HISTORICAL_PENDING_ROOTS,
        },
        "matches_historical_block_count": current_blocks == HISTORICAL_BLOCKS,
        "matches_historical_instruction_slots": current_slots == HISTORICAL_INSTRUCTION_SLOTS,
        "note": (
            "Historical counts came from the surviving adr-coverage-build2.log. "
            "A mismatch is expected until the lost title-specific function/root inventory is reconstructed."
        ),
    }
    summary_path = args.output / "lego_reconstruction_summary.json"
    summary_path.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
