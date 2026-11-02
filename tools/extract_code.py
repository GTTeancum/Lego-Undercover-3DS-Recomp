#!/usr/bin/env python3
"""Extract the decompressed main executable (.code) of a decrypted cartridge
image, for reading strings and data next to the recompiled code.

Usage: extract_code.py <game.cci> <out code.bin>
Writes the text, rodata and data segments laid out as loaded, starting at
the text address (0x00100000 for this game), so file offset + base is the
guest address. The output is game data: keep it out of Git (work/ is ignored).
"""
import struct
import sys

MEDIA = 0x200


def blz_decompress(data: bytes) -> bytes:
    """Nintendo's bottom-up LZ ("BLZ") used for ExeFS .code."""
    # footer: low 24 bits = compressed size from the end, high 8 = footer
    # size; then how much longer the output is
    footer, extra = struct.unpack_from("<II", data, len(data) - 8)
    enc_len = footer & 0xFFFFFF
    hdr_len = footer >> 24
    out = bytearray(data + bytes(extra))
    src = len(data) - hdr_len
    dst = len(out)
    end = len(data) - enc_len
    while src > end:
        src -= 1
        flags = data[src]
        for _ in range(8):
            if src <= end:
                break
            if flags & 0x80:
                src -= 2
                pair = data[src] | (data[src + 1] << 8)
                length = (pair >> 12) + 3
                disp = (pair & 0xFFF) + 3
                for _ in range(length):
                    dst -= 1
                    out[dst] = out[dst + disp]
            else:
                src -= 1
                dst -= 1
                out[dst] = data[src]
            flags <<= 1
    return bytes(out)


def main() -> None:
    rom, out = sys.argv[1:3]
    with open(rom, "rb") as f:
        f.seek(0x120)
        part0_off, part0_len = struct.unpack("<II", f.read(8))
        ncch = part0_off * MEDIA
        f.seek(ncch + 0x100)
        assert f.read(4) == b"NCCH", "not a decrypted NCCH"
        f.seek(ncch + 0x18F)
        if f.read(1)[0] & 0x04 == 0:
            sys.exit("the image is encrypted; a decrypted dump is needed")
        f.seek(ncch + 0x1A0)
        exefs_off, exefs_len = struct.unpack("<II", f.read(8))
        f.seek(ncch + 0x200)  # exheader
        exh = f.read(0x400)
        compressed = exh[0xD] & 1
        text_addr, text_pages, text_size = struct.unpack_from("<III", exh, 0x10)
        ro_addr, ro_pages, ro_size = struct.unpack_from("<III", exh, 0x20)
        data_addr, data_pages, data_size = struct.unpack_from("<III", exh, 0x30)
        exefs = ncch + exefs_off * MEDIA
        f.seek(exefs)
        header = f.read(0x200)
        for i in range(8):
            name, off, size = struct.unpack_from("<8sII", header, i * 16)
            if name.rstrip(b"\0") == b".code":
                f.seek(exefs + 0x200 + off)
                code = f.read(size)
                break
        else:
            sys.exit("no .code in ExeFS")
    if compressed:
        code = blz_decompress(code)
    if len(sys.argv) > 3:  # the decompressed .code as it is, to check its hash
        with open(sys.argv[3], "wb") as f:
            f.write(code)
    image = bytearray(data_addr + data_size - text_addr)
    t, r, d = text_pages * 0x1000, ro_pages * 0x1000, data_size
    image[0:text_size] = code[0:text_size]
    image[ro_addr - text_addr: ro_addr - text_addr + ro_size] = code[t: t + ro_size]
    image[data_addr - text_addr: data_addr - text_addr + d] = code[t + r: t + r + d]
    with open(out, "wb") as f:
        f.write(image)
    print(f"text {text_addr:08X}+{text_size:X} rodata {ro_addr:08X}+{ro_size:X} "
          f"data {data_addr:08X}+{data_size:X} -> {out} ({len(image)} bytes)")


if __name__ == "__main__":
    main()
