#!/usr/bin/env python3
"""Unpacks LEGO City Undercover: The Chase Begins' lego_city3ds.fib (TT
Games FUSE1.00, table of contents in lego_city3ds.csv). Compressed entries
are a u32 length then a RefPack-like stream, as the game's f_0012EFE0
decodes it, in chunks of up to 256 KiB unpacked.

Usage: python3 tools/fuse.py <lego_city3ds.fib> <lego_city3ds.csv> <out dir> [name filter]
(both come out of the RomFS: tools/romfs.py <rom> extract <name> <file>)."""
import csv, sys, os

def unpack(src, size):
    out = bytearray(); i = 0; n = len(src)
    while i < n and len(out) < size:
        b = src[i]
        if b < 0x80:                       # 2 bytes
            b1 = src[i+1]; i += 2
            lit = (b >> 2) & 3; cnt = ((b >> 4) & 7) + 3; off = ((b & 3) << 8) + b1 + 1
        elif b < 0xC0:                     # 3 bytes
            b1, b2 = src[i+1], src[i+2]; i += 3
            lit = b1 >> 6; cnt = (b & 0x3F) + 4; off = ((b1 & 0x3F) << 8) + b2 + 1
        elif b < 0xE0:                     # 4 bytes
            b1, b2, b3 = src[i+1], src[i+2], src[i+3]; i += 4
            lit = (b >> 3) & 3; cnt = b3 + ((b & 6) << 7) + 5; off = ((b & 1) << 16) + (b1 << 8) + b2 + 1
        elif b < 0xFC:                     # literals
            lit = ((b & 0x1F) << 2) + 4; cnt = 0; off = 0; i += 1
        else:                              # last literals, end
            lit = b & 3; cnt = 0; off = 0; i += 1
        out += src[i:i+lit]; i += lit
        if cnt:
            start = len(out) - off
            for k in range(cnt): out.append(out[start + k])
        if b >= 0xFC: break
    return bytes(out)

def entries(csv_path):
    rows = list(csv.reader(open(csv_path)))[1:]
    return [(r[1], int(r[2]), int(r[3]), int(r[4], 16)) for r in rows]

CHUNK = 0x40000

def read(f, comp, size, off):
    """an entry's bytes; compressed ones come in chunks of up to 256 KiB
    unpacked, each a u32 length and its stream."""
    f.seek(off); data = f.read(comp)
    if comp == size: return data
    out = bytearray(); pos = 0
    while len(out) < size and pos + 4 <= len(data):
        n = int.from_bytes(data[pos:pos+4], 'little')
        out += unpack(data[pos+4:pos+4+n], min(CHUNK, size - len(out)))
        pos += 4 + n
    return bytes(out)

if __name__ == '__main__':
    fib, csvp, outdir = sys.argv[1:4]
    want = sys.argv[4] if len(sys.argv) > 4 else ''
    f = open(fib, 'rb'); bad = 0; done = 0
    for name, comp, size, off in entries(csvp):
        if want and want not in name: continue
        d = read(f, comp, size, off)
        if len(d) != size: bad += 1; print('short', name, len(d), size)
        p = os.path.join(outdir, name); os.makedirs(os.path.dirname(p), exist_ok=True)
        open(p, 'wb').write(d); done += 1
    print(done, 'files,', bad, 'wrong size')
