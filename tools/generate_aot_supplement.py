#!/usr/bin/env python3
"""Build-time-only supplement for observed missing A32 blocks; original pages stay intact.

Requires the exact private supported executable and the existing LLVM recovery
frontend. Generated C++/disassembly contain private game words and must NOT be
committed. No automatic runtime decoding, code patch, or forced return is used.
"""
from __future__ import annotations
import argparse, hashlib, json, pathlib, struct, tempfile
import generate_aot_pages_llvm as frontend

MAX_SUPPLEMENT_BYTES = 1024  # At most 256 A32 words, still within one 4 KiB page.

def verified_alu_candidate(raw: int) -> str | None:
    """Admit canonical shifted AND/SUB/ADD/ORR/MOV and immediate AND/TST forms.

    The LLVM frontend's generic CoreAlu category is deliberately broader than
    this policy. Immediate TST requires primary class 001, opcode 1000, S=1
    and canonical unused Rd=0. Its full rotated-immediate field is retained;
    the existing evaluator updates NZC and preserves V without writing a
    destination. The shifted register forms use the existing immediate
    shifter (including RRX), condition and raw S bit. Logical operations use
    shifter carry while SUB uses arithmetic carry/overflow when S is set.
    Exclude PC reads/writes, exception returns, register-specified shifts
    and neighboring encoding spaces.
    MOV's unused Rn field must be zero. No address or mnemonic is a permit.
    """
    if not 0 <= raw <= 0xFFFFFFFF or raw >> 28 == 15:
        return None
    # A32 AND immediate: class 001, opcode 0000, either S bit. The full
    # rotated immediate stays in raw; CoreAlu derives NZC only when S=1.
    # Reject PC source/destination, including exception-return variants.
    if (raw & 0x0FE00000) == 0x02000000:
        rn, rd = (raw >> 16) & 15, (raw >> 12) & 15
        return 'and_immediate' if rn != 15 and rd != 15 else None
    if (raw & 0x0FF0F000) == 0x03100000 and ((raw >> 16) & 15) != 15:
        return 'tst_immediate'
    if raw & 0x0E000010:  # bits 27..25=000 and bit 4=0: immediate register shift
        return None
    opcode = (raw >> 21) & 15
    names = {0: 'and_register_immediate_shift',
             2: 'sub_register_immediate_shift',
             4: 'add_register_immediate_shift',
             12: 'orr_register_immediate_shift',
             13: 'mov_register_immediate_shift'}
    if opcode not in names:
        return None
    rn, rd, rm = (raw >> 16) & 15, (raw >> 12) & 15, raw & 15
    if rd == 15 or rm == 15:
        return None
    if (opcode == 13 and rn != 0) or (opcode != 13 and rn == 15):
        return None
    return names[opcode]

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
        if start < prev or (start // 4096) != ((end - 1) // 4096) or end - start > MAX_SUPPLEMENT_BYTES:
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
            if category=='core_alu_candidate':
                category=verified_alu_candidate(raw)
            if opcode=='Unsupported' or category is None:
                raise ValueError('unverified supplemental opcode class')
            terminal=frontend.indirect_terminator(mn, operands) or frontend.branch_kind(mn) is not None
            if pc+4==end:
                if not terminal: raise ValueError('supplement must end at a decoded branch/return')
            elif frontend.ends(mn, operands):
                raise ValueError('internal control-flow exit requires a separately reviewed block')
            lines.append(f'    {{0x{raw:08X}U, a32::EncodeMetadata(a32::Opcode::{opcode}, a32::Condition::{condition}, 0x{flags:02X}U)}},')
            records.append({'pc':pc,'raw':raw,'mnemonic':mn,'operands':operands,'opcode':opcode,'condition':condition,'flags':flags,'category':category})
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
