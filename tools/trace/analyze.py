#!/usr/bin/env python3
"""Relate game functions to what the bottom screen shows.

A traced run (tools/trace) leaves, per window of N frames, the functions
entered (trace.txt: "frame address count") and the bottom screen at the
end of the window (bottom_<frame>.png).

  analyze.py cluster <run dir>
      groups the windows by how the bottom screen looks, writes
      clusters.json and clusters.png (one picture per group) to name them.
  analyze.py correlate <run dir> <labels.json>
      labels.json maps group ids to state names ("idle", "map", ...; groups
      with the same name merge, "?" is left out). Prints, per state, the
      functions entered in nearly every window of it and almost never
      outside it, and the functions entered only where a state begins.
"""
import collections
import json
import pathlib
import sys

from PIL import Image, ImageDraw

THUMB = (16, 12)


def load_trace(run: pathlib.Path) -> dict[int, dict[int, int]]:
    windows = collections.defaultdict(dict)
    with open(run / "trace.txt") as f:
        for line in f:
            frame, address, count = line.split()
            windows[int(frame)][int(address, 16)] = int(count)
    return windows


def thumb(path: pathlib.Path) -> list[int]:
    im = Image.open(path).convert("RGB").resize(THUMB, Image.BILINEAR)
    return [c for px in im.getdata() for c in px]


def cluster(run: pathlib.Path) -> None:
    frames = sorted(int(p.stem.split("_")[1]) for p in run.glob("bottom_*.png"))
    groups: list[dict] = []
    assignment = {}
    for frame in frames:
        t = thumb(run / f"bottom_{frame:06d}.png")
        best = None
        for g in groups:
            d = sum(abs(a - b) for a, b in zip(t, g["thumb"])) / len(t)
            if d < 12 and (best is None or d < best[0]):
                best = (d, g)
        if best is None:
            g = {"id": len(groups), "thumb": t, "frames": []}
            groups.append(g)
        else:
            g = best[1]
        g["frames"].append(frame)
        assignment[frame] = g["id"]
    out = [{"id": g["id"], "count": len(g["frames"]), "first": g["frames"][0], "last": g["frames"][-1],
            "frames": g["frames"]} for g in groups]
    (run / "clusters.json").write_text(json.dumps({"groups": out, "assignment": assignment}))
    cols = 8
    rows = (len(groups) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * 330, rows * 262), (30, 30, 30))
    draw = ImageDraw.Draw(sheet)
    for i, g in enumerate(groups):
        mid = g["frames"][len(g["frames"]) // 2]
        x, y = (i % cols) * 330, (i // cols) * 262
        sheet.paste(Image.open(run / f"bottom_{mid:06d}.png").convert("RGB").resize((320, 240)), (x + 5, y + 20))
        draw.text((x + 5, y + 4), f"#{g['id']} n={len(g['frames'])} {g['frames'][0]}-{g['frames'][-1]}", fill=(255, 255, 0))
    sheet.save(run / "clusters.png")
    print(f"{len(frames)} windows, {len(groups)} groups -> {run / 'clusters.png'}")


def correlate(run: pathlib.Path, labels_path: pathlib.Path) -> None:
    clusters = json.loads((run / "clusters.json").read_text())
    labels = json.loads(labels_path.read_text())
    windows = load_trace(run)
    state_of = {}
    for frame, gid in clusters["assignment"].items():
        name = labels.get(str(gid), "?")
        if name != "?":
            state_of[int(frame)] = name
    frames = sorted(f for f in state_of if f in windows)
    states = sorted(set(state_of[f] for f in frames))
    present = collections.Counter()
    by_state = {s: collections.Counter() for s in states}
    size = collections.Counter(state_of[f] for f in frames)
    for f in frames:
        for a in windows[f]:
            present[a] += 1
            by_state[state_of[f]][a] += 1
    total = len(frames)
    for s in states:
        n = size[s]
        rows = []
        for a, k in by_state[s].items():
            inside = k / n
            outside = (present[a] - k) / max(1, total - n)
            if inside >= 0.8 and outside <= 0.05:
                rows.append((inside - outside, a, inside, outside))
        rows.sort(reverse=True)
        print(f"\n== {s}: {n} windows; functions mostly only here")
        for _, a, inside, outside in rows[:25]:
            print(f"  f_{a:08X}  in {inside:4.0%}  elsewhere {outside:4.0%}")
    # entries at state changes: functions in the window a state begins, rare overall
    print("\n== at state changes (functions in under 3% of all windows)")
    starts = collections.defaultdict(collections.Counter)
    changes = collections.Counter()
    prev = None
    for f in frames:
        s = state_of[f]
        if prev is not None and s != state_of[prev]:
            key = f"{state_of[prev]} -> {s}"
            changes[key] += 1
            for a in set(windows[f]) | set(windows.get(prev, {})):
                if present[a] <= 0.03 * total:
                    starts[key][a] += 1
        prev = f
    for key, n in sorted(changes.items(), key=lambda kv: -kv[1]):
        common = [(c, a) for a, c in starts[key].items() if c >= max(1, n * 0.6)]
        common.sort(reverse=True)
        print(f"  {key} (x{n}): " + " ".join(f"f_{a:08X}({c})" for c, a in common[:12]))


if __name__ == "__main__":
    {"cluster": lambda: cluster(pathlib.Path(sys.argv[2])),
     "correlate": lambda: correlate(pathlib.Path(sys.argv[2]), pathlib.Path(sys.argv[3]))}[sys.argv[1]]()
