#!/usr/bin/env python3
"""Find the recompiled functions that refer to a string or address.

ARM code reaches an address either from a literal pool (a word holding it
placed after a function's code) or, for data near the code, with a
PC-relative ADD/SUB (ADR). This finds both in the code image and names the
function whose range holds the reference (functions from the f_XXXXXXXX
definitions 3dsrecomp generated). Text is looked up from the start of the
string that contains it.

Usage:
  xrefs.py <code.bin> <generated dir> <text or 0xADDRESS> [...]
Text is searched in the image and each place it occurs is looked up.
"""
import bisect
import pathlib
import re
import struct
import sys

BASE = 0x00100000


def functions(generated: pathlib.Path) -> list[int]:
    starts = set()
    for path in generated.glob("code*.c"):
        starts.update(int(m, 16) for m in re.findall(r"^void f_([0-9A-F]{8})\(", path.read_text(), re.M))
    return sorted(starts)


def owner(starts: list[int], address: int) -> int | None:
    i = bisect.bisect_right(starts, address) - 1
    return starts[i] if i >= 0 else None


def main() -> None:
    image = pathlib.Path(sys.argv[1]).read_bytes()
    starts = functions(pathlib.Path(sys.argv[2]))
    words = {}
    text_end = 0x00357000 - BASE
    for off in range(0, len(image) - 3, 4):
        word = struct.unpack_from("<I", image, off)[0]
        words.setdefault(word, []).append(BASE + off)
        # B/BL/BLX (ARM): calls and jumps reach a function by offset
        if off < text_end and (word & 0x0E000000) == 0x0A000000:
            disp = word & 0xFFFFFF
            disp = (disp - (1 << 24) if disp & 0x800000 else disp) << 2
            if (word >> 28) == 0xF:  # BLX imm: Thumb target, H bit
                disp |= ((word >> 24) & 1) << 1
            words.setdefault((BASE + off + 8 + disp) & 0xFFFFFFFF, []).append(BASE + off)
        # ADD/SUB Rd, PC, #imm (ARM, any condition): ADR
        if off < text_end and (word & 0x0FFF0000) in (0x028F0000, 0x024F0000):
            rot = ((word >> 8) & 0xF) * 2
            imm = word & 0xFF
            imm = ((imm >> rot) | (imm << (32 - rot))) & 0xFFFFFFFF
            pc = BASE + off + 8
            target = (pc + imm if (word & 0x0FFF0000) == 0x028F0000 else pc - imm) & 0xFFFFFFFF
            words.setdefault(target, []).append(BASE + off)
    for query in [q for q in sys.argv[3:] if q != "--sites"]:
        if query.startswith("0x"):
            targets = [(int(query, 16), query)]
        else:
            needle = query.encode()
            targets = []
            for m in re.finditer(re.escape(needle), image):
                start = m.start()
                while start > 0 and 0x20 <= image[start - 1] < 0x7F:
                    start -= 1
                end = image.index(b"\0", m.end())
                targets.append((BASE + start, image[start:end].decode("latin-1")))
        for target, label in targets:
            refs = words.get(target, [])
            # a printable byte just before a string (alignment, data) makes
            # it look longer: try a byte or two in
            for skip in (1, 2):
                if refs or not label:
                    break
                if words.get(target + skip):
                    target, label, refs = target + skip, label[skip:], words[target + skip]
            users = sorted({owner(starts, r) for r in refs if r < 0x00357000} - {None})
            data = [r for r in refs if r >= 0x00357000]
            names = " ".join(f"f_{u:08X}" for u in users) or "-"
            if data:
                names += "  in data: " + " ".join(f"{r:08X}" for r in data)
            if "--sites" in sys.argv:
                names += "  sites: " + " ".join(f"{r:08X}" for r in sorted(refs) if r < 0x00357000)
            print(f"{target:08X} {label!r}: {names}")


if __name__ == "__main__":
    main()
