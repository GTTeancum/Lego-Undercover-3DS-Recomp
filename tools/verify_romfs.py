#!/usr/bin/env python3
"""Verify the exact extracted USA RomFS and all IVFC block hashes, without keys.
The input is prepare_game.py --romfs's raw romfs.bin (including its IVFC prefix).
This verifier never changes the input or extracts/replaces game assets.
"""
from __future__ import annotations
import argparse,hashlib,json,mmap,struct
from pathlib import Path
EXPECTED_BYTES=769179648
EXPECTED_SHA256='6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf'
VIEW_OFFSET=0x1000

def verify(path: Path) -> dict:
    if path.stat().st_size != EXPECTED_BYTES:
        raise ValueError('wrong raw RomFS size')
    with path.open('rb') as f, mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as data:
        if hashlib.sha256(data).hexdigest()!=EXPECTED_SHA256:
            raise ValueError('wrong raw RomFS SHA-256')
        if data[:8]!=b'IVFC\0\0\1\0':
            raise ValueError('unexpected IVFC header')
        master_bytes=struct.unpack_from('<I',data,8)[0]
        levels=[struct.unpack_from('<QQII',data,12+i*24) for i in range(3)]
        if levels != [(0,46592,12,0),(49152,5962208,12,0),(6012928,763161254,12,0)]:
            raise ValueError('unexpected verified layout')
        # Level 3 follows the header/master area. Levels 1/2 follow aligned L3,
        # using their logical offsets. Zero padding is part of each hashed block.
        hash_base=(VIEW_OFFSET+levels[2][1]+4095)&~4095
        offsets=[hash_base+levels[0][0],hash_base+levels[1][0],VIEW_OFFSET]
        parents=[(0x60,master_bytes),(offsets[0],levels[0][1]),(offsets[1],levels[1][1])]
        verified=[]
        for i,(_,length,exp,_) in enumerate(levels):
            block_bytes=1<<exp; count=(length+block_bytes-1)//block_bytes
            parent,parent_bytes=parents[i]
            if count*32!=parent_bytes: raise ValueError('inconsistent IVFC parent extent')
            for block in range(count):
                start=offsets[i]+block*block_bytes
                raw=data[start:start+block_bytes]
                if len(raw)!=block_bytes:raise ValueError('truncated IVFC block')
                if hashlib.sha256(raw).digest()!=data[parent+block*32:parent+(block+1)*32]:
                    raise ValueError(f'IVFC level {i+1}, block {block} failed')
            verified.append({'level':i+1,'physical_offset':offsets[i],'logical_offset':levels[i][0],
                             'data_bytes':length,'block_bytes':block_bytes,'blocks_verified':count})
        header=struct.unpack_from('<10I',data,VIEW_OFFSET)
        if header!=(40,40,12,52,68,120,212,332,4480,4816):raise ValueError('unexpected L3 header')
        # The native loader exposes ALL bytes after the prefix, including the
        # trailing integrity tables, not only the logical level-3 data extent.
        return {'raw_bytes':len(data),'raw_sha256':EXPECTED_SHA256,'view_offset':VIEW_OFFSET,
                'view_bytes':len(data)-VIEW_OFFSET,'level3_data_bytes':levels[2][1],
                'ivfc_levels':verified,'header_u32':header,
                'provenance':'exact USA CCI-derived input; complete raw hash and IVFC hash chain verified'}

def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('romfs',type=Path);parser.add_argument('--report',type=Path)
    args=parser.parse_args()
    try: report=verify(args.romfs)
    except (OSError,ValueError) as error: parser.error(str(error))
    text=json.dumps(report,indent=2)+'\n'
    if args.report:
        with args.report.open('x',encoding='utf-8') as out:out.write(text)
    print(text,end='');return 0
if __name__=='__main__':raise SystemExit(main())
