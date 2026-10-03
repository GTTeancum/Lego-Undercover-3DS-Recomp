#!/usr/bin/env python3
"""Build the LEGO Chase native process layout from prepared ExHeader/code.bin."""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

PAGE_SIZE = 0x1000
HEAP_VADDR_END = 0x10000000
TLS_AREA_VADDR = 0x1FF82000
TLS_PAGE_SIZE = 0x1000
CONFIG_MEMORY_VADDR = 0x1FF80000
SHARED_PAGE_VADDR = 0x1FF81000
DSP_RAM_VADDR = 0x1FF00000
DSP_RAM_SIZE = 0x00080000
VRAM_VADDR = 0x1F000000
VRAM_SIZE = 0x00600000
LEGACY_LINEAR_HEAP_VADDR = 0x14000000
HEAP_VADDR = 0x08000000
HEAP_SIZE = 0x08000000
USER32_CPSR = 0x10
CTR_MAIN_FPSCR = 0x03C00010
EXPECTED_CODE_SHA256 = "5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f"
EXPECTED_INITIAL_COMMIT = 4_935_680

# APPLICATION, SYSTEM, BASE region sizes indexed by ExHeader memory mode.
CTR_MEMORY_REGION_SIZES = (
    (0x04000000, 0x02C00000, 0x01400000),
    (0, 0, 0),
    (0x06000000, 0x00C00000, 0x01400000),
    (0x05000000, 0x01C00000, 0x01400000),
    (0x04800000, 0x02400000, 0x01400000),
    (0x02000000, 0x04C00000, 0x01400000),
    (0x07C00000, 0x06400000, 0x02000000),
    (0x0B200000, 0x02E00000, 0x02000000),
)


def u32(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 4 > len(data):
        raise ValueError(f"u32 at {offset:#x} is outside ExHeader")
    return struct.unpack_from("<I", data, offset)[0]


def align_up(value: int, alignment: int) -> int:
    return (value + alignment - 1) & ~(alignment - 1)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def segment(exheader: bytes, offset: int) -> dict[str, int]:
    return {
        "address": u32(exheader, offset),
        "physical_pages": u32(exheader, offset + 4),
        "code_size": u32(exheader, offset + 8),
    }


def validate_segment(name: str, item: dict[str, int]) -> None:
    if item["address"] % PAGE_SIZE:
        raise ValueError(f"{name} address is not page aligned")
    if item["physical_pages"] == 0:
        raise ValueError(f"{name} has no physical pages")
    if item["code_size"] > item["physical_pages"] * PAGE_SIZE:
        raise ValueError(f"{name} code size exceeds mapped pages")


def initial_value(offset: int, size: int, value: int) -> dict[str, int]:
    return {"offset": offset, "size": size, "value": value}


def ctr_memory_layout(exheader: bytes) -> tuple[int, int, int, int]:
    mode = exheader[0x20D] & 0x0F
    if mode >= len(CTR_MEMORY_REGION_SIZES):
        raise ValueError(f"unsupported CTR memory mode {mode}")
    app, system, base = CTR_MEMORY_REGION_SIZES[mode]
    if app == 0:
        raise ValueError(f"unused CTR memory mode {mode}")
    return mode, app, system, base


def ctr_system_regions(exheader: bytes) -> list[dict[str, object]]:
    mode, app, system, base = ctr_memory_layout(exheader)
    config_values = [
        initial_value(0x01, 1, 0x3A),
        initial_value(0x03, 1, 0x02),
        initial_value(0x08, 8, 0x0004013000008002),
        initial_value(0x10, 4, 0x02),
        initial_value(0x14, 1, 0x01),
        initial_value(0x16, 1, 0x01),
        initial_value(0x18, 4, 0x0000F450),
        initial_value(0x30, 4, mode),
        initial_value(0x40, 4, app),
        initial_value(0x44, 4, system),
        initial_value(0x48, 4, base),
        initial_value(0x62, 1, 0x3A),
        initial_value(0x63, 1, 0x02),
        initial_value(0x64, 4, 0x02),
        initial_value(0x68, 4, 0x0000F450),
    ]
    shared_values = [
        initial_value(0x04, 1, 0x01),
        initial_value(0x66, 1, 0x03),
        initial_value(0x67, 1, 0x02),
        initial_value(0x85, 1, 0x17),
        initial_value(0x86, 1, 0x01),
    ]
    return [
        {"name": "ctr_vram", "address": VRAM_VADDR, "mapped_size": VRAM_SIZE,
         "writable": True, "executable": False, "initial_values": []},
        {"name": "ctr_dsp_ram", "address": DSP_RAM_VADDR, "mapped_size": DSP_RAM_SIZE,
         "writable": True, "executable": False, "initial_values": []},
        {"name": "ctr_config_memory", "address": CONFIG_MEMORY_VADDR, "mapped_size": PAGE_SIZE,
         "writable": False, "executable": False, "initial_values": config_values},
        {"name": "ctr_shared_page", "address": SHARED_PAGE_VADDR, "mapped_size": PAGE_SIZE,
         "writable": False, "executable": False, "initial_values": shared_values},
    ]


def build_manifest(exheader: bytes, code: bytes) -> dict[str, object]:
    if len(exheader) < 0x400:
        raise ValueError("ExHeader is truncated")
    code_hash = sha256(code)
    if code_hash != EXPECTED_CODE_SHA256:
        raise ValueError(f"code.bin SHA-256 mismatch: {code_hash}")

    text = segment(exheader, 0x10)
    ro = segment(exheader, 0x20)
    data = segment(exheader, 0x30)
    for name, item in (("text", text), ("rodata", ro), ("data", data)):
        validate_segment(name, item)

    text_pages = text["physical_pages"] * PAGE_SIZE
    ro_pages = ro["physical_pages"] * PAGE_SIZE
    data_pages = data["physical_pages"] * PAGE_SIZE
    initialized_size = text_pages + ro_pages + data_pages
    if len(code) != initialized_size:
        raise ValueError(
            f"code.bin {len(code):#x} does not match ExHeader page total {initialized_size:#x}"
        )
    if text["address"] + text_pages > ro["address"]:
        raise ValueError("text and rodata ranges overlap")
    if ro["address"] + ro_pages > data["address"]:
        raise ValueError("rodata and data ranges overlap")

    stack_size = u32(exheader, 0x1C)
    bss_size = u32(exheader, 0x3C)
    if stack_size == 0 or stack_size % PAGE_SIZE:
        raise ValueError("main stack size is zero or not page aligned")
    bss_pages = align_up(bss_size, PAGE_SIZE)
    data_mapped_size = data_pages + bss_pages
    stack_base = HEAP_VADDR_END - stack_size
    if data["address"] + data_mapped_size >= stack_base:
        raise ValueError("process image collides with main stack")

    segments = [
        {
            "name": "text", "address": text["address"],
            "mapped_size": text_pages, "declared_code_size": text["code_size"],
            "file_offset": 0, "file_size": text_pages,
            "writable": False, "executable": True,
        },
        {
            "name": "rodata", "address": ro["address"],
            "mapped_size": ro_pages, "declared_code_size": ro["code_size"],
            "file_offset": text_pages, "file_size": ro_pages,
            "writable": False, "executable": False,
        },
        {
            "name": "data_bss", "address": data["address"],
            "mapped_size": data_mapped_size, "declared_code_size": data["code_size"],
            "file_offset": text_pages + ro_pages, "file_size": data_pages,
            "writable": True, "executable": False,
        },
    ]

    mode, app_memory, system_memory, base_memory = ctr_memory_layout(exheader)
    initial_commit = sum(int(item["mapped_size"]) for item in segments) + stack_size
    if initial_commit != EXPECTED_INITIAL_COMMIT:
        raise ValueError(
            f"initial commit drifted: expected {EXPECTED_INITIAL_COMMIT}, got {initial_commit}"
        )

    return {
        "format": "lego_chase_native_process_manifest_v1",
        "source": {
            "exheader_sha256": sha256(exheader),
            "code_bin_sha256": code_hash,
            "code_bin_size": len(code),
            "layout_authority": "CTR ExHeader + recovered Stage 2 invariants",
        },
        "process": {
            "name": exheader[:8].split(b"\0", 1)[0].decode("ascii", "strict"),
            "entrypoint": text["address"],
            "page_size": PAGE_SIZE,
            "memory_mode": mode,
            "application_memory_size": app_memory,
            "system_memory_size": system_memory,
            "base_memory_size": base_memory,
            "bss_declared_size": bss_size,
            "bss_mapped_size": bss_pages,
            "initial_commit_bytes": initial_commit,
            "segments": segments,
            "system_regions": ctr_system_regions(exheader),
            "heap_address_space": {"base_address": HEAP_VADDR, "size": HEAP_SIZE},
            "linear_heap_address_space": {
                "base_address": LEGACY_LINEAR_HEAP_VADDR,
                "size": app_memory,
            },
            "stage2_observed_allocations": {
                "normal_heap": {"base_address": 0x08000000, "size": 0x0124B000},
                "linear_heap": {"base_address": 0x14000000, "size": 0x02900000},
                "combined_committed_bytes": 67_108_864,
            },
        },
        "primary_thread": {
            "stack_base_address": stack_base,
            "stack_size": stack_size,
            "tls_base_address": TLS_AREA_VADDR,
            "tls_size": TLS_PAGE_SIZE,
            "thread_pointer": TLS_AREA_VADDR,
            "argument0": 0,
            "initial_cpsr": USER32_CPSR,
            "initial_fpscr": CTR_MAIN_FPSCR,
            "priority": exheader[0x20F],
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("prepared", type=Path, help="directory containing exheader.bin and code.bin")
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    manifest = build_manifest(
        (args.prepared / "exheader.bin").read_bytes(),
        (args.prepared / "code.bin").read_bytes(),
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(
        f"LEGO process manifest: entry={manifest['process']['entrypoint']:#010x} "
        f"initial_commit={manifest['process']['initial_commit_bytes']} output={args.output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
