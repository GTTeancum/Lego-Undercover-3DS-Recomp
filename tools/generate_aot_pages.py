#!/usr/bin/env python3
"""Generate the reconstructed Recovery-J-style page-oriented A32 source layout.

The original LEGO-specific page emitter was lost. Recovery F/J document 604
unchanged generated AOT files, while the executable text allocation is exactly
599 4-KiB pages and Focus K01 compiles four generated AOT pages. This recovery
emitter therefore uses one C++ translation unit per 4-KiB guest text page and
five deterministic support artifacts.

The five support filenames below are a recovery convention, NOT a claim that
these were the historical names. The page boundaries and source executable are
verified facts; the exact historical filenames remain unknown.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import sys
from typing import Sequence

ROOT = Path(__file__).resolve().parents[1]
VENDOR = ROOT / "vendor" / "triaevum-a9b4477"
TOOLS = ROOT / "tools"

BASE = 0x00100000
TEXT_BYTES = 2_450_732
PAGE_SIZE = 0x1000
TEXT_PAGES = 599
TEXT_ALLOCATED_BYTES = TEXT_PAGES * PAGE_SIZE
EXPECTED_CODE_SHA256 = "5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f"
RECOVERY_J_GENERATED_FILES = 604

GENERATED_HEADER = "lego_aot_generated.h"
REGISTRY_SOURCE = "lego_aot_registry.cpp"
FUNCTIONS_SOURCE = "lego_aot_functions.cpp"
PAGES_METADATA = "lego_aot_pages.json"
MANIFEST_FILE = "manifest.json"


@dataclass(frozen=True)
class RecoveryOp:
    pc: int
    raw: int
    opcode: str
    condition: str
    flags: int
    category: str = ""


@dataclass(frozen=True)
class RecoveryBlock:
    pc: int
    ops: tuple[RecoveryOp, ...]
    native_candidate: bool = False


@dataclass(frozen=True)
class RecoveryFunction:
    entry: int
    end: int
    name: str


@dataclass(frozen=True)
class RecoveryPage:
    index: int
    address: int
    blocks: tuple[RecoveryBlock, ...]

    @property
    def source_name(self) -> str:
        return f"lego_page_{self.address >> 12:05X}.cpp"

    @property
    def symbol(self) -> str:
        return f"kLegoPage{self.address >> 12:05X}Shard"


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def page_index(pc: int) -> int:
    if pc < BASE or pc >= BASE + TEXT_ALLOCATED_BYTES or pc & 3:
        raise ValueError(f"PC outside aligned text allocation: 0x{pc:08X}")
    return (pc - BASE) // PAGE_SIZE


def split_blocks_into_pages(blocks: Sequence[RecoveryBlock]) -> tuple[RecoveryPage, ...]:
    page_blocks: list[list[RecoveryBlock]] = [[] for _ in range(TEXT_PAGES)]
    seen_pcs: set[int] = set()

    for block in sorted(blocks, key=lambda item: item.pc):
        if not block.ops:
            raise ValueError(f"empty block at 0x{block.pc:08X}")
        if block.ops[0].pc != block.pc:
            raise ValueError("block PC does not match first op")
        expected = block.pc
        current_ops: list[RecoveryOp] = []
        current_page: int | None = None
        for op in block.ops:
            if op.pc != expected:
                raise ValueError(f"non-contiguous block at 0x{block.pc:08X}")
            if op.pc in seen_pcs:
                raise ValueError(f"duplicate generated PC 0x{op.pc:08X}")
            seen_pcs.add(op.pc)
            idx = page_index(op.pc)
            if current_page is None:
                current_page = idx
            if idx != current_page:
                assert current_ops
                page_blocks[current_page].append(
                    RecoveryBlock(current_ops[0].pc, tuple(current_ops), block.native_candidate)
                )
                current_ops = []
                current_page = idx
            current_ops.append(op)
            expected += 4
        assert current_page is not None and current_ops
        page_blocks[current_page].append(
            RecoveryBlock(current_ops[0].pc, tuple(current_ops), block.native_candidate)
        )

    pages = []
    for index, items in enumerate(page_blocks):
        items.sort(key=lambda item: item.pc)
        for left, right in zip(items, items[1:]):
            left_end = left.pc + len(left.ops) * 4
            if left_end > right.pc:
                raise ValueError(f"overlapping blocks in page {index}")
        pages.append(RecoveryPage(index, BASE + index * PAGE_SIZE, tuple(items)))
    return tuple(pages)


def _render_op(op: RecoveryOp) -> str:
    return (
        "    {"
        f"0x{op.raw:08X}U, a32::EncodeMetadata(a32::Opcode::{op.opcode}, "
        f"a32::Condition::{op.condition}, 0x{op.flags:02X}U)"
        "},"
    )


def render_header() -> str:
    return """#pragma once

#include "recomp/a32_runtime.h"

namespace oot3d::recomp {

const a32::Registry& GetA32GeneratedRegistry() noexcept;

}  // namespace oot3d::recomp
"""


def render_page(page: RecoveryPage) -> str:
    symbol = f"Page{page.address >> 12:05X}"
    lines = [
        f'#include "{GENERATED_HEADER}"',
        "",
        "namespace oot3d::recomp {",
        "",
    ]
    if page.blocks:
        ops = [op for block in page.blocks for op in block.ops]
        lines.append(f"static constexpr a32::PackedOp kLego{symbol}Ops[] = {{")
        lines.extend(_render_op(op) for op in ops)
        lines.extend(["};", ""])
        lines.append(f"static const a32::Block kLego{symbol}Blocks[] = {{")
        offset = 0
        for block in page.blocks:
            native = ", true" if block.native_candidate else ""
            lines.append(
                f"    {{0x{block.pc:08X}U, kLego{symbol}Ops + {offset}U, "
                f"{len(block.ops)}U{native}}},"
            )
            offset += len(block.ops)
        lines.extend(["};", ""])
        lines.append(
            f"extern const a32::BlockShard {page.symbol} = "
            f"{{0x{page.address:08X}U, 0x{page.address + PAGE_SIZE - 4:08X}U, "
            f"kLego{symbol}Blocks, {len(page.blocks)}U}};"
        )
    else:
        lines.append(
            f"extern const a32::BlockShard {page.symbol} = "
            f"{{0x{page.address:08X}U, 0x{page.address + PAGE_SIZE - 4:08X}U, "
            "nullptr, 0U};"
        )
    lines.extend(["", "}  // namespace oot3d::recomp", ""])
    return "\n".join(lines)


def render_functions(functions: Sequence[RecoveryFunction]) -> str:
    ordered = sorted(functions, key=lambda item: (item.entry, item.end, item.name))
    lines = [
        f'#include "{GENERATED_HEADER}"',
        "",
        "#include <cstdint>",
        "",
        "namespace oot3d::recomp {",
        "",
    ]
    if ordered:
        lines.append("extern const a32::Function kLegoAotFunctions[] = {")
        for function in ordered:
            lines.append(
                f"    {{0x{function.entry:08X}U, 0x{function.end:08X}U, "
                f"{json.dumps(function.name, ensure_ascii=True)}}},"
            )
        lines.extend(["};", ""])
        lines.append(
            f"extern const std::uint32_t kLegoAotFunctionCount = {len(ordered)}U;"
        )
    else:
        lines.append("extern const a32::Function* kLegoAotFunctions = nullptr;")
        lines.append("extern const std::uint32_t kLegoAotFunctionCount = 0U;")
    lines.extend(["", "}  // namespace oot3d::recomp", ""])
    return "\n".join(lines)


def render_registry(pages: Sequence[RecoveryPage], has_functions: bool) -> str:
    lines = [
        f'#include "{GENERATED_HEADER}"',
        "",
        "#include <array>",
        "#include <cstdint>",
        "",
        "namespace oot3d::recomp {",
        "",
    ]
    for page in pages:
        lines.append(f"extern const a32::BlockShard {page.symbol};")
    if has_functions:
        lines.extend([
            "extern const a32::Function kLegoAotFunctions[];",
            "extern const std::uint32_t kLegoAotFunctionCount;",
        ])
    else:
        lines.extend([
            "extern const a32::Function* kLegoAotFunctions;",
            "extern const std::uint32_t kLegoAotFunctionCount;",
        ])
    lines.extend(["", f"static const std::array<a32::BlockShard, {len(pages)}> kLegoPages = {{"])
    lines.extend(f"    {page.symbol}," for page in pages)
    lines.extend([
        "};",
        "",
        "const a32::Registry& GetA32GeneratedRegistry() noexcept {",
        "    static const a32::Registry registry = {",
        "        kLegoPages.data(),",
        "        static_cast<std::uint32_t>(kLegoPages.size()),",
        "        kLegoAotFunctions,",
        "        kLegoAotFunctionCount,",
        "    };",
        "    return registry;",
        "}",
        "",
        "}  // namespace oot3d::recomp",
        "",
    ])
    return "\n".join(lines)


def _write_if_different(path: Path, data: bytes) -> bool:
    try:
        if path.read_bytes() == data:
            return False
    except FileNotFoundError:
        pass
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_name(f".{path.name}.tmp")
    temp.write_bytes(data)
    temp.replace(path)
    return True


def emit_recovery_artifacts(
    output: Path,
    blocks: Sequence[RecoveryBlock],
    functions: Sequence[RecoveryFunction],
    *,
    source_metadata: dict | None = None,
) -> dict:
    pages = split_blocks_into_pages(blocks)
    if len(pages) != TEXT_PAGES:
        raise AssertionError("page count drifted")

    text_contents: dict[str, str] = {
        GENERATED_HEADER: render_header(),
        REGISTRY_SOURCE: render_registry(pages, bool(functions)),
        FUNCTIONS_SOURCE: render_functions(functions),
    }
    for page in pages:
        text_contents[page.source_name] = render_page(page)

    page_metadata = {
        "format": "lego_chase_generated_pages_v1",
        "page_size": PAGE_SIZE,
        "page_count": TEXT_PAGES,
        "text_base": f"0x{BASE:08X}",
        "text_allocated_bytes": TEXT_ALLOCATED_BYTES,
        "pages": [
            {
                "index": page.index,
                "guest_address": f"0x{page.address:08X}",
                "source": page.source_name,
                "blocks": len(page.blocks),
                "ops": sum(len(block.ops) for block in page.blocks),
            }
            for page in pages
        ],
    }
    text_contents[PAGES_METADATA] = json.dumps(page_metadata, indent=2) + "\n"

    byte_contents = {name: value.encode("utf-8") for name, value in text_contents.items()}
    files = [
        {"path": name, "bytes": len(data), "sha256": sha256_bytes(data)}
        for name, data in sorted(byte_contents.items())
    ]
    manifest = {
        "format": "lego_chase_page_aot_reconstruction_v1",
        "historical_filename_identity_claimed": False,
        "recovery_j_documented_generated_file_count": RECOVERY_J_GENERATED_FILES,
        "recovery_layout": {
            "page_translation_units": TEXT_PAGES,
            "support_artifacts_excluding_manifest": 4,
            "artifact_count_including_manifest": TEXT_PAGES + 5,
        },
        "counts": {
            "blocks": len(blocks),
            "ops": sum(len(block.ops) for block in blocks),
            "functions": len(functions),
            "nonempty_pages": sum(bool(page.blocks) for page in pages),
        },
        "source": source_metadata or {},
        "files_excluding_manifest": files,
    }
    if manifest["recovery_layout"]["artifact_count_including_manifest"] != RECOVERY_J_GENERATED_FILES:
        raise AssertionError("recovery artifact count does not match documented 604-file boundary")
    manifest_bytes = (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode("utf-8")

    output.mkdir(parents=True, exist_ok=True)
    expected = set(byte_contents) | {MANIFEST_FILE}
    for stale in output.glob("lego_page_*.cpp"):
        if stale.name not in expected:
            stale.unlink()
    for name, data in byte_contents.items():
        _write_if_different(output / name, data)
    _write_if_different(output / MANIFEST_FILE, manifest_bytes)

    actual = {path.name for path in output.iterdir() if path.is_file() and path.name in expected}
    if len(actual) != RECOVERY_J_GENERATED_FILES or actual != expected:
        raise AssertionError(f"expected exactly {RECOVERY_J_GENERATED_FILES} generated artifacts")
    return manifest


def _convert_blocks(aot, blocks, native_blocks: set[int]) -> list[RecoveryBlock]:
    converted = []
    for block in blocks:
        converted.append(
            RecoveryBlock(
                block.pc,
                tuple(
                    RecoveryOp(op.pc, op.raw, op.opcode, op.condition, op.flags, op.category)
                    for op in block.ops
                ),
                block.pc in native_blocks,
            )
        )
    return converted


def generate_from_verified_code(
    code_path: Path,
    output: Path,
    *,
    inventory_path: Path | None = None,
    boundary_audit_path: Path | None = None,
    native_blocks_path: Path | None = None,
) -> dict:
    if sha256_path(code_path) != EXPECTED_CODE_SHA256:
        raise ValueError("wrong code.bin revision")

    sys.path.insert(0, str(TOOLS))
    sys.path.insert(0, str(VENDOR))
    try:
        import generate_aot as lego_inventory
        from oot3d_pack import a32_cpp_aot as aot
    except ModuleNotFoundError as exc:
        if exc.name == "capstone":
            raise RuntimeError(
                "Capstone 5.0.7 is required for full AOT analysis; page rendering tests do not require it"
            ) from exc
        raise

    code = code_path.read_bytes()
    work = output.parent / "generation-inputs-pages"
    if inventory_path is None or boundary_audit_path is None:
        inventory_path, boundary_audit_path, _ = lego_inventory.write_inputs(work, code)

    functions, callable_functions, slots, callable_slots = aot._load_inputs(
        inventory_path, boundary_audit_path, len(code), BASE
    )
    decoder = aot._Decoder(code, BASE)
    flow_slots = slots | set(range(BASE, BASE + TEXT_ALLOCATED_BYTES, 4))
    flow, literal_data, literal_sources, iterations = aot._address_aware_flow(
        decoder,
        flow_slots,
        (function.entry for function in callable_functions),
        flow_slots,
    )
    emitted = aot._emitted_ops(decoder, flow.decoded_reachable | flow.unknown_stops)
    if set(emitted) & literal_data:
        raise AssertionError("literal data leaked into emitted operations")
    generic_blocks = aot._make_blocks(decoder, emitted, flow.block_starts, 1 << 30)

    native_blocks: set[int] = set()
    if native_blocks_path is not None:
        doc = json.loads(native_blocks_path.read_text(encoding="utf-8"))
        native_blocks = {int(item["pc"]) for item in doc.get("blocks", [])}

    recovery_blocks = _convert_blocks(aot, generic_blocks, native_blocks)
    recovery_functions = [
        RecoveryFunction(item.entry, item.end, item.name) for item in callable_functions
    ]
    source = {
        "code_sha256": EXPECTED_CODE_SHA256,
        "inventory": str(inventory_path),
        "boundary_audit": str(boundary_audit_path),
        "fixed_point_iterations": iterations,
        "literal_data_slots": len(literal_data),
        "literal_sources": len(literal_sources),
        "pointer_roots": len(flow.pointer_roots),
        "unknown_cfg_stops": len(flow.unknown_stops),
        "frontend_generated_ops": len(emitted),
        "frontend_blocks_before_page_split": len(generic_blocks),
    }
    return emit_recovery_artifacts(output, recovery_blocks, recovery_functions, source_metadata=source)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("code", type=Path, help="verified prepared code.bin")
    parser.add_argument("output", type=Path)
    parser.add_argument("--inventory", type=Path)
    parser.add_argument("--boundary-audit", type=Path)
    parser.add_argument("--native-blocks", type=Path)
    args = parser.parse_args()
    if (args.inventory is None) != (args.boundary_audit is None):
        parser.error("--inventory and --boundary-audit must be supplied together")
    manifest = generate_from_verified_code(
        args.code,
        args.output,
        inventory_path=args.inventory,
        boundary_audit_path=args.boundary_audit,
        native_blocks_path=args.native_blocks,
    )
    print(
        f"Generated {manifest['recovery_layout']['artifact_count_including_manifest']} "
        f"recovery artifacts across {TEXT_PAGES} guest pages"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
