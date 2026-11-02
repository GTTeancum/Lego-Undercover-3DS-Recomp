#!/usr/bin/env python3
"""List a vtable's class-specific methods with the strings each uses.
Usage: vtable_dump.py <code.bin> <generated dir> <0xVTABLE> [...]"""
import pathlib, re, struct, sys
sys.path.insert(0, str(pathlib.Path(__file__).parent))
from callers import load, strings_of

BASE = 0x00100000
image, starts, refs = load(sys.argv[1], sys.argv[2])
fset = set(starts)
for arg in sys.argv[3:]:
    vt = int(arg, 16)
    print(f"== vtable {vt:08X}")
    for i in range(48):
        a = vt + 4 * i
        w = struct.unpack_from("<I", image, a - BASE)[0]
        if w == 0:
            continue
        if w not in fset:
            break
        if 0x00339F00 <= w < 0x0033A000:
            continue  # the base class's empty stubs
        s = strings_of(image, starts, w)
        print(f"  +{4*i:02X} f_{w:08X}" + (("  [" + "; ".join(s[:8]) + "]") if s else ""))
