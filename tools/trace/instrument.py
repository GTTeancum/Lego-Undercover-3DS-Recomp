#!/usr/bin/env python3
"""Copy 3dsrecomp's generated C into a trace build: each f_XXXXXXXX counts
its entries (calls landing on its start) through recomp_trace_hit.
Usage: instrument.py <generated dir> <trace dir>"""
import pathlib, re, shutil, sys

src, dst = map(pathlib.Path, sys.argv[1:3])
dst.mkdir(parents=True, exist_ok=True)
head = re.compile(r"^void f_([0-9A-F]{8})\(Context \*ctx\) \{$", re.M)
for path in src.iterdir():
    if path.suffix == ".h":
        shutil.copy(path, dst / path.name)
    elif path.suffix == ".c":
        text = path.read_text()
        if path.name.startswith("code"):
            text = "#include <stdint.h>\nextern void recomp_trace_hit(uint32_t);\n" + head.sub(
                lambda m: f"{m.group(0)}\n    if (ctx->r[15] == 0x{m.group(1)}u) recomp_trace_hit(0x{m.group(1)}u);", text)
        (dst / path.name).write_text(text)
shutil.copy(pathlib.Path(__file__).with_name("trace.c"), dst / "trace.c")
