import base64, gzip, hashlib, json, os, pathlib, subprocess, sys
base='5e7b03ed29724a7647c2b86cfedf4e99cb9b70c7'
expected_tree='844a50596d85592126c7657b42ea2e9a81afde38'
gzsha='447da728f1473d52d555ccbb856ac550aad58679982f8eabbf91d5d6a1a43b65'
patchsha='eed8b9e750528f77307238fd1636c31acbae410801252847723f324c902ceeb8'
source=pathlib.Path(sys.argv[1]).resolve(); candidate=pathlib.Path(sys.argv[2]).resolve()
receipt=pathlib.Path('receipt'); receipt.mkdir(exist_ok=True)
raw=source.read_bytes(); (receipt/'received.gz').write_bytes(raw)
# The first rejected payload was downloaded and compared byte-for-byte with
# the original local patch. These two exact base64 edits reconstruct that patch;
# both hashes and the complete resulting Git tree must still match before use.
encoded=base64.b64encode(raw).decode()
assert len(encoded)==11248 and encoded[414:416]=='it'
encoded=encoded[:414]+'d'+encoded[416:]+'A'
corrected=base64.b64decode(encoded,validate=True)
assert hashlib.sha256(corrected).hexdigest()==gzsha, 'Compressed patch identity mismatch; no candidate published'
patch=gzip.decompress(corrected)
assert len(patch)==31834 and hashlib.sha256(patch).hexdigest()==patchsha
patchpath=(receipt/'implementation.patch').resolve(); patchpath.write_bytes(patch)
def git(*args): return subprocess.check_output(['git',*args],cwd=candidate,text=True).strip()
assert git('rev-parse','HEAD')==base
git('apply','--check',str(patchpath));git('apply','--index',str(patchpath))
assert git('write-tree')==expected_tree, 'Reconstructed source differs from tested tree'
git('config','user.name','LEGO Recomp checkpoint');git('config','user.email','noreply@github.com')
git('commit','-m','Read the real DSP startup reply and translate retained DATA addresses\n\nReturn actual pipe bytes with output preflight, alias and reservation checks.\nPreserve CPU read-pointer ownership and run bounded real mailbox-wait quanta;\npost-commit faults retain partial effects and cannot be retried as rollback.\n\nOriginal code reads 2+30 bytes, translates 30 firmware-returned words and\nreaches untouched CFG sound-output block 0x70001 at round 295. No rendering\nor completed audio initialization is claimed. Local GCC/Clang full builds,\n63 suites in each compiler and ROM-free sanitizers, six startup pairs and\n291 capture pairs passed. This candidate awaits independent hosted checks.')
sha=git('rev-parse','HEAD');branch='work/dsp-pipe-read-candidate-'+os.environ['GITHUB_RUN_ID']
git('push','origin',sha+':refs/heads/'+branch)
(receipt/'publication.json').write_text(json.dumps({'base':base,'tree':expected_tree,'commit':sha,'branch':branch},indent=2))
with open(os.environ['GITHUB_OUTPUT'],'a') as f:f.write('sha='+sha+'\n')
