#!/usr/bin/env python3
"""Package tracked public source and ROM-free hosted test evidence, never game inputs."""
from __future__ import annotations
import hashlib
import io
import json
import os
import re
from pathlib import Path, PurePosixPath
import subprocess
import tarfile
import xml.etree.ElementTree as ET


def git(*args: str) -> bytes:
    return subprocess.check_output(["git", *args])


def expected_test_names() -> set[str]:
    # This repository registers its ROM-free suites in one literal foreach list.
    # Fail closed if that structure changes; do not merely accept any test count.
    loops = re.findall(r"foreach\(test_name\s+([^)]+)\)", Path("CMakeLists.txt").read_text())
    if len(loops) != 1:
        raise ValueError("Expected one literal CTest suite list")
    names = loops[0].split()
    if not names or len(names) != len(set(names)) or any(
        re.fullmatch(r"ctr_[a-z0-9_]+", name) is None for name in names
    ):
        raise ValueError("Invalid or duplicate CTest suite name")
    return set(names)


def main() -> None:
    commit = git("rev-parse", "HEAD").decode().strip()
    tree = git("rev-parse", "HEAD^{tree}").decode().strip()
    members: dict[str, tuple[bytes, int]] = {}
    source_index = []
    for entry in git("ls-files", "--stage", "-z").split(b"\0"):
        if not entry:
            continue
        prefix, raw_path = entry.split(b"\t", 1)
        mode, blob, stage = prefix.decode().split()
        path = raw_path.decode("utf-8")
        rel = PurePosixPath(path)
        if stage != "0" or mode not in {"100644", "100755"}:
            raise ValueError(f"Unsupported index entry: {path!r}")
        if rel.is_absolute() or ".." in rel.parts or Path(path).is_symlink():
            raise ValueError(f"Unsafe source entry: {path!r}")
        data = Path(path).read_bytes()
        actual = hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
        if actual != blob:
            raise ValueError(f"Working source does not match committed blob: {path}")
        members[f"repo/{path}"] = (data, int(mode, 8) & 0o777)
        source_index.append({"path": path, "mode": mode, "blob": blob})

    variants = ("gcc-release", "clang-release", "clang-sanitizers")
    totals = {}
    expected = expected_test_names()
    for variant in variants:
        folder = Path("evidence") / f"lcd-evidence-{variant}"
        if (folder / "source-commit.txt").read_text().strip() != commit:
            raise ValueError(f"Stale evidence: {variant}")
        root = ET.parse(folder / "ctest.xml").getroot()
        cases = root.findall(".//testcase")
        if len(cases) != len(expected) or {case.get("name") for case in cases} != expected:
            raise ValueError(f"CTest suite inventory mismatch: {variant}")
        for case in cases:
            if any(case.find(tag) is not None for tag in ("failure", "error", "skipped")):
                raise ValueError(f"Nonpassing case in {variant}: {case.attrib}")
            if case.get("status", "run") not in {"run", "passed"}:
                raise ValueError(f"Nonpassing CTest status: {case.attrib}")
        totals[variant] = len(cases)
        for item in sorted(folder.rglob("*")):
            if item.is_file() and not item.is_symlink():
                members[f"hosted-test-evidence/{variant}/{item.relative_to(folder).as_posix()}"] = (item.read_bytes(), 0o644)

    archive_name = f"LEGO-Chase-source-checkpoint-{commit[:7]}-HOSTED.tgz"
    handoff_name = f"LEGO-Undercover-3DS-HANDOFF-{commit[:7]}-HOSTED.md"
    receipt = {
        "source_commit": commit, "source_tree": tree,
        "workflow_run": os.environ.get("GITHUB_RUN_ID"),
        "test_counts": totals, "source_files": len(source_index),
        "scope": "ROM-free hosted verification ONLY; no original-game run in this workflow",
        "private_evidence_predecessor": "See canonical handoff for separate private evidence",
    }
    handoff = Path("docs/recovery/CURRENT-REBUILD.md").read_text() + (
        "\n\n## Hosted package receipt\n\n"
        f"Packaged source commit: `{commit}`. Exact tree: `{tree}`.\n"
        f"Workflow run: `{receipt['workflow_run']}`. All three hosted variants passed {len(expected)} suites.\n"
        f"Archive: `{archive_name}`. Handoff: `{handoff_name}`.\n"
        "This package contains public source and new hosted test logs, NOT private game-run captures.\n"
        "Retain the separate private evidence and code/AOT/RomFS backups identified above.\n"
        "No local archive round-trip or full original-game run is asserted by this receipt.\n"
    )
    members[handoff_name] = (handoff.encode(), 0o644)
    members["SOURCE-INDEX.json"] = (json.dumps({"commit": commit, "tree": tree, "files": source_index}, indent=2).encode() + b"\n", 0o644)
    members["HOSTED-RECEIPT.json"] = (json.dumps(receipt, indent=2).encode() + b"\n", 0o644)
    verifier = '''#!/usr/bin/env python3
import hashlib, json, sys, tarfile
with tarfile.open(sys.argv[1], "r:gz") as archive:
    all_members = archive.getmembers()
    if any(not member.isfile() for member in all_members):
        raise SystemExit("Non-regular member")
    names = [member.name for member in all_members]
    if len(names) != len(set(names)):
        raise SystemExit("Duplicate member")
    manifest = json.load(archive.extractfile("CHECKPOINT-MANIFEST.json"))
    expected = {entry["path"] for entry in manifest["files"]}
    if set(names) != expected | {"CHECKPOINT-MANIFEST.json"}:
        raise SystemExit("Member set mismatch")
    for entry in manifest["files"]:
        member = archive.getmember(entry["path"])
        data = archive.extractfile(member).read()
        if len(data) != entry["bytes"] or (member.mode & 0o777) != entry["mode"] or hashlib.sha256(data).hexdigest() != entry["sha256"]:
            raise SystemExit("Mismatch: " + entry["path"])
    print("PASS: verified", len(expected), "manifest members; no extraction performed")
'''
    members["verify_checkpoint.py"] = (verifier.encode(), 0o755)
    manifest = {"format": 1, "files": [{"path": name, "bytes": len(data), "mode": mode, "sha256": hashlib.sha256(data).hexdigest()} for name, (data, mode) in sorted(members.items())]}
    members["CHECKPOINT-MANIFEST.json"] = (json.dumps(manifest, indent=2).encode() + b"\n", 0o644)
    delivery = Path("delivery")
    delivery.mkdir(exist_ok=False)
    archive_path = delivery / archive_name
    with tarfile.open(archive_path, "w:gz") as archive:
        for name, (data, mode) in sorted(members.items()):
            info = tarfile.TarInfo(name)
            info.size = len(data); info.mode = mode; info.mtime = 0
            archive.addfile(info, io.BytesIO(data))
    subprocess.run(["python3", "-c", verifier, str(archive_path)], check=True)
    digest = hashlib.sha256(archive_path.read_bytes()).hexdigest()
    receipt["archive_sha256"] = digest
    receipt["manifest_members"] = len(manifest["files"])
    (delivery / handoff_name).write_text(handoff)
    (delivery / (archive_name + ".sha256")).write_text(f"{digest}  {archive_name}\n")
    (delivery / "HOSTED-VERIFICATION.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps(receipt, indent=2))


if __name__ == "__main__":
    main()
