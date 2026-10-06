"""Rebuild one exact, reviewed public source tree; publish only a candidate branch."""
from pathlib import Path, PurePosixPath
import base64, hashlib, json, lzma, os, subprocess, sys, tempfile

BASE = '3aca88f93a0305d37463525bca2bbd25893c188b'
BASE_TREE = '368c2b830b8a19a3f10fd2d2fded9ed2672fc440'
TARGET = 'c958adbde76565fe614eb97fdad922895a5f1d91'
PIN = '3d697a18df504f4677b65129d9ab14c7c597e3eb'
RAW_SHA = 'bf535fe1bb781da5fa7da7aa9256138eed65fe59de09fc0d48578baa3445abc7'
COMP_SHA = '1d1ec17daeabf57137b048f3977f205f81270916caedaa83cc8d287bed037630'

def git(root, *args, data=None):
    return subprocess.run(['git', '-C', str(root), *args], input=data,
                          stdout=subprocess.PIPE, check=True).stdout.decode().strip()

def require(ok, message):
    if not ok:
        raise RuntimeError(message)

def safe_path(root, name):
    p = PurePosixPath(name)
    require(not p.is_absolute() and p.parts and '..' not in p.parts and '.git' not in p.parts,
            'unsafe source path')
    out = root.joinpath(*p.parts)
    require(out.resolve().is_relative_to(root.resolve()) and not out.is_symlink(), 'source escape')
    return out

def prepare(records, source, upstream):
    require(git(source, 'write-tree') == BASE_TREE, 'wrong source baseline tree')
    strings = []
    for part in range(3):
        for line in (records / f'part-{part}.txt').read_text().splitlines():
            index, digest, text = line.split('|')
            require(index == f'{len(strings):03d}', 'missing or reordered payload record')
            require(hashlib.sha256(text.encode()).hexdigest()[:12] == digest, 'payload record corruption')
            strings.append(text)
    require(len(strings) == 360, 'wrong record count')
    comp = base64.b64decode(''.join(strings), validate=True)
    require(hashlib.sha256(comp).hexdigest() == COMP_SHA, 'compressed payload hash')
    raw = lzma.decompress(comp, memlimit=128 * 1024 * 1024)
    require(len(raw) == 132891 and hashlib.sha256(raw).hexdigest() == RAW_SHA, 'payload hash')
    p = json.loads(raw)
    require(p['base'] == BASE and p['tree'] == TARGET and p['source_count'] == 438, 'payload identity')
    require(p['vendor']['commit'] == PIN and p['vendor']['repository'] == 'wwylele/teakra', 'vendor identity')
    paths = []
    for entry in p['vendor']['files']:
        src = safe_path(upstream, entry['path'])
        b = src.read_bytes()
        require(hashlib.sha256(b).hexdigest() == entry['sha256'], 'upstream content hash')
        require(hashlib.sha1(b'blob ' + str(len(b)).encode() + b'\0' + b).hexdigest() == entry['blob'], 'upstream Git blob')
        name = 'vendor/teakra-3d697a1/' + entry['path']
        dst = safe_path(source, name)
        require(not dst.exists(), 'vendor source already exists')
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_bytes(b)
        dst.chmod(0o644)
        paths.append(name)
    # git apply performs its own path validation. No --unsafe-paths is used.
    with tempfile.NamedTemporaryFile(suffix='.patch') as patch:
        patch.write(p['patch'].encode()); patch.flush()
        git(source, 'apply', '--check', patch.name)
        git(source, 'apply', patch.name)
    for name, mode in p['modes'].items():
        require(mode in ('100644', '100755'), 'unsupported source mode')
        safe_path(source, name).chmod(0o755 if mode == '100755' else 0o644)
        paths.append(name)
    git(source, 'add', '-f', '--', *sorted(set(paths)))
    tree = git(source, 'write-tree')
    count = len(git(source, 'ls-files').splitlines())
    require(tree == TARGET and count == 438, 'reconstructed source does not match tested tree')
    return {'base': BASE, 'tree': tree, 'source_files': count, 'vendor_pin': PIN}

def main():
    records, source, upstream = map(Path, sys.argv[1:4])
    require(git(source, 'rev-parse', 'HEAD') == BASE, 'wrong baseline commit')
    require(git(upstream, 'rev-parse', 'HEAD') == PIN, 'wrong upstream commit')
    result = prepare(records, source, upstream)
    run = os.environ['GITHUB_RUN_ID']
    require(run.isdecimal(), 'invalid run identifier')
    branch = 'work/dsp-execution-candidate-' + run
    env = os.environ.copy()
    env.update(GIT_AUTHOR_NAME='LEGO Recomp checkpoint', GIT_AUTHOR_EMAIL='noreply@github.com',
               GIT_COMMITTER_NAME='LEGO Recomp checkpoint', GIT_COMMITTER_EMAIL='noreply@github.com')
    message = ('Execute original DSP firmware in a guarded host-only Teakra probe\n\n'
               'Preserve pending special configuration and handshake controller. Import the\n'
               'pinned MIT interpreter with unknown-memory, MMIO and external-bus guards.\n'
               'Known-only and explicit reference-data-reset runs reach distinct real stops;\n'
               'no firmware replies or successful game LoadComponent are fabricated.\n\n'
               'Full local GCC/Clang 599-page builds and all 53 tests passed; ROM-free\n'
               'ASan/UBSan passed 53 tests. Four paired original-startup cases and eleven\n'
               'paired CLI guards passed. Original inputs retain verified identities.\n'
               'This candidate awaits independent hosted checks before main promotion.\n')
    commit = subprocess.run(['git', '-C', str(source), 'commit-tree', TARGET, '-p', BASE],
                            input=message.encode(), env=env, stdout=subprocess.PIPE,
                            check=True).stdout.decode().strip()
    git(source, 'push', 'origin', commit + ':refs/heads/' + branch)
    result.update(commit=commit, branch=branch, workflow_run=run)
    Path('candidate-receipt.json').write_text(json.dumps(result, indent=2) + '\n')
    with open(os.environ['GITHUB_OUTPUT'], 'a') as output:
        output.write('commit=' + commit + '\n')
    print(json.dumps(result, indent=2))

if __name__ == '__main__':
    main()
