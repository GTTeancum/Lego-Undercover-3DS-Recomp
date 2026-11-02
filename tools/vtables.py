#!/usr/bin/env python3
"""Name the game's classes from their static constructors, which store a
vtable and a name string side by side (as TouchCameraSystem's does), and
say which class a vtable slot belongs to.

Usage: vtables.py <code.bin> [0xSLOT ...]
Without slots, lists every (vtable, name) pair found.
"""
import bisect
import re
import struct
import sys

import capstone

BASE = 0x00100000
TEXT_END = 0x0035652C
DATA = (0x00357000, 0x00386670)


def main():
    image = open(sys.argv[1], "rb").read()

    def string_at(a):
        o = a - BASE
        if not 0 <= o < len(image):
            return None
        e = image.find(b"\0", o, o + 80)
        raw = image[o:e] if e > o else b""
        return raw.decode() if len(raw) >= 3 and all(0x20 <= c < 0x7F for c in raw) else None

    md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_ARM)
    md.skipdata = True
    window = []  # recent (kind, value) from the last few instructions
    pairs = {}
    for a, _, mn, op in md.disasm_lite(image[:TEXT_END - BASE], BASE):
        item = None
        m = re.search(r"\[pc, #(-?0x[0-9a-f]+|-?\d+)\]", op)
        if mn == "ldr" and m:
            lit = a + 8 + int(m.group(1), 0)
            if BASE <= lit < BASE + len(image) - 3:
                v = struct.unpack_from("<I", image, lit - BASE)[0]
                if DATA[0] <= v < DATA[1]:
                    item = ("vt", v)
                elif string_at(v):
                    item = ("name", string_at(v))
        elif mn == "add" and ", pc, #" in op:
            imm = int(re.search(r"#(0x[0-9a-f]+|\d+)", op).group(1), 0)
            s = string_at(a + 8 + imm)
            if s:
                item = ("name", s)
        if item:
            window.append((a, item))
        window = [w for w in window if a - w[0] <= 24]
        if mn.startswith("stm") or mn == "str":
            vts = [i[1] for _, i in window if i[0] == "vt"]
            names = [i[1] for _, i in window if i[0] == "name"]
            if vts and names and re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]{3,}", names[-1]):
                pairs.setdefault(vts[-1], names[-1])
                window = []
    # every data address the code loads is where something starts: a slot
    # belongs to the closest such start before it
    loaded = set()
    for off in range(0, TEXT_END - BASE - 3, 4):
        v = struct.unpack_from("<I", image, off)[0]
        if DATA[0] <= v < DATA[1]:
            loaded.add(v)
    loaded |= set(pairs)
    table = sorted(pairs.items())
    if len(sys.argv) == 2:
        for vt, name in table:
            print(f"{vt:08X} {name}")
        return
    starts = sorted(loaded)
    for arg in sys.argv[2:]:
        slot = int(arg, 16)
        i = bisect.bisect_right(starts, slot) - 1
        vt = starts[i] if i >= 0 else None
        name = pairs.get(vt, "?") if vt is not None else "?"
        print(f"{slot:08X} -> {name} (vtable {vt:08X} +{slot - vt:#x})" if vt else f"{slot:08X} -> ?")


if __name__ == "__main__":
    main()
