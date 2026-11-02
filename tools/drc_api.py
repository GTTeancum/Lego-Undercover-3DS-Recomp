#!/usr/bin/env python3
"""Profile every function that uses the bottom-screen (DRC) manager global:
which of its fields it reads and writes, strings it uses, what it calls.
Usage: drc_api.py <code.bin> <generated dir> [global address]"""
import bisect, pathlib, re, struct, sys
import capstone

BASE = 0x00100000
image = pathlib.Path(sys.argv[1]).read_bytes()
GLOBAL = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0x00519D94
starts = set()
for p in pathlib.Path(sys.argv[2]).glob("code*.c"):
    starts.update(int(m, 16) for m in re.findall(r"^void f_([0-9A-F]{8})\(", p.read_text(), re.M))
starts = sorted(starts)
md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_ARM)
md.skipdata = True

def string_at(a):
    o = a - BASE
    if not 0 <= o < len(image): return None
    e = image.find(b"\0", o, o + 120)
    raw = image[o:e] if e > o else b""
    return raw.decode() if len(raw) >= 3 and all(0x20 <= c < 0x7F for c in raw) else None

users = set()
for off in range(0, 0x257000, 4):
    if struct.unpack_from("<I", image, off)[0] == GLOBAL:
        i = bisect.bisect_right(starts, BASE + off) - 1
        if i >= 0: users.add(starts[i])
for f in sorted(users):
    i = bisect.bisect_right(starts, f)
    end = starts[i] if i < len(starts) else f + 0x400
    held = set()  # registers holding the global (or the object it points to)
    reads, writes, strs, calls = set(), set(), [], []
    for a, sz, mn, op in md.disasm_lite(image[f - BASE:end - BASE], f):
        regs = [r.strip() for r in op.split(",")]
        dst = regs[0] if regs else ""
        m = re.search(r"\[pc, #(-?0x[0-9a-f]+|-?\d+)\]", op)
        if mn.startswith("ldr") and m:
            lit = a + 8 + int(m.group(1), 0)
            v = struct.unpack_from("<I", image, lit - BASE)[0] if BASE <= lit < BASE + len(image) - 3 else 0
            if v == GLOBAL: held.add(dst); continue
            s = string_at(v)
            if s: strs.append(s)
        elif re.match(r"(add|sub)", mn) and len(regs) > 2 and regs[1] == "pc":
            imm = int(re.search(r"#(0x[0-9a-f]+|\d+)", op).group(1), 0)
            s = string_at(a + 8 + (imm if mn.startswith("add") else -imm))
            if s: strs.append(s)
        mm = re.search(r"\[(\w+), #(0x[0-9a-f]+|\d+)\]", op)
        if mm and mm.group(1) in held:
            (writes if mn.startswith("str") else reads).add(int(mm.group(2), 0))
        elif mm is None and re.search(r"\[(\w+)\]", op) and re.search(r"\[(\w+)\]", op).group(1) in held:
            (writes if mn.startswith("str") else reads).add(0)
        if mn.startswith("bl"):
            t = re.match(r"#(0x[0-9a-f]+)", op)
            if t: calls.append(int(t.group(1), 16))
        if dst in held and not mn.startswith(("str", "cmp", "tst", "push")):
            held.discard(dst)
    fmt = lambda s: " ".join(f"+{x:X}" for x in sorted(s)) or "-"
    print(f"f_{f:08X} ({end - f:#x} bytes)  reads {fmt(reads)}  writes {fmt(writes)}")
    if strs: print("    strings: " + "; ".join(dict.fromkeys(strs)))
    if calls: print("    calls: " + " ".join(f"f_{c:08X}" for c in dict.fromkeys(calls))[:300])
