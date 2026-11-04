#!/usr/bin/env bash
# Build the standalone port on Linux (Windows: scripts/windows/build-port.ps1).
# Usage: scripts/build_port.sh <game.cci> [--target x86_64-pc-windows-gnu]
# Output: work/port (or $PORT_DIR)/target/[<target>/]release/lego-city-undercover[.exe]
# The program contains the game's code compiled: for your own copy only.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ROM="$(realpath "$1")"; shift
"$ROOT/scripts/verify_rom.sh" "$ROM"
[ -x "$ROOT/work/3dsrecomp/target/release/3dsrecomp" ] && [ -d "$ROOT/work/zakuro/.git" ] || "$ROOT/scripts/setup.sh"

# the places Zakuro had to interpret, so this build recompiles them too
LIB="${XDG_DATA_HOME:-$HOME/.local/share}/3dsrecomp"
mkdir -p "$LIB"
cp "$ROOT/hints/00040000000AD500.hints" "$LIB/"

PORT="${PORT_DIR:-$ROOT/work/port}"
rm -rf "$PORT"
if [ "${1:-}" = "--target" ] && [[ "$2" == *windows-gnu ]]; then
  # cross-compiling: the game's C has to be built by MinGW too
  export CC=x86_64-w64-mingw32-gcc AR=x86_64-w64-mingw32-ar
  # the program's icon (assets/icon.ico), a resource linked into the exe
  mkdir -p "$ROOT/work"
  x86_64-w64-mingw32-windres --include-dir "$ROOT/assets" "$ROOT/assets/icon.rc" -O coff -o "$ROOT/work/icon-res.o"
  export CARGO_TARGET_X86_64_PC_WINDOWS_GNU_RUSTFLAGS="${CARGO_TARGET_X86_64_PC_WINDOWS_GNU_RUSTFLAGS:-} -C link-arg=$ROOT/work/icon-res.o"
fi
"$ROOT/work/3dsrecomp/target/release/3dsrecomp" port "$ROM" "$PORT" \
  --zakuro "$ROOT/work/zakuro" --name lego-city-undercover --overrides "$ROOT/overrides"
# build against the same dependency versions Zakuro pins
cp "$ROOT/work/zakuro/Cargo.lock" "$PORT/Cargo.lock"
cd "$PORT" && cargo build --release "$@"
