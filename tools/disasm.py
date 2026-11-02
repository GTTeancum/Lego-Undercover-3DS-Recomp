#!/usr/bin/env python3
"""Disassemble one recompiled function from the code image, with the
strings it points at and the functions it calls named.

Usage: disasm.py <code.bin> <generated dir> <function address> [...]
Needs capstone (pip install capstone).
"""
import bisect
import pathlib
import re
import struct
import sys

import capstone

BASE = 0x00100000


def functions(generated: pathlib.Path) -> list[int]:
    starts = set()
    for path in generated.glob("code*.c"):
        starts.update(int(m, 16) for m in re.findall(r"^void f_([0-9A-F]{8})\(", path.read_text(), re.M))
    return sorted(starts)


def string_at(image: bytes, address: int) -> str | None:
    off = address - BASE
    if not 0 <= off < len(image):
        return None
    end = image.find(b"\0", off, off + 200)
    if end - off < 3:
        return None
    raw = image[off:end]
    if all(0x20 <= c < 0x7F for c in raw):
        return raw.decode()
    return None


def main() -> None:
    image = pathlib.Path(sys.argv[1]).read_bytes()
    starts = functions(pathlib.Path(sys.argv[2]))
    md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_ARM)
    md.detail = True
    md.skipdata = True
    for arg in sys.argv[3:]:
        start = int(arg, 16)
        i = bisect.bisect_right(starts, start)
        end = starts[i] if i < len(starts) else start + 0x400
        print(f"; f_{start:08X} .. {end:08X}")
        code = image[start - BASE:end - BASE]
        for ins in md.disasm(code, start):
            note = ""
            if ins.mnemonic.startswith("ldr") and "[pc," in ins.op_str:
                lit = ins.address + 8 + int(re.search(r"#(-?0x[0-9a-f]+|-?\d+)", ins.op_str).group(1), 0)
                if BASE <= lit < BASE + len(image) - 3:
                    value = struct.unpack_from("<I", image, lit - BASE)[0]
                    s = string_at(image, value)
                    note = f"=0x{value:08X}" + (f' "{s}"' if s else "")
            elif re.match(r"(add|sub)", ins.mnemonic) and ins.op_str.split(", ")[1:2] == ["pc"]:
                imm = int(re.search(r"#(0x[0-9a-f]+|\d+)", ins.op_str).group(1), 0)
                target = ins.address + 8 + (imm if ins.mnemonic.startswith("add") else -imm)
                s = string_at(image, target)
                note = f"-> 0x{target:08X}" + (f' "{s}"' if s else "")
            elif ins.mnemonic in ("bl", "blx", "b") or ins.mnemonic.startswith("bl"):
                m = re.match(r"#(0x[0-9a-f]+)", ins.op_str)
                if m and int(m.group(1), 16) in set(starts):
                    note = f"f_{int(m.group(1), 16):08X}"
            print(f"{ins.address:08X}: {ins.mnemonic:8} {ins.op_str}" + (f"   ; {note}" if note else ""))


if __name__ == "__main__":
    main()
