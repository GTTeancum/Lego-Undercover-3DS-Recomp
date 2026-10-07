#!/usr/bin/env python3
"""Reconstruct reviewed PUBLIC source only. Never load or download game inputs."""
import base64, hashlib, json, os, pathlib, subprocess, sys, zlib
BASE='9fc4085f346d49dbb1c3f549362a049b08505147'
BASE_TREE='e4b64d3897ed83ea99ab4fde2176879ef32ea89b'
TREE='3167ec7ebe8f69f5abfae950cfb757743ddac98f'
COMPRESSED='2f3321379957366a1f8e83d6f3c7645cd62e64a25af11b42bc0e61f1bad69a10'
PAYLOAD='10b2cda946d3881a9673dc50e361fd348147c568fee929a0b3d5e9da6cc365fb'
source=pathlib.Path(__file__).resolve().parent
root=pathlib.Path(os.environ['RUNNER_TEMP'])/'lego-language-candidate'
def git(*args, cwd=None):
    return subprocess.check_output(['git',*args],cwd=cwd,text=True).strip()
assert git('rev-parse',BASE+'^{tree}')==BASE_TREE
subprocess.run(['git','worktree','add','--detach',str(root),BASE],check=True)
compressed=base64.b64decode(''.join((source/f'part{i}.txt').read_text() for i in range(3)),validate=True)
assert hashlib.sha256(compressed).hexdigest()==COMPRESSED
raw=zlib.decompress(compressed)
assert len(raw)==59950 and hashlib.sha256(raw).hexdigest()==PAYLOAD
records=json.loads(raw);assert len(records)==15
paths=[]
for rec in records:
    rel=pathlib.PurePosixPath(rec['path'])
    assert not rel.is_absolute() and '..' not in rel.parts and str(rel) not in paths
    path=root/rel
    assert not path.is_symlink() and rec['mode'] in ('100644','100755')
    old=path.read_bytes() if path.exists() else b''
    assert (hashlib.sha256(old).hexdigest() if path.exists() else None)==rec['old']
    lines=old.decode('utf-8').splitlines(keepends=True)
    chunks=[]
    for op in rec['ops']:
        if isinstance(op,list):
            assert len(op)==2 and 0<=op[0]<=op[1]<=len(lines)
            chunks.append(''.join(lines[op[0]:op[1]]))
        else:
            assert isinstance(op,str);chunks.append(op)
    data=''.join(chunks).encode('utf-8')
    assert hashlib.sha256(data).hexdigest()==rec['sha256']
    path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
    path.chmod(0o755 if rec['mode']=='100755' else 0o644);paths.append(str(rel))
subprocess.run(['git','add','-f','--',*paths],cwd=root,check=True)
assert git('write-tree',cwd=root)==TREE
message='Read explicit CFG language and compile authenticated missing A32 blocks\n\nAdd independent language selection without inferring console settings. Generate\nthree supplemental blocks (13 original words) only at build time from verified\nprivate code; preserve every original saved AOT page. No runtime decode fallback.\n\nOriginal English startup passes three language reads and both missing locations,\nthen stops at untouched DSP FlushDataCache at round 592. No frame or menu.\n\nFull local GCC/Clang builds and 69 suites passed. ROM-free Clang sanitizers passed\n69 suites; six original-startup compiler pairs and 28 capture-file pairs match.\nAll original inputs retain verified identities. Hosted verification follows on\nthis exact public tree; no private game data is included.'
subprocess.run(['git','-c','user.name=LEGO Recomp checkpoint','-c','user.email=noreply@github.com','commit','-m',message],cwd=root,check=True)
commit=git('rev-parse','HEAD',cwd=root)
assert git('rev-parse','HEAD^',cwd=root)==BASE and git('rev-parse','HEAD^{tree}',cwd=root)==TREE
branch='work/language-aot-candidate-'+os.environ['GITHUB_RUN_ID']
subprocess.run(['git','push','origin','HEAD:refs/heads/'+branch],cwd=root,check=True)
receipt={'base':BASE,'base_tree':BASE_TREE,'tree':TREE,'commit':commit,'branch':branch,'payload_sha256':PAYLOAD,'records':len(records),'scope':'public source only; no private game input or main promotion'}
pathlib.Path('language-candidate.json').write_text(json.dumps(receipt,indent=2)+'\n')
with open(os.environ['GITHUB_OUTPUT'],'a') as out:out.write('commit='+commit+'\n')
print(json.dumps(receipt))
