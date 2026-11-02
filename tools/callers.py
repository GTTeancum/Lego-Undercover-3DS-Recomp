#!/usr/bin/env python3
"""Who calls a function, up to a depth, with each caller's strings to tell
what it is. Calls through vtables show as "vtable@<data address>".

Usage: callers.py <code.bin> <generated dir> <0xADDRESS> [depth] [--strings]
Needs capstone only for --strings (the strings each function uses).
"""
import bisect
import pathlib
import re
import struct
import sys

BASE = 0x00100000
TEXT_END = 0x00357000


def load(code_path, generated):
    image = pathlib.Path(code_path).read_bytes()
    starts = set()
    for p in pathlib.Path(generated).glob("code*.c"):
        starts.update(int(m, 16) for m in re.findall(r"^void f_([0-9A-F]{8})\(", p.read_text(), re.M))
    starts = sorted(starts)
    refs = {}
    for off in range(0, len(image) - 3, 4):
        word = struct.unpack_from("<I", image, off)[0]
        here = BASE + off
        refs.setdefault(word, []).append(here)  # literal or vtable entry
        if off < TEXT_END - BASE:
            if (word & 0x0E000000) == 0x0A000000:  # B/BL/BLX
                disp = word & 0xFFFFFF
                disp = (disp - (1 << 24) if disp & 0x800000 else disp) << 2
                if (word >> 28) == 0xF:
                    disp |= ((word >> 24) & 1) << 1
                refs.setdefault((here + 8 + disp) & 0xFFFFFFFF, []).append(here)
    return image, starts, refs


def owner(starts, address):
    i = bisect.bisect_right(starts, address) - 1
    return starts[i] if i >= 0 else None


def strings_of(image, starts, f):
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_ARM)
    md.skipdata = True
    i = bisect.bisect_right(starts, f)
    end = starts[i] if i < len(starts) else f + 0x400
    found = []

    def at(a):
        o = a - BASE
        if not 0 <= o < len(image):
            return None
        e = image.find(b"\0", o, o + 100)
        raw = image[o:e] if e > o else b""
        return raw.decode() if len(raw) >= 3 and all(0x20 <= c < 0x7F for c in raw) else None

    for a, _, mn, op in md.disasm_lite(image[f - BASE:end - BASE], f):
        m = re.search(r"\[pc, #(-?0x[0-9a-f]+|-?\d+)\]", op)
        if mn.startswith("ldr") and m:
            lit = a + 8 + int(m.group(1), 0)
            if BASE <= lit < BASE + len(image) - 3:
                s = at(struct.unpack_from("<I", image, lit - BASE)[0])
                if s:
                    found.append(s)
        elif re.match(r"(add|sub)", mn) and ", pc, #" in op:
            imm = int(re.search(r"#(0x[0-9a-f]+|\d+)", op).group(1), 0)
            s = at(a + 8 + (imm if mn.startswith("add") else -imm))
            if s:
                found.append(s)
    return list(dict.fromkeys(found))


def main():
    image, starts, refs = load(sys.argv[1], sys.argv[2])
    root = int(sys.argv[3], 16)
    depth = int(sys.argv[4]) if len(sys.argv) > 4 and not sys.argv[4].startswith("--") else 2
    show = "--strings" in sys.argv
    seen = set()

    def walk(f, level):
        label = f"f_{f:08X}"
        if show:
            s = strings_of(image, starts, f)
            if s:
                label += "  [" + "; ".join(s[:6]) + "]"
        print("  " * level + label)
        if level >= depth or f in seen:
            return
        seen.add(f)
        users = set()
        for r in refs.get(f, []):
            if r < TEXT_END:
                u = owner(starts, r)
                if u is not None and u != f:
                    users.add(("f", u))
            else:
                users.add(("v", r))
        for kind, u in sorted(users):
            if kind == "f":
                walk(u, level + 1)
            else:
                print("  " * (level + 1) + f"vtable@{u:08X}")

    walk(root, 0)


if __name__ == "__main__":
    main()
