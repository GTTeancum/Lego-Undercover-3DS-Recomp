#!/usr/bin/env bash
# Rebuild one override's body and relink a 3dsrecomp library in seconds, for
# iterating on overrides that already exist (a new override address needs a
# full `3dsrecomp build`, since its callers are compiled to call it).
# Usage: tools/relink_override.sh <build dir> <overrides/name.c>
set -euo pipefail
DIR="$1"; SRC="$2"; NAME="$(basename "$SRC" .c)"
cp "$SRC" "$DIR/hand/$NAME.c"
gcc -O2 -ffp-contract=off -fno-math-errno -w -fPIC -fvisibility=hidden -I "$DIR" -c "$DIR/hand/$NAME.c" -o "$DIR/hand/$NAME.o"
# a build that splits the code into fewer files leaves the older ones
# behind; they must not be linked again
for c in "$DIR"/code*.c; do [ "$c" -ot "$DIR/code000.c" ] && rm -f -- "$c" "${c%.c}.o"; done
LIB="$DIR/$(basename "$(ls "$DIR"/*.so 2>/dev/null || echo 00040000000AD500.so)")"
gcc -shared -static-libgcc -o "$LIB" "$DIR"/*.o "$DIR"/hand/*.o -lm
echo "relinked $LIB"
