#!/usr/bin/env bash
# Build a function-entry trace library from the generated C in work/recomp
# (3dsrecomp build game.cci work/recomp). Output: work/trace/00040000000AD500.so
# Use it with scripts/run_headless.sh ... --recompiled work/trace/00040000000AD500.so
# and ZAKURO_TRACE_LIB / ZAKURO_TRACE_OUT set (see patches/zakuro/0001).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
T="$ROOT/work/trace"
rm -rf "$T"
python3 -I "$ROOT/tools/trace/instrument.py" "$ROOT/work/recomp" "$T"
cd "$T"
ls *.c | xargs -P "${JOBS:-3}" -I{} gcc -O2 -ffp-contract=off -fno-math-errno -w -fPIC -fvisibility=hidden \
  --param ggc-min-expand=20 --param ggc-min-heapsize=65536 -I . -c {} -o {}.o
gcc -shared -static-libgcc -o 00040000000AD500.so *.o
echo "$T/00040000000AD500.so"
