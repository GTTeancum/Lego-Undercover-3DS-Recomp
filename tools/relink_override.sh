#!/usr/bin/env bash
# Rebuild one override's body and relink a 3dsrecomp library in seconds, for
# iterating on overrides that already exist (a new override address needs a
# full `3dsrecomp build`, since its callers are compiled to call it).
# Usage: tools/relink_override.sh <build dir> <overrides/name.c>
set -euo pipefail
DIR="$1"; SRC="$2"; NAME="$(basename "$SRC" .c)"
cp "$SRC" "$DIR/hand/$NAME.c"
gcc -O2 -ffp-contract=off -fno-math-errno -w -fPIC -fvisibility=hidden -I "$DIR" -c "$DIR/hand/$NAME.c" -o "$DIR/hand/$NAME.o"
LIB=$(ls "$DIR"/*.so)
gcc -shared -static-libgcc -o "$LIB" "$DIR"/*.o "$DIR"/hand/*.o -lm
echo "relinked $LIB"
