#!/usr/bin/env python3
"""List or extract files from the RomFS of a decrypted cartridge image.

Usage:
  romfs.py <game.cci> list [substring]
  romfs.py <game.cci> extract <path in RomFS> <out file>
  romfs.py <game.cci> grep <bytes> [path substring]   (files containing it)
Output is game data: keep extracted files in work/ (ignored by Git).
"""
import struct
import sys

MEDIA = 0x200


def open_romfs(path):
    f = open(path, "rb")
    f.seek(0x120)
    part0 = struct.unpack("<I", f.read(4))[0] * MEDIA
    f.seek(part0 + 0x100)
    assert f.read(4) == b"NCCH"
    f.seek(part0 + 0x1B0)
    romfs_off = struct.unpack("<I", f.read(4))[0] * MEDIA + part0
    f.seek(romfs_off)
    assert f.read(4) == b"IVFC", "RomFS IVFC header missing (encrypted?)"
    f.seek(romfs_off + 0x08)
    master_hash_size = struct.unpack("<I", f.read(4))[0]
    # level 3 follows the IVFC header and master hash, 0x1000-aligned
    lv3 = romfs_off + ((0x60 + master_hash_size + 0xFFF) & ~0xFFF)
    f.seek(lv3)
    hdr = struct.unpack("<10I", f.read(40))
    _, dh_off, dh_len, dm_off, dm_len, fh_off, fh_len, fm_off, fm_len, data_off = hdr
    f.seek(lv3 + dm_off)
    dirs = f.read(dm_len)
    f.seek(lv3 + fm_off)
    files = f.read(fm_len)
    return f, lv3 + data_off, dirs, files


def walk(dirs, files):
    out = []

    def name_at(blob, off, header):
        nlen = struct.unpack_from("<I", blob, off + header - 4)[0]
        return blob[off + header: off + header + nlen].decode("utf-16-le")

    def visit(doff, prefix):
        parent, sibling, child, first_file, _, nlen = struct.unpack_from("<6I", dirs, doff)
        foff = first_file
        while foff != 0xFFFFFFFF:
            _, fsib, data, size, _, fnlen = struct.unpack_from("<IIQQII", files, foff)
            name = files[foff + 0x20: foff + 0x20 + fnlen].decode("utf-16-le")
            out.append((prefix + name, data, size))
            foff = fsib
        c = child
        while c != 0xFFFFFFFF:
            _, csib, _, _, _, cnlen = struct.unpack_from("<6I", dirs, c)
            cname = dirs[c + 0x18: c + 0x18 + cnlen].decode("utf-16-le")
            visit(c, prefix + cname + "/")
            c = csib

    visit(0, "")
    return out


def main():
    f, data_base, dirs, files = open_romfs(sys.argv[1])
    entries = walk(dirs, files)
    cmd = sys.argv[2]
    if cmd == "list":
        sub = sys.argv[3].lower() if len(sys.argv) > 3 else ""
        for path, _, size in entries:
            if sub in path.lower():
                print(f"{size:10d}  {path}")
    elif cmd == "extract":
        for path, off, size in entries:
            if path == sys.argv[3]:
                f.seek(data_base + off)
                open(sys.argv[4], "wb").write(f.read(size))
                return
        sys.exit("not found")
    elif cmd == "grep":
        needle = sys.argv[3].encode()
        sub = sys.argv[4].lower() if len(sys.argv) > 4 else ""
        for path, off, size in entries:
            if sub in path.lower() and size < 64 << 20:
                f.seek(data_base + off)
                if needle in f.read(size):
                    print(path)


if __name__ == "__main__":
    main()
