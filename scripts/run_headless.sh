#!/usr/bin/env bash
# Headless run with screenshots and an optional input script.
# Usage: scripts/run_headless.sh <frames> [input script] [extra zakuro args...]
# Output: work/runs/<timestamp>/ with PNGs, log and sheet.png.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FRAMES="$1"; shift
INPUT="${1:-}"; [ $# -gt 0 ] && shift
ROM="$(ls "$ROOT"/game/*.cci "$ROOT"/game/*.3ds 2>/dev/null | head -1)"
OUT="$ROOT/work/runs/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$OUT" "$ROOT/work/data"
export ZAKURO_DUMP_DIR="$OUT" ZAKURO_DUMP_EVERY="${ZAKURO_DUMP_EVERY:-180}"
[ -n "$INPUT" ] && export ZAKURO_INPUT="$(realpath "$INPUT")"
"$ROOT/work/zakuro/target/release/zakuro" "$ROM" --headless "$FRAMES" \
  --rasterizer software --mute --data "$ROOT/work/data" "$@" 2>&1 | tee "$OUT/log.txt" | grep -v ' INFO '
python3 -I "$ROOT/scripts/contact_sheet.py" "$OUT" "$OUT/sheet.png"
echo "sheet: $OUT/sheet.png"
