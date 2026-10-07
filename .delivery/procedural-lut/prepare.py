#!/usr/bin/env python3
"""Reconstruct an exact reviewed public tree and create only an isolated candidate."""
import base64, hashlib, json, lzma, os, pathlib, subprocess
BASE='0799795e30b277ad1a6cc7c05ac624c3110171eb'
TREE='e4b64d3897ed83ea99ab4fde2176879ef32ea89b'
PATCH_SHA='1f4697f97a5d05cdd2357c815f1a75fa39029e99456e88a582ae898b5cade8c6'
here=pathlib.Path(__file__).resolve().parent
repo=pathlib.Path('candidate').resolve()
def git(*args, data=None):
    return subprocess.check_output(['git','-C',str(repo),*args],input=data).decode().strip()
assert git('rev-parse','HEAD') == BASE
parts=[(here/f'part-{i}.b64').read_text() for i in range(4)]
assert [len(p) for p in parts] == [8000,8000,8000,7836]
raw=lzma.decompress(base64.b64decode(''.join(parts),validate=True))
assert len(raw)==81247 and hashlib.sha256(raw).hexdigest()==PATCH_SHA
patch=here/'current.patch';patch.write_bytes(raw)
git('apply','--check','--index',str(patch))
git('apply','--index',str(patch))
assert git('write-tree')==TREE
assert len(git('ls-files','-z').split('\0'))-1 == 476
for key,value in {'GIT_AUTHOR_NAME':'LEGO Recomp checkpoint','GIT_AUTHOR_EMAIL':'noreply@github.com','GIT_COMMITTER_NAME':'LEGO Recomp checkpoint','GIT_COMMITTER_EMAIL':'noreply@github.com'}.items():
    os.environ[key]=value
message='Implement procedural PICA uploads and lossless bounded diagnostic audio sink\n\nThe original 23360-byte setup list commits 512 procedural words and 83 swizzles;\nno draw is implemented. An exclusive-created DSPAUD1 sink preserves actual FIFO\nsamples without raising the fixed in-memory capture capacity. Original startup\nreaches untouched CFG language block 0xA0002 on thread 7 at round 569. Video memory\ncontains a depth clear, not a rendered frame.\n\nFull local GCC/Clang 599-page builds and all 67 tests pass; ROM-free sanitizers pass.\nSix startup pairs, six CLI guards and 194 capture-file pairs match. Private original\ngame inputs remain unchanged and are not included. Candidate requires hosted checks.\n'
sha=git('commit-tree',TREE,'-p',BASE,data=message.encode())
branch='work/proctex-audio-candidate-'+os.environ['GITHUB_RUN_ID']
git('push','origin',sha+':refs/heads/'+branch)
receipt={'commit':sha,'tree':TREE,'parent':BASE,'candidate_branch':branch,'patch_sha256':PATCH_SHA,'source_files':476,'scope':'public source only; no main promotion'}
pathlib.Path('candidate-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
with open(os.environ['GITHUB_OUTPUT'],'a') as out: out.write('sha='+sha+'\n')
print(json.dumps(receipt,indent=2))
