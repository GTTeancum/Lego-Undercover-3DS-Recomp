#!/usr/bin/env bash
# Check a ROM against the supported revision. Usage: scripts/verify_rom.sh <game.cci>
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
want="$(python3 -c "import json;print(json.load(open('$ROOT/config/supported_revision.json'))['cci']['sha256'])")"
got="$(sha256sum "$1" | cut -d' ' -f1)"
if [ "$got" = "$want" ]; then echo "OK: supported USA revision"; else echo "MISMATCH: $got (want $want)"; exit 1; fi
