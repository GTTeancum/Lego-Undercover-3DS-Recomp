#!/usr/bin/env bash
# Fetch 3dsrecomp and Zakuro at the commits pinned in config/upstream.json,
# apply local patches, and build both into work/.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$ROOT/work"
mkdir -p "$WORK"

pin() { python3 -c "import json,sys;print(json.load(open('$ROOT/config/upstream.json'))[sys.argv[1]][sys.argv[2]])" "$1" "$2"; }

checkout() {
  local name="$1" dir="$WORK/$1" url commit
  url="$(pin "$name" url)"; commit="$(pin "$name" commit)"
  if [ ! -d "$dir/.git" ]; then
    git init -q "$dir"
    git -C "$dir" remote add origin "$url"
  fi
  git -C "$dir" fetch -q --depth 1 origin "$commit"
  git -C "$dir" checkout -q --force "$commit"
  git -C "$dir" clean -qfd
  shopt -s nullglob
  for p in "$ROOT/patches/$name"/*.patch; do
    echo "applying $(basename "$p") to $name"
    git -C "$dir" apply "$p"
  done
  shopt -u nullglob
}

checkout 3dsrecomp
checkout zakuro

(cd "$WORK/3dsrecomp" && cargo build --release)
(cd "$WORK/zakuro" && cargo build --release -p zakuro)
echo "3dsrecomp: $WORK/3dsrecomp/target/release/3dsrecomp"
echo "zakuro:    $WORK/zakuro/target/release/zakuro"
