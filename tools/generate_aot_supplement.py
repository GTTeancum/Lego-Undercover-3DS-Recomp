#!/usr/bin/env python3
"""Build-time-only supplement for observed missing A32 blocks; original pages stay intact.

Requires the exact private supported executable and the existing LLVM recovery
frontend. Generated C++/disassembly contain private game words and must NOT be
committed. No automatic runtime decoding, code patch, or forced return is used.
"""
from __future__ import annotations
import argparse, hashlib, json, pathlib, re, struct, tempfile
import generate_aot_pages_llvm as frontend
from aot_recorded_fallthrough import recorded_page_ranges

MAX_SUPPLEMENT_BYTES = 1024  # At most 256 original words per reviewed logical section.
PAGE_BYTES = 4096  # Emitted physical blocks must stay inside their registry shard.

def verified_alu_candidate(raw: int) -> str | None:
    """Admit canonical shifted AND/SUB/ADD/ORR/MOV and immediate AND/TST/RSB forms.

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
    # A32 RSB immediate: class 001, opcode 0011, either S bit. CoreAlu
    # computes expanded immediate minus Rn (including negation for #0).
    # Carry/overflow come from subtraction, never from the immediate rotation
    # or the incoming carry. Keep PC and exception-return variants excluded.
    if (raw & 0x0FE00000) == 0x02600000:
        rn, rd = (raw >> 16) & 15, (raw >> 12) & 15
        return 'rsb_immediate' if rn != 15 and rd != 15 else None
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
    for block in raw:
        if 'recorded_fallthrough' in block:
            proof = block['recorded_fallthrough']
            if (not isinstance(proof, dict) or set(proof) != {'page_sha256'} or
                    not isinstance(proof['page_sha256'], str) or
                    re.fullmatch(r'[0-9a-f]{64}', proof['page_sha256']) is None):
                raise ValueError('invalid recorded_fallthrough page binding')
            if int(block['entry'], 0) // PAGE_BYTES != int(block['end'], 0) // PAGE_BYTES:
                raise ValueError('recorded fallthrough must stay within one page')
        if type(block.get('page_fallthrough', False)) is not bool:
            raise ValueError('page_fallthrough must be a boolean')
    ranges = sorted((int(b['entry'], 0), int(b['end'], 0),
                     b.get('page_fallthrough', False)) for b in raw)
    prev = frontend.BASE
    for start, end, page_fallthrough in ranges:
        if start & 3 or end & 3 or not frontend.BASE <= start < end <= frontend.BASE + frontend.TEXT_BYTES:
            raise ValueError('invalid aligned text interval')
        if start < prev or end - start > MAX_SUPPLEMENT_BYTES:
            raise ValueError('overlapping or oversized supplement')
        if start // PAGE_BYTES != (end - 1) // PAGE_BYTES and not page_fallthrough:
            raise ValueError('cross-page supplement requires explicit page_fallthrough')
        prev = end
    return [(start, end) for start, end, _ in ranges]

def authenticate_recorded_fallthroughs(code: bytes, document: dict,
                                      base_aot_dir: pathlib.Path | None) -> dict[int, int]:
    """Approve only missing prefixes ending at an exact, pinned original entry.

    No join is inferred from an address or disassembler mnemonic. The manifest
    must pin the unchanged original page; its complete declarations and every
    recorded word are checked against the same input used for generation.
    """
    ranges = checked_ranges(document)
    marked = [b for b in document['blocks'] if 'recorded_fallthrough' in b]
    if not marked:
        return {}
    if base_aot_dir is None:
        raise ValueError('recorded fallthrough requires a base AOT directory')
    joins = {}
    for block in marked:
        start, end = int(block['entry'], 0), int(block['end'], 0)
        page = start & ~(PAGE_BYTES - 1)
        path = base_aot_dir / frontend.page_name(page)
        if path.is_symlink() or not path.is_file() or path.stat().st_size > 512 * 1024:
            raise ValueError('recorded fallthrough requires a bounded regular base page')
        data = path.read_bytes()
        if hashlib.sha256(data).hexdigest() != block['recorded_fallthrough']['page_sha256']:
            raise ValueError('recorded base page hash mismatch')
        original = recorded_page_ranges(code, data.decode('utf-8'), page,
                                       text_base=frontend.BASE, text_bytes=frontend.TEXT_BYTES)
        if not any(a == end for a, _ in original):
            raise ValueError('fallthrough must reach an exact recorded block entry')
        if any(a < z and b > x for a, b in original for x, z in ranges):
            raise ValueError('supplement overlaps recorded code in pinned page')
        joins[start] = end
    return joins


def render(code: bytes, inst: dict, ranges: list[tuple[int, int]], *,
           recorded_fallthroughs: dict[int, int] | None = None) -> tuple[str, dict]:
    # Validate the complete reviewed section before dividing it at a page edge.
    # A split is ordinary sequential fallthrough, not a new guest instruction,
    # branch, return or runtime decoder. The first real control exit must still
    # be the final original word of the entire logical section, except an
    # explicitly authenticated same-page join to unchanged original code.
    ranges = checked_ranges({'format': 'lego_observed_aot_supplements_v1',
                             'blocks': [{'entry': hex(a), 'end': hex(b), 'page_fallthrough': True}
                                        for a, b in ranges]})
    joins = {} if recorded_fallthroughs is None else recorded_fallthroughs
    if any((a, z) not in ranges or a // PAGE_BYTES != z // PAGE_BYTES
           for a, z in joins.items()):
        raise ValueError('invalid recorded fallthrough range')
    lines=['// PRIVATE: generated from authenticated original input. Do not publish.',
           '#include "host/aot_supplement.h"','namespace lego::host {',
           'namespace a32 = oot3d::recomp::a32;']
    evidence=[]
    page_splits=[]
    for start, end in ranges:
        records=[]
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
                if start in joins:
                    if terminal or frontend.ends(mn, operands):
                        raise ValueError('recorded fallthrough cannot end in a control exit')
                elif not terminal: raise ValueError('supplement must end at a decoded branch/return')
            elif frontend.ends(mn, operands):
                raise ValueError('internal control-flow exit requires a separately reviewed block')
            records.append({'pc':pc,'raw':raw,'mnemonic':mn,'operands':operands,'opcode':opcode,'condition':condition,'flags':flags,'category':category})
        cursor=start
        while cursor<end:
            segment_end=min(end, (cursor//PAGE_BYTES+1)*PAGE_BYTES)
            segment=records[(cursor-start)//4:(segment_end-start)//4]
            n=len(evidence)
            lines.append(f'static constexpr a32::PackedOp ops{n}[] = {{')
            for row in segment:
                lines.append(f'    {{0x{row["raw"]:08X}U, a32::EncodeMetadata(a32::Opcode::{row["opcode"]}, a32::Condition::{row["condition"]}, 0x{row["flags"]:02X}U)}},')
            lines.append('};')
            evidence.append({'entry':cursor,'end':segment_end,'instructions':segment})
            if segment_end<end:
                page_splits.append({'entry':start,'end':end,'fallthrough_at':segment_end})
            cursor=segment_end
    lines.append('static const a32::Block blocks[] = {')
    for n, block in enumerate(evidence):
        lines.append(f'    {{0x{block["entry"]:08X}U,ops{n},{len(block["instructions"])}U,false}},')
    lines+=['};','std::span<const a32::Block> GetAotSupplementalBlocks() noexcept {return blocks;}','} // namespace lego::host','']
    receipt={'blocks':evidence}
    if page_splits:
        receipt['page_splits']=page_splits
    if joins:
        receipt['recorded_fallthroughs'] = [
            {'entry': a, 'end': z, 'target': z} for a, z in sorted(joins.items())]
    return '\n'.join(lines),receipt

def main() -> int:
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('code',type=pathlib.Path);p.add_argument('manifest',type=pathlib.Path);p.add_argument('output',type=pathlib.Path)
    p.add_argument('--clang',default='clang');p.add_argument('--llvm-objdump',default='llvm-objdump')
    p.add_argument('--base-aot-dir', type=pathlib.Path, help='Unchanged private original AOT pages for explicitly pinned joins')
    a=p.parse_args()
    code=a.code.read_bytes();digest=hashlib.sha256(code).hexdigest()
    if len(code)!=frontend.EXPECTED_CODE_BYTES or digest!=frontend.EXPECTED_CODE_SHA256:
        raise ValueError('supplement requires exact supported private code.bin')
    document = json.loads(a.manifest.read_text())
    ranges=checked_ranges(document)
    joins=authenticate_recorded_fallthroughs(code, document, a.base_aot_dir)
    with tempfile.TemporaryDirectory(prefix='lego-aot-supplement-') as tmp:
        dis=frontend.build_disassembly(a.code,pathlib.Path(tmp),a.clang,a.llvm_objdump)
        source,receipt=render(code,frontend.parse_disassembly(dis),ranges,recorded_fallthroughs=joins)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(source);receipt['code_sha256']=digest;receipt['generation']='offline LLVM frontend, no runtime fallback';receipt['source_sha256']=hashlib.sha256(source.encode()).hexdigest()
    a.output.with_suffix('.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(f'Generated {len(receipt["blocks"])} authenticated supplemental A32 blocks from {len(ranges)} reviewed sections')
    return 0
if __name__=='__main__':raise SystemExit(main())
