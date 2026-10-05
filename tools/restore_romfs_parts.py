#!/usr/bin/env python3
"""Restore the prepared raw RomFS from its private, uncompressed Library parts.
No original 7z/CCI extraction is needed. Refuses to overwrite an existing file.
The manifest must be the verified romfs-parts.json alongside the two parts.
"""
from __future__ import annotations
import argparse,hashlib,json
from pathlib import Path
from verify_romfs import EXPECTED_BYTES,EXPECTED_SHA256

def restore(manifest_path: Path, output: Path) -> None:
    manifest=json.loads(manifest_path.read_text(encoding='utf-8'))
    if manifest['raw_bytes']!=EXPECTED_BYTES or manifest['raw_sha256']!=EXPECTED_SHA256:
        raise ValueError('manifest does not identify the verified USA RomFS')
    parts=manifest['parts']
    if not isinstance(parts,list) or len(parts)!=2:raise ValueError('expected two raw parts')
    total=0; paths=[]
    for part in parts:
        name=part['name']
        if not isinstance(name,str) or Path(name).name!=name or name in ('.','..'):
            raise ValueError('part must be a local basename')
        p=manifest_path.parent/name
        if p.is_symlink() or not p.is_file() or p.stat().st_size!=part['bytes']:
            raise ValueError(f'part missing or wrong size: {name}')
        if part['offset']!=total:raise ValueError('part offsets are not contiguous')
        total+=part['bytes'];paths.append((p,part))
    if total!=EXPECTED_BYTES:raise ValueError('wrong combined size')
    # Input failure leaves only our newly-created incomplete output; it is never
    # reported as restored. Do not delete unknown files or overwrite old inputs.
    digest=hashlib.sha256()
    with output.open('xb') as out:
        for path,part in paths:
            part_hash=hashlib.sha256();count=0
            with path.open('rb') as stream:
                while chunk:=stream.read(4*1024*1024):
                    count+=len(chunk);part_hash.update(chunk);digest.update(chunk);out.write(chunk)
            if count!=part['bytes'] or part_hash.hexdigest()!=part['sha256']:
                raise ValueError(f'part checksum mismatch: {path.name}; output is incomplete')
    if digest.hexdigest()!=EXPECTED_SHA256:raise ValueError('combined checksum mismatch')

def main()->int:
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('manifest',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
    try:restore(a.manifest,a.output)
    except (OSError,ValueError,KeyError,TypeError) as e:p.error(str(e))
    print(f'Verified prepared RomFS: {a.output} ({EXPECTED_BYTES} bytes, {EXPECTED_SHA256})');return 0
if __name__=='__main__':raise SystemExit(main())
