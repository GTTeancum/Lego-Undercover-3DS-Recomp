#!/usr/bin/env python3
"""Build-time-only supplement for observed missing A32 blocks; original pages stay intact.

Requires the exact private supported executable and the existing LLVM recovery
frontend. Generated C++/disassembly contain private game words and must NOT be
committed. No automatic runtime decoding, code patch, or forced return is used.
"""
from __future__ import annotations
import argparse, hashlib, json, pathlib, struct, tempfile
import generate_aot_pages_llvm as frontend

def checked_ranges(document: dict) -> list[tuple[int, int]]:
    if document.get('format') != 'lego_observed_aot_supplements_v1':
        raise ValueError('unsupported supplement manifest')
    raw = document.get('blocks')
    if not isinstance(raw, list) or not 1 <= len(raw) <= 128:
        raise ValueError('supplement block count outside host bound')
    ranges = sorted((int(b['entry'], 0), int(b['end'], 0)) for b in raw)
    prev = frontend.BASE
    for start, end in ranges:
        if start & 3 or end & 3 or not frontend.BASE <= start < end <= frontend.BASE + frontend.TEXT_BYTES:
            raise ValueError('invalid aligned text interval')
        if start < prev or (start // 4096) != ((end - 1) // 4096) or end - start > 256:
            raise ValueError('overlapping, cross-page or oversized supplement')
        prev = end
    return ranges

def render(code: bytes, inst: dict, ranges: list[tuple[int, int]]) -> tuple[str, dict]:
    lines=['// PRIVATE: generated from authenticated original input. Do not publish.',
           '#include "host/aot_supplement.h"','namespace lego::host {',
           'namespace a32 = oot3d::recomp::a32;']
    evidence=[]
    for n, (start, end) in enumerate(ranges):
        records=[];lines.append(f'static constexpr a32::PackedOp ops{n}[] = {{')
        for pc in range(start, end, 4):
            if pc not in inst: raise ValueError(f'missing LLVM instruction at {pc:#x}')
            raw, mn, operands=inst[pc]
            if struct.unpack_from('<I', code, pc-frontend.BASE)[0] != raw:
                raise ValueError('disassembly/input disagreement')
            opcode, condition, flags, category=frontend.classify(raw, mn)
            if opcode=='Unsupported' or category=='core_alu_candidate':
                raise ValueError('unverified supplemental opcode class')
            terminal=frontend.indirect_terminator(mn, operands) or frontend.branch_kind(mn) is not None
            if pc+4==end:
                if not terminal: raise ValueError('supplement must end at a decoded branch/return')
            elif frontend.ends(mn, operands):
                raise ValueError('internal control-flow exit requires a separately reviewed block')
            lines.append(f'    {{0x{raw:08X}U, a32::EncodeMetadata(a32::Opcode::{opcode}, a32::Condition::{condition}, 0x{flags:02X}U)}},')
            records.append({'pc':pc,'raw':raw,'mnemonic':mn,'operands':operands,'opcode':opcode,'condition':condition,'flags':flags})
        lines.append('};');evidence.append({'entry':start,'end':end,'instructions':records})
    lines.append('static const a32::Block blocks[] = {')
    for n,(start,end) in enumerate(ranges):lines.append(f'    {{0x{start:08X}U,ops{n},{(end-start)//4}U,false}},')
    lines+=['};','std::span<const a32::Block> GetAotSupplementalBlocks() noexcept {return blocks;}','} // namespace lego::host','']
    return '\n'.join(lines),{'blocks':evidence}

def main() -> int:
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('code',type=pathlib.Path);p.add_argument('manifest',type=pathlib.Path);p.add_argument('output',type=pathlib.Path)
    p.add_argument('--clang',default='clang');p.add_argument('--llvm-objdump',default='llvm-objdump');a=p.parse_args()
    code=a.code.read_bytes();digest=hashlib.sha256(code).hexdigest()
    if len(code)!=frontend.EXPECTED_CODE_BYTES or digest!=frontend.EXPECTED_CODE_SHA256:
        raise ValueError('supplement requires exact supported private code.bin')
    ranges=checked_ranges(json.loads(a.manifest.read_text()))
    with tempfile.TemporaryDirectory(prefix='lego-aot-supplement-') as tmp:
        dis=frontend.build_disassembly(a.code,pathlib.Path(tmp),a.clang,a.llvm_objdump)
        source,receipt=render(code,frontend.parse_disassembly(dis),ranges)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(source);receipt['code_sha256']=digest;receipt['generation']='offline LLVM frontend, no runtime fallback';receipt['source_sha256']=hashlib.sha256(source.encode()).hexdigest()
    a.output.with_suffix('.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(f'Generated {len(ranges)} authenticated supplemental A32 blocks')
    return 0
if __name__=='__main__':raise SystemExit(main())
