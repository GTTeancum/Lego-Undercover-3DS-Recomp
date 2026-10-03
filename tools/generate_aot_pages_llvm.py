#!/usr/bin/env python3
"""Generate a conservative page-oriented LEGO Chase AOT tree using LLVM.

This is a deterministic recovery path for environments where the historically
pinned Capstone 5.0.7 frontend cannot run. It verifies the exact supported
code.bin, disassembles the allocated ARM text with LLVM, recovers conservative
control flow from the real entrypoint/initializer/callback/absolute-pointer roots,
and emits 599 four-KiB page translation units plus five support artifacts.

This output is deliberately labelled diagnostic/recovery output. It is not
claimed byte-identical to Recovery J and uncertain instructions remain explicit
Unsupported/Core* candidates instead of fabricated semantics.
"""
from __future__ import annotations

import argparse
from collections import deque
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile

BASE = 0x00100000
TEXT_BYTES = 2_450_732
PAGE_SIZE = 0x1000
TEXT_PAGES = 599
TEXT_ALLOCATED_BYTES = TEXT_PAGES * PAGE_SIZE
EXPECTED_CODE_BYTES = 2_650_112
EXPECTED_CODE_SHA256 = "5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f"
RECOVERY_ARTIFACTS = 604
INIT_BEGIN = 0x0036A1A8
INIT_END = 0x0036A648
OBSERVED_CALLBACKS = (0x002DEB78, 0x002FAC60, 0x003261EC)

CONDITIONS = ("Eq","Ne","Cs","Cc","Mi","Pl","Vs","Vc","Hi","Ls","Ge","Lt","Gt","Le","Al","Nv")
COND_SUFFIXES = ("eq","ne","hs","cs","lo","cc","mi","pl","vs","vc","hi","ls","ge","lt","gt","le","al")
SYSTEM = {"nop","yield","wfe","wfi","sev","clrex","dmb","dsb","isb","mrs","msr","cps","setend","bkpt","udf","hvc","smc"}
MEM_PREFIX = ("ldr","str","ldm","stm","push","pop","ldrex","strex","swp","pld","pli")
VFP_TRANSPORT = ("vldr","vstr","vmov","vmrs","vmsr","vpush","vpop","vldmia","vstmia")
VFP_SCALAR = ("vadd","vsub","vmul","vdiv","vabs","vneg","vsqrt","vcmp","vcmpe","vcvt","vmla","vmls","vnmla","vnmls","vnmul")
LINE_RE = re.compile(r"^\s*([0-9a-f]+):\s+([0-9a-f]{8})\s+([^\s]+)(?:\s+(.*?))?\s*$")
HEX_RE = re.compile(r"0x([0-9a-f]+)")


def file_sha256(path: Path) -> str:
    h=hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda:f.read(1024*1024),b''): h.update(chunk)
    return h.hexdigest()


def strip_cond(mn: str) -> str:
    base=mn.lower().split('.',1)[0]
    for cc in COND_SUFFIXES:
        if base.endswith(cc) and len(base)>len(cc)+1:
            return base[:-len(cc)]
    return base


def vfp_family(mn: str, families: tuple[str, ...]) -> str | None:
    """Match a VFP mnemonic without confusing opcode text with condition suffixes."""
    base=mn.lower().split('.',1)[0]
    if base in families:
        return base
    for cc in COND_SUFFIXES:
        if base.endswith(cc):
            stem=base[:-len(cc)]
            if stem in families:
                return stem
    return None


def build_disassembly(code_path: Path, work: Path, clang: str, llvm_objdump: str) -> Path:
    asm=work/'code.S'; obj=work/'code.o'; dis=work/'text.disasm'
    escaped=str(code_path.resolve()).replace('\\','\\\\').replace('"','\\"')
    asm.write_text(
        '.syntax unified\n.arm\n.section .text,"ax",%progbits\n.global _start\n_start:\n'
        f'.incbin "{escaped}", 0, {TEXT_ALLOCATED_BYTES}\n', encoding='utf-8')
    subprocess.run([clang,'-target','armv7a-none-eabi','-mfpu=neon','-c',str(asm),'-o',str(obj)],check=True)
    with dis.open('wb') as out:
        subprocess.run([llvm_objdump,'-d',str(obj)],stdout=out,check=True)
    return dis


def parse_disassembly(path: Path) -> dict[int,tuple[int,str,str]]:
    inst={}
    with path.open(errors='replace') as f:
        for line in f:
            m=LINE_RE.match(line)
            if not m: continue
            off=int(m.group(1),16)
            if off>=TEXT_BYTES: continue
            inst[BASE+off]=(int(m.group(2),16),m.group(3).lower(),(m.group(4) or '').strip().lower())
    return inst


def direct_target(operand: str) -> int|None:
    m=HEX_RE.search(operand)
    return BASE+int(m.group(1),16) if m else None


def branch_kind(mn: str) -> tuple[str,bool]|None:
    if mn=='b': return ('branch',False)
    if mn=='bl': return ('call',False)
    if mn=='blx': return ('callx',False)
    for cc in COND_SUFFIXES:
        if mn=='b'+cc: return ('branch',True)
        if mn=='bl'+cc: return ('call',True)
    return None


def indirect_terminator(mn: str, operand: str) -> bool:
    base=strip_cond(mn); low=operand.lower().strip()
    if base=='bx': return True
    if base=='pop' and re.search(r'\{[^}]*\bpc\b[^}]*\}',low): return True
    if base.startswith('ldm') and re.search(r'\{[^}]*\bpc\b[^}]*\}',low): return True
    dest=low.split(',',1)[0].strip() if low else ''
    return dest=='pc' and base.startswith(('ldr','mov','mvn','add','sub','rsb','orr','eor','and','bic','adc','sbc','rsc'))


def ror32(value: int, amount: int) -> int:
    amount &= 31
    value &= 0xFFFFFFFF
    return value if amount == 0 else ((value >> amount) | (value << (32 - amount))) & 0xFFFFFFFF


def simple_dp(raw: int):
    if ((raw >> 28) & 15) != 14 or (raw & 0x0C000000) != 0:
        return None
    immediate = bool(raw & (1 << 25))
    opcode = (raw >> 21) & 15
    rn = (raw >> 16) & 15
    rd = (raw >> 12) & 15
    operand2 = raw & 0xFFF
    if opcode not in (2, 4, 10, 13):
        return None
    if immediate:
        imm = ror32(operand2 & 0xFF, ((operand2 >> 8) & 15) * 2)
        return opcode, rd, rn, None, imm
    if operand2 & 0xFF0:
        return None
    return opcode, rd, rn, operand2 & 15, None


def conservative_writes(raw: int):
    decoded = simple_dp(raw)
    if decoded is not None:
        opcode, rd, _, _, _ = decoded
        return set() if opcode == 10 else {rd}
    if (raw & 0x0C000000) == 0x04000000:
        writes = set()
        rn = (raw >> 16) & 15
        rd = (raw >> 12) & 15
        if raw & (1 << 20):
            writes.add(rd)
        if raw & (1 << 21) or not raw & (1 << 24):
            writes.add(rn)
        return writes
    if (raw & 0x0E000000) == 0x08000000:
        writes = set()
        rn = (raw >> 16) & 15
        if raw & (1 << 20):
            writes.update(register for register in range(16) if raw & (1 << register))
        if raw & (1 << 21):
            writes.add(rn)
        return writes
    return None


def static_register_before(
    inst: dict[int, tuple[int, str, str]], reachable: set[int], pc: int, register: int
) -> int | None:
    adjustment = 0
    for candidate in range(pc - 4, pc - 68, -4):
        if candidate not in reachable or candidate not in inst:
            break
        raw, mn, operand = inst[candidate]
        if branch_kind(mn) or mn == "blx" or indirect_terminator(mn, operand):
            break
        writes = conservative_writes(raw)
        if writes is None or 15 in writes:
            break
        if register not in writes:
            continue
        decoded = simple_dp(raw)
        if decoded is None:
            break
        opcode, _, rn, rm, imm = decoded
        if opcode == 13:
            if imm is not None:
                return (imm + adjustment) & 0xFFFFFFFF
            if rm == 15:
                return (candidate + 8 + adjustment) & 0xFFFFFFFF
            register = rm
        elif opcode in (4, 2) and imm is not None:
            adjustment += imm if opcode == 4 else -imm
            if rn == 15:
                return (candidate + 8 + adjustment) & 0xFFFFFFFF
            register = rn
        else:
            break
    return None


def explicit_lr_return(
    inst: dict[int, tuple[int, str, str]], reachable: set[int], pc: int
) -> int | None:
    target = static_register_before(inst, reachable, pc, 14)
    return target if target in inst else None


def base_relative_switch_targets(
    inst: dict[int, tuple[int, str, str]], code: bytes, reachable: set[int]
) -> tuple[set[int], set[int], list[int]]:
    targets: set[int] = set()
    table_words: set[int] = set()
    sites: list[int] = []
    for pc in sorted(reachable):
        raw, _, _ = inst[pc]
        decoded = simple_dp(raw)
        if decoded is None:
            continue
        opcode, rd, rn, rm, imm = decoded
        if opcode != 4 or rd != 15 or imm is not None:
            continue
        load_pc = None
        load_raw = None
        for candidate in range(pc - 4, pc - 36, -4):
            if candidate not in reachable or candidate not in inst:
                break
            candidate_raw, _, _ = inst[candidate]
            if (
                (candidate_raw & 0x0FF00000) == 0x07900000
                and not candidate_raw & 0x10
                and ((candidate_raw >> 5) & 3) == 0
                and ((candidate_raw >> 28) & 15) == 14
            ):
                load_pc = candidate
                load_raw = candidate_raw
                break
            writes = conservative_writes(candidate_raw)
            if writes is None or writes & {rn, rm, 15}:
                break
        if load_pc is None or load_raw is None:
            continue
        base_register = (load_raw >> 16) & 15
        value_register = (load_raw >> 12) & 15
        if {rn, rm} != {base_register, value_register} or base_register == value_register:
            continue
        base = static_register_before(inst, reachable, load_pc, base_register)
        if base is None:
            continue
        words: list[int] = []
        site_targets: list[int | None] = []
        for word in range(base, base + 256 * 4, 4):
            if not (BASE <= word < BASE + TEXT_ALLOCATED_BYTES) or word in reachable:
                break
            offset = struct.unpack_from("<I", code, word - BASE)[0]
            if offset == 0:
                words.append(word)
                site_targets.append(None)
                continue
            target = (base + offset) & 0xFFFFFFFF
            if (
                not (BASE <= target < BASE + TEXT_ALLOCATED_BYTES)
                or target == word
                or target not in inst
                or ((inst[target][0] >> 28) & 15) != 14
                or inst[target][1] == "<unknown>"
            ):
                break
            words.append(word)
            site_targets.append(target)
        while words and struct.unpack_from("<I", code, words[-1] - BASE)[0] == 0:
            words.pop()
            site_targets.pop()
        if sum(target is not None for target in site_targets) >= 2 and len(words) < 256:
            sites.append(pc)
            table_words.update(words)
            targets.update(target for target in site_targets if target is not None)
    return targets, table_words, sites


def initializer_roots(code: bytes) -> list[int]:
    roots=[]
    for slot in range(INIT_BEGIN,INIT_END,4):
        raw=struct.unpack_from('<I',code,slot-BASE)[0]
        disp=raw-0x100000000 if raw&0x80000000 else raw
        target=(slot+disp)&0xffffffff
        if target%4 or not (BASE<=target<BASE+TEXT_BYTES): raise ValueError(f'bad initializer root 0x{slot:08X}->0x{target:08X}')
        roots.append(target)
    if len(roots)!=296: raise AssertionError('initializer root count drifted')
    return roots


def absolute_pointer_roots(code: bytes) -> set[int]:
    out=set()
    for off in range(0,len(code)-3,4):
        value=struct.unpack_from('<I',code,off)[0]
        if value%4==0 and BASE<=value<BASE+TEXT_BYTES: out.add(value)
    return out


def walk(inst: dict[int,tuple[int,str,str]], roots: list[int]) -> dict:
    todo=deque(roots); queued=set(roots); seen=set(); starts=set(); unknown=[]; indirect=[]; direct_edges=0; lr_returns=set()
    while todo:
        start=todo.popleft()
        if start in seen or start not in inst: continue
        starts.add(start); pc=start
        while BASE<=pc<BASE+TEXT_BYTES and pc in inst and pc not in seen:
            seen.add(pc); raw,mn,op=inst[pc]
            if mn=='<unknown>': unknown.append(pc); break
            bk=branch_kind(mn)
            if bk:
                kind,conditional=bk; target=direct_target(op)
                if target is not None and BASE<=target<BASE+TEXT_BYTES and target%4==0:
                    direct_edges+=1
                    if target not in queued: todo.append(target);queued.add(target);starts.add(target)
                if kind=='branch' and not conditional: break
                pc+=4; starts.add(pc); continue
            if mn=='blx':
                target=direct_target(op)
                if target is not None and BASE<=target<BASE+TEXT_BYTES and target%4==0 and target not in queued:
                    todo.append(target);queued.add(target);starts.add(target)
                pc+=4; starts.add(pc); continue
            if indirect_terminator(mn,op):
                indirect.append((pc,mn,op))
                ret=explicit_lr_return(inst,seen,pc)
                if ret is not None and ret not in queued:
                    todo.append(ret);queued.add(ret);starts.add(ret);lr_returns.add(ret)
                cond=(raw>>28)&0xf
                if cond not in (0xe,0xf):
                    fallthrough=pc+4
                    if BASE<=fallthrough<BASE+TEXT_BYTES and fallthrough%4==0 and fallthrough not in queued:
                        todo.append(fallthrough);queued.add(fallthrough);starts.add(fallthrough)
                break
            pc+=4
    starts={x for x in starts if x in seen}
    return {'seen':seen,'starts':starts,'unknown':unknown,'indirect':indirect,'direct_edges':direct_edges,'lr_returns':lr_returns}


def classify(raw: int, mn: str) -> tuple[str,str,int,str]:
    cond=(raw>>28)&0xf; flags=0
    if (raw&0x0f000000)==0x0f000000: return ('Svc',CONDITIONS[cond],flags,'fast_path')
    if cond!=0xf and (raw&0x0e000000)==0x0a000000:
        if raw&0x01000000: flags|=4
        return ('Branch',CONDITIONS[cond],flags,'fast_path')
    if (raw&0x0ffffff0) in (0x012fff10,0x012fff30):
        if (raw&0x0ffffff0)==0x012fff30: flags|=4
        return ('BranchReg',CONDITIONS[cond],flags,'fast_path')
    if (raw&0x0c000000)==0:
        opcode=(raw>>21)&15; immediate=bool(raw&(1<<25)); operand2=raw&0xfff
        if (immediate or not (operand2&0xff0)) and opcode in (2,4,10,13):
            if raw&(1<<20) or opcode==10: flags|=1
            if immediate: flags|=2
            name={2:'Sub',4:'Add',10:'Cmp',13:'MovImm' if immediate else 'MovReg'}[opcode]
            return (name,CONDITIONS[cond],flags,'fast_path')
    if (raw&0x0c000000)==0x04000000:
        immediate_offset=not bool(raw&(1<<25)); pre=bool(raw&(1<<24)); byte=bool(raw&(1<<22)); wb=bool(raw&(1<<21)); load=bool(raw&(1<<20))
        if immediate_offset and pre and not byte and not wb:
            if not raw&(1<<23): flags|=32
            return ('Ldr32' if load else 'Str32',CONDITIONS[cond],flags,'fast_path')
    if vfp_family(mn,VFP_TRANSPORT): return ('VfpTransport',CONDITIONS[cond],flags,'vfp_transport')
    if vfp_family(mn,VFP_SCALAR): return ('VfpScalar',CONDITIONS[cond],flags,'vfp_scalar')
    base=strip_cond(mn)
    if base in SYSTEM: return ('CoreSystem',CONDITIONS[cond],flags,'core_system')
    if base.startswith(MEM_PREFIX): return ('CoreMemory',CONDITIONS[cond],flags,'core_memory')
    if base.startswith('v'): return ('Unsupported',CONDITIONS[cond],flags,'unsupported_vfp_simd')
    if mn!='<unknown>': return ('CoreAlu',CONDITIONS[cond],flags,'core_alu_candidate')
    return ('Unsupported',CONDITIONS[cond],flags,'unknown')


def ends(mn: str, op: str) -> bool:
    return bool(branch_kind(mn) or mn=='blx' or indirect_terminator(mn,op) or mn.startswith('svc') or mn=='<unknown>')


def make_blocks(inst: dict, seen: set[int], starts: set[int]) -> list[list[int]]:
    blocks=[];cur=[];prev=None
    for pc in sorted(seen):
        if cur and (pc!=prev+4 or pc in starts or ends(inst[prev][1],inst[prev][2]) or (pc//PAGE_SIZE)!=(prev//PAGE_SIZE)):
            blocks.append(cur);cur=[]
        cur.append(pc);prev=pc
    if cur: blocks.append(cur)
    return blocks


def page_symbol(address:int)->str: return f'kLegoPage{address>>12:05X}Shard'
def page_name(address:int)->str: return f'lego_page_{address>>12:05X}.cpp'


def render_page(address:int, blocks:list[list[int]], inst:dict, classified:dict)->str:
    sym=f'Page{address>>12:05X}'; lines=['#include "lego_aot_generated.h"','','namespace oot3d::recomp {','']
    ops=[pc for block in blocks for pc in block]
    if ops:
        lines.append(f'static constexpr a32::PackedOp kLego{sym}Ops[] = {{')
        for pc in ops:
            raw=inst[pc][0]; opcode,condition,flags,_=classified[pc]
            lines.append(f'    {{0x{raw:08X}U, a32::EncodeMetadata(a32::Opcode::{opcode}, a32::Condition::{condition}, 0x{flags:02X}U)}},')
        lines += ['};','',f'static const a32::Block kLego{sym}Blocks[] = {{']
        offset=0
        for block in blocks:
            lines.append(f'    {{0x{block[0]:08X}U, kLego{sym}Ops + {offset}U, {len(block)}U}},');offset+=len(block)
        lines += ['};','',f'extern const a32::BlockShard {page_symbol(address)} = {{0x{address:08X}U, 0x{address+PAGE_SIZE-4:08X}U, kLego{sym}Blocks, {len(blocks)}U}};']
    else:
        lines.append(f'extern const a32::BlockShard {page_symbol(address)} = {{0x{address:08X}U, 0x{address+PAGE_SIZE-4:08X}U, nullptr, 0U}};')
    lines += ['','}  // namespace oot3d::recomp','']
    return '\n'.join(lines)


def emit(code: bytes, inst:dict, flow:dict, init:list[int], pointers:set[int], output:Path)->dict:
    blocks=make_blocks(inst,flow['seen'],flow['starts'])
    classified={pc:classify(inst[pc][0],inst[pc][1]) for pc in flow['seen']}
    bypage={BASE+i*PAGE_SIZE:[] for i in range(TEXT_PAGES)}
    for block in blocks: bypage[block[0]&~(PAGE_SIZE-1)].append(block)
    if output.exists(): shutil.rmtree(output)
    output.mkdir(parents=True)
    (output/'lego_aot_generated.h').write_text('#pragma once\n#include "recomp/a32_runtime.h"\nnamespace oot3d::recomp { const a32::Registry& GetA32GeneratedRegistry() noexcept; }\n')
    funcs=['#include "lego_aot_generated.h"','#include <cstdint>','namespace oot3d::recomp {','extern const a32::Function kLegoAotFunctions[] = {',f'    {{0x{BASE:08X}U, 0x{BASE+TEXT_BYTES:08X}U, "LEGOCITY_text"}},']
    for i,pc in enumerate(init): funcs.append(f'    {{0x{pc:08X}U, 0x{pc+4:08X}U, "initializer_{i:03d}"}},')
    for pc in OBSERVED_CALLBACKS: funcs.append(f'    {{0x{pc:08X}U, 0x{pc+4:08X}U, "observed_callback_{pc:08X}"}},')
    funcs += ['};',f'extern const std::uint32_t kLegoAotFunctionCount = {1+len(init)+len(OBSERVED_CALLBACKS)}U;','}']
    (output/'lego_aot_functions.cpp').write_text('\n'.join(funcs)+'\n')
    registry=['#include "lego_aot_generated.h"','#include <array>','#include <cstdint>','namespace oot3d::recomp {']
    for address in bypage: registry.append(f'extern const a32::BlockShard {page_symbol(address)};')
    registry += ['extern const a32::Function kLegoAotFunctions[];','extern const std::uint32_t kLegoAotFunctionCount;',f'static const std::array<a32::BlockShard, {TEXT_PAGES}> kLegoPages = {{']
    registry += [f'    {page_symbol(address)},' for address in bypage]
    registry += ['};','const a32::Registry& GetA32GeneratedRegistry() noexcept {','    static const a32::Registry r = {kLegoPages.data(), static_cast<std::uint32_t>(kLegoPages.size()), kLegoAotFunctions, kLegoAotFunctionCount};','    return r;','}','}']
    (output/'lego_aot_registry.cpp').write_text('\n'.join(registry)+'\n')
    page_meta=[]
    for address,pblocks in bypage.items():
        source=render_page(address,pblocks,inst,classified); path=output/page_name(address);path.write_text(source)
        page_meta.append({'address':f'0x{address:08X}','source':path.name,'blocks':len(pblocks),'ops':sum(map(len,pblocks)),'sha256':hashlib.sha256(source.encode()).hexdigest()})
    categories={}
    for value in classified.values(): categories[value[3]]=categories.get(value[3],0)+1
    meta={'format':'lego_chase_llvm_conservative_pages_v1','authority':'diagnostic conservative graph; not Recovery J equivalence','code_sha256':hashlib.sha256(code).hexdigest(),'roots':{'entry':1,'initializers':len(init),'observed_callbacks':len(OBSERVED_CALLBACKS),'aligned_absolute_code_pointer_targets':len(pointers)},'counts':{'instruction_slots':len(flow['seen']),'block_starts':len(flow['starts']),'emitted_blocks':len(blocks),'pages':TEXT_PAGES,'nonempty_pages':sum(bool(v) for v in bypage.values()),'unsupported_ops':sum(v[0]=='Unsupported' for v in classified.values()),'indirect_stops':len(flow['indirect']),'unknown_stops':len(set(flow['unknown']))},'categories':dict(sorted(categories.items())),'historical_intermediate_reference':{'instruction_slots':547756,'blocks':111312},'pages':page_meta}
    (output/'lego_aot_pages.json').write_text(json.dumps(meta,indent=2)+'\n')
    records=[]
    for path in sorted(output.iterdir()):
        if path.name=='manifest.json': continue
        data=path.read_bytes();records.append({'path':path.name,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()})
    manifest={'format':'lego_chase_conservative_generated_manifest_v1','generated_files_excluding_manifest':len(records),'generated_files_including_manifest':len(records)+1,'counts':meta['counts'],'categories':meta['categories'],'files':records}
    (output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    if len(records)+1!=RECOVERY_ARTIFACTS: raise AssertionError('artifact count drifted')
    return manifest


def main()->int:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('code',type=Path)
    ap.add_argument('output',type=Path)
    ap.add_argument('--clang',default='clang')
    ap.add_argument('--llvm-objdump',default='llvm-objdump')
    args=ap.parse_args()
    if args.code.stat().st_size!=EXPECTED_CODE_BYTES or file_sha256(args.code)!=EXPECTED_CODE_SHA256: ap.error('wrong code.bin revision')
    code=args.code.read_bytes(); init=initializer_roots(code); pointers=absolute_pointer_roots(code)
    with tempfile.TemporaryDirectory(prefix='lego-aot-llvm-') as tmp:
        dis=build_disassembly(args.code,Path(tmp),args.clang,args.llvm_objdump); inst=parse_disassembly(dis)
    roots=list(dict.fromkeys([BASE,*init,*OBSERVED_CALLBACKS,*sorted(pointers)]))
    flow=walk(inst,roots)
    switch_targets, switch_words, switch_sites = base_relative_switch_targets(inst,code,flow["seen"])
    if switch_targets:
        flow=walk(inst,list(dict.fromkeys([*roots,*sorted(switch_targets)])))
    flow["base_relative_switch_sites"]=len(switch_sites)
    flow["base_relative_switch_targets"]=len(switch_targets)
    manifest=emit(code,inst,flow,init,pointers,args.output)
    print(json.dumps({'files':manifest['generated_files_including_manifest'],'counts':manifest['counts'],'categories':manifest['categories']},indent=2))
    return 0

if __name__=='__main__': raise SystemExit(main())
