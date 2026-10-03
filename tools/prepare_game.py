#!/usr/bin/env python3
"""Prepare the verified USA LEGO City Undercover: The Chase Begins image.

This tool accepts an already-decrypted .cci/.3ds NCSD image, verifies the exact
revision used by the original recomp project, and extracts ExHeader and the
BLZ-decompressed ExeFS .code. RomFS extraction is optional and remains local.

No keys or decryption code are included.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import BinaryIO

MEDIA_UNIT = 0x200
EXPECTED_CCI_SIZE = 1_073_741_824
EXPECTED_CCI_SHA256 = "3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525"
EXPECTED_CODE_SIZE = 2_650_112
EXPECTED_CODE_SHA256 = "5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f"
EXPECTED_PROGRAM_ID = 0x00040000000AD500
EXPECTED_PRODUCT_CODE = "CTR-P-AA8E"
EXPECTED_PROCESS_NAME = "LEGOCITY"
EXPECTED_TRIAEVUM_COMMIT = "a9b447709d4405848d75352354891059cebb9ff8"


class PreparationError(RuntimeError):
    pass


@dataclass(frozen=True)
class Layout:
    partition_index: int
    partition_base: int
    partition_end: int
    program_id: int
    product_code: str
    process_name: str
    compressed_code: bool
    exheader_offset: int
    code_offset: int
    code_size: int
    romfs_offset: int
    romfs_size: int


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def u64(data: bytes, offset: int) -> int:
    return struct.unpack_from("<Q", data, offset)[0]


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(4 * 1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_exact(stream: BinaryIO, offset: int, size: int, label: str) -> bytes:
    stream.seek(offset)
    data = stream.read(size)
    if len(data) != size:
        raise PreparationError(f"truncated while reading {label}")
    return data


def checked_region(offset: int, size: int, lower: int, upper: int, label: str) -> None:
    if offset < lower or size <= 0 or offset > upper or size > upper - offset:
        raise PreparationError(f"{label} is outside the selected NCCH partition")


def parse_ncch(stream: BinaryIO, file_size: int, base: int, size: int, index: int) -> Layout:
    end = base + size
    checked_region(base, 0x200, 0, file_size, "NCCH header")
    if end > file_size:
        raise PreparationError("NCCH partition exceeds image")
    header = read_exact(stream, base, 0x200, "NCCH header")
    if header[0x100:0x104] != b"NCCH":
        raise PreparationError("partition is not NCCH")
    if not (header[0x18D] & 0x02):
        raise PreparationError("partition is not executable")
    if u32(header, 0x180) == 0:
        raise PreparationError("partition has no ExHeader")

    exheader_offset = base + 0x200
    exheader = read_exact(stream, exheader_offset, 0x800, "ExHeader")
    process_name = exheader[:8].split(b"\0", 1)[0].decode("ascii", "replace")
    product_code = header[0x150:0x160].split(b"\0", 1)[0].decode("ascii", "replace")

    exefs_offset = base + u32(header, 0x1A0) * MEDIA_UNIT
    exefs_size = u32(header, 0x1A4) * MEDIA_UNIT
    checked_region(exefs_offset, exefs_size, base, end, "ExeFS")
    exefs_header = read_exact(stream, exefs_offset, 0x200, "ExeFS header")
    code_offset = None
    code_size = 0
    for slot in range(8):
        pos = slot * 0x10
        name = exefs_header[pos:pos + 8].split(b"\0", 1)[0]
        if name == b".code":
            code_offset = exefs_offset + 0x200 + u32(exefs_header, pos + 8)
            code_size = u32(exefs_header, pos + 12)
            checked_region(code_offset, code_size, exefs_offset + 0x200, exefs_offset + exefs_size, ".code")
            break
    if code_offset is None:
        raise PreparationError("ExeFS has no .code section")

    romfs_offset = base + u32(header, 0x1B0) * MEDIA_UNIT
    romfs_size = u32(header, 0x1B4) * MEDIA_UNIT
    checked_region(romfs_offset, romfs_size, base, end, "RomFS")
    if read_exact(stream, romfs_offset, 4, "RomFS magic") != b"IVFC":
        raise PreparationError("RomFS is not a readable decrypted IVFC image")

    return Layout(
        partition_index=index,
        partition_base=base,
        partition_end=end,
        program_id=u64(header, 0x118),
        product_code=product_code,
        process_name=process_name,
        compressed_code=bool(exheader[0x0D] & 1),
        exheader_offset=exheader_offset,
        code_offset=code_offset,
        code_size=code_size,
        romfs_offset=romfs_offset,
        romfs_size=romfs_size,
    )


def find_application(stream: BinaryIO, file_size: int) -> Layout:
    header = read_exact(stream, 0, 0x200, "NCSD header")
    if header[0x100:0x104] != b"NCSD":
        raise PreparationError("input is not an NCSD cartridge image")
    failures = []
    for index in range(8):
        pos = 0x120 + index * 8
        base = u32(header, pos) * MEDIA_UNIT
        size = u32(header, pos + 4) * MEDIA_UNIT
        if not base or not size:
            continue
        try:
            return parse_ncch(stream, file_size, base, size, index)
        except PreparationError as exc:
            failures.append(str(exc))
    raise PreparationError("no readable executable NCCH partition: " + "; ".join(failures))


def decompress_exefs_code(compressed: bytes) -> bytes:
    if len(compressed) < 8:
        raise PreparationError("compressed .code is too small")
    top_bottom = u32(compressed, len(compressed) - 8)
    additional = u32(compressed, len(compressed) - 4)
    output_size = len(compressed) + additional
    footer_size = (top_bottom >> 24) & 0xFF
    encoded_size = top_bottom & 0xFFFFFF
    if footer_size < 8 or footer_size > len(compressed) or encoded_size < footer_size or encoded_size > len(compressed):
        raise PreparationError("compressed .code footer is invalid")
    index = len(compressed) - footer_size
    stop = len(compressed) - encoded_size
    out_index = output_size
    out = bytearray(output_size)
    out[:len(compressed)] = compressed
    while index > stop:
        index -= 1
        control = compressed[index]
        for _ in range(8):
            if index <= stop or out_index == 0:
                break
            if control & 0x80:
                if index < 2:
                    raise PreparationError("truncated .code back-reference")
                index -= 2
                segment = compressed[index] | (compressed[index + 1] << 8)
                count = ((segment >> 12) & 0xF) + 3
                distance = (segment & 0xFFF) + 2
                if out_index < count:
                    raise PreparationError(".code decompression underflow")
                for _ in range(count):
                    source = out_index + distance
                    if source >= len(out):
                        raise PreparationError(".code back-reference outside output")
                    out_index -= 1
                    out[out_index] = out[source]
            else:
                if index <= stop or out_index == 0:
                    raise PreparationError("truncated .code literal")
                index -= 1
                out_index -= 1
                out[out_index] = compressed[index]
            control = (control << 1) & 0xFF
    return bytes(out)


def service_access(exheader: bytes) -> list[str]:
    result = []
    for pos in range(0x250, 0x330, 8):
        raw = exheader[pos:pos + 8].split(b"\0", 1)[0]
        if not raw:
            continue
        try:
            value = raw.decode("ascii")
        except UnicodeDecodeError:
            continue
        if all(0x20 <= byte < 0x7F for byte in raw):
            result.append(value)
    return result


def copy_region(stream: BinaryIO, offset: int, size: int, path: Path) -> str:
    digest = hashlib.sha256()
    stream.seek(offset)
    remaining = size
    with path.open("wb") as out:
        while remaining:
            chunk = stream.read(min(remaining, 4 * 1024 * 1024))
            if not chunk:
                raise PreparationError("image ended while copying RomFS")
            out.write(chunk)
            digest.update(chunk)
            remaining -= len(chunk)
    return digest.hexdigest()


def prepare(image: Path, output: Path, extract_romfs: bool) -> dict:
    image = image.resolve()
    if image.stat().st_size != EXPECTED_CCI_SIZE:
        raise PreparationError(f"unexpected image size: {image.stat().st_size}")
    image_sha = sha256_file(image)
    if image_sha != EXPECTED_CCI_SHA256:
        raise PreparationError(f"wrong game revision: SHA-256 {image_sha}")

    if output.exists():
        if any(output.iterdir()):
            raise PreparationError(f"output directory is not empty: {output}")
    else:
        output.mkdir(parents=True)

    with image.open("rb") as stream:
        layout = find_application(stream, image.stat().st_size)
        if layout.program_id != EXPECTED_PROGRAM_ID:
            raise PreparationError(f"wrong program ID: {layout.program_id:016X}")
        if layout.product_code != EXPECTED_PRODUCT_CODE or layout.process_name != EXPECTED_PROCESS_NAME:
            raise PreparationError("product/process identity does not match the supported revision")
        exheader = read_exact(stream, layout.exheader_offset, 0x800, "ExHeader")
        compressed = read_exact(stream, layout.code_offset, layout.code_size, ".code")
        code = decompress_exefs_code(compressed) if layout.compressed_code else compressed
        code_sha = hashlib.sha256(code).hexdigest()
        if len(code) != EXPECTED_CODE_SIZE or code_sha != EXPECTED_CODE_SHA256:
            raise PreparationError(f"decompressed executable mismatch: {len(code)} bytes, {code_sha}")
        (output / "exheader.bin").write_bytes(exheader)
        (output / "code.bin").write_bytes(code)
        romfs_sha = None
        if extract_romfs:
            romfs_sha = copy_region(stream, layout.romfs_offset, layout.romfs_size, output / "romfs.bin")

    profile = {
        "format": "lego_chase_prepared_v1",
        "source_revision": {
            "cci_bytes": EXPECTED_CCI_SIZE,
            "cci_sha256": image_sha,
            "program_id": f"{layout.program_id:016X}",
            "product_code": layout.product_code,
            "process_name": layout.process_name,
            "partition_index": layout.partition_index,
        },
        "framework": {"triaevum_commit": EXPECTED_TRIAEVUM_COMMIT},
        "executable": {
            "bytes": len(code),
            "sha256": code_sha,
            "entry": "0x00100000",
            "text": {"address": "0x00100000", "bytes": 2450732, "pages": 599},
            "rodata": {"address": "0x00357000", "bytes": 79432, "pages": 20},
            "data": {"address": "0x0036B000", "bytes": 112240, "pages": 28},
            "bss_bytes": 2219880,
            "stack_bytes": 65536,
        },
        "services": service_access(exheader),
        "romfs": {"bytes": layout.romfs_size, "sha256": romfs_sha, "extracted": extract_romfs},
    }
    (output / "profile.json").write_text(json.dumps(profile, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return profile


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path, help="decrypted USA .cci/.3ds image")
    parser.add_argument("output", type=Path, help="new/empty local preparation directory")
    parser.add_argument("--romfs", action="store_true", help="also copy raw RomFS (large)")
    args = parser.parse_args()
    try:
        profile = prepare(args.image, args.output, args.romfs)
    except (OSError, PreparationError) as exc:
        parser.error(str(exc))
    print(f"Verified {profile['source_revision']['product_code']} / {profile['source_revision']['program_id']}")
    print(f"code.bin {profile['executable']['bytes']} bytes {profile['executable']['sha256']}")
    print(f"Prepared: {args.output.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
