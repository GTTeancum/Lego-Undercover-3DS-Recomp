#!/usr/bin/env python3
"""Reconstruct the reviewed public source; authenticate each base/result and full tree."""
import hashlib,json,lzma,os,subprocess,sys
from pathlib import Path,PurePosixPath
payload=Path(sys.argv[1]).resolve();root=Path(sys.argv[2]).resolve();receipt=Path(sys.argv[3]).resolve()
BASE='b6ea4709af883666f9b0eec9cb0af74b5926f7e2'
BASE_TREE='3167ec7ebe8f69f5abfae950cfb757743ddac98f'
TREE='d3a219c1d973e0a1b90b153184ac989378d52e7c'
def git(*args):return subprocess.check_output(['git','-C',str(root),*args]).decode().strip()
def blob(data):return hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
assert git('rev-parse','HEAD')==BASE
assert git('rev-parse','HEAD^{tree}')==BASE_TREE
z=b''.join((payload/('edits.part'+str(i))).read_bytes() for i in range(3))
assert hashlib.sha256(z).hexdigest()=='885c9ac5d0953be00cb79c6555a155ebac64879f7d3a300625d9461f5a72b778'
raw=lzma.decompress(z)
assert hashlib.sha256(raw).hexdigest()=='b1049f71929d7b40e83253f03d388d57ee51bdbb2367ceea4c76aa8d961e4576'
records=json.loads(raw);assert len(records)==18
seen=set()
for r in records:
 rel=PurePosixPath(r['path']);assert not rel.is_absolute() and '..' not in rel.parts and str(rel)==r['path']
 assert r['path'] not in seen and '.git' not in rel.parts;seen.add(r['path'])
 path=root/r['path'];assert not path.is_symlink()
 if r['before'] is None:assert not path.exists();old=b''
 else:old=path.read_bytes();assert blob(old)==r['before']
 lines=old.decode('utf-8').splitlines(keepends=True);result=[];pos=0
 for start,end,text in r['edits']:
  assert pos<=start<=end<=len(lines);result.extend(lines[pos:start]);result.append(text);pos=end
 result.extend(lines[pos:]);data=''.join(result).encode('utf-8');assert blob(data)==r['after']
 path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data);path.chmod(r['mode'])
 subprocess.run(['git','-C',str(root),'add','-f','--',r['path']],check=True)
assert git('write-tree')==TREE
assert len(git('ls-files').splitlines())==490
subprocess.run(['git','-C',str(root),'diff','--cached','--check'],check=True)
subprocess.run(['git','-C',str(root),'config','user.name','LEGO Recomp checkpoint'],check=True)
subprocess.run(['git','-C',str(root),'config','user.email','noreply@github.com'],check=True)
message='Validate cacheless DSP/GSP flushes and execute bounded VFP short vectors\n\nOriginal startup passes eight DSP flushes, one GSP flush and 58 recorded\nshort-vector-mode operations. Independent finite RN binary32 replay matches\nall 214 lane operations. The next 1376-byte GPU list stops uncommitted at\ndefault/immediate attributes (0x232), round 843. No rendered frame or menu.\n\nFull local GCC/Clang 599-page builds plus authenticated supplement passed\nall 72 suites; ROM-free Clang ASan/UBSan passed all 72. Six original-startup\ncompiler pairs and 69 final capture pairs match. Game inputs stay unchanged.\nHosted checks follow on this exact reviewed public tree. No private game data.\n'
commit=subprocess.check_output(['git','-C',str(root),'commit-tree',TREE,'-p',BASE],input=message.encode()).decode().strip()
branch='work/cache-vfp-candidate-'+os.environ['GITHUB_RUN_ID']
subprocess.run(['git','-C',str(root),'push','origin',commit+':refs/heads/'+branch],check=True)
receipt.mkdir(parents=True,exist_ok=True)
(receipt/'candidate.json').write_text(json.dumps({'commit':commit,'tree':TREE,'parent':BASE,'branch':branch,'source_files':490,'all_base_result_blob_checks':True},indent=2)+'\n')
with open(os.environ['GITHUB_OUTPUT'],'a') as f:f.write('commit='+commit+'\n')
print('Verified candidate',commit,TREE)
