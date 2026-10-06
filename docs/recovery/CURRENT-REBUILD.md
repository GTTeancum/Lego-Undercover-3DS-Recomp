# LEGO Chase Begins — canonical continuation handoff

Continue development in assistant scratch or ROM-free GitHub Actions, NOT on the
user's PC or Work. Project: LEGO City Undercover: The Chase Begins, Nintendo 3DS USA.
POST AN UPDATED DOWNLOADABLE HANDOFF AND SOURCE/EVIDENCE CHECKPOINT EVERY WORK TURN.
No useful game visuals exist in the last verified run. Do not substitute test
patterns, generated art, reference screenshots, or blank buffers for genuine output.

## Current change: LCD blanking (awaiting hosted verification)

This source adds the observed GSP SetLcdForceBlack request, exact header 0x000B0040.
The baseline is main commit da5c9a9e8aa74567deae519380caa86e06e35247, source tree
ea8fb58dae3570e62dbb512d63524d0faa9cebc3. Preserve that private source/evidence archive.
The current implementation has NOT yet been validated in an original-game run.
Do not claim startup has advanced or name a new game blocker without running it.

Local container execution and Python initialization failed before any local command
could run. GitHub connector reads/writes still worked. Development uses a separate
work/lcd-blanking branch and ROM-free hosted tests; no original code.bin, private
AOT pages, ROM, RomFS or private game captures are uploaded to public GitHub/Actions.
The candidate requires GCC, Clang and Clang ASan/UBSan validation before promotion.

## LCD implementation scope

Only a connected client can issue the exact one-normal-word, zero-descriptor IPC.
Pinned IPC Pop<bool> consumes the low byte: (word & 0xFF) != 0. Upper 24 bits do
not enable blanking. SetLcdForceBlack constructs zero RGB plus enable bit 24, and
stores the complete value in BOTH global LCD color-fill control words. The words
are separate from PICA GPU register storage, VRAM and framebuffer descriptors.

Read-only lcd_color_fill_word(screen) exposes top=0/bottom=1; invalid screen indexes
return nullopt. The initial zero words follow pinned PicaCore::RegsLcd regs_lcd{},
not a measured hardware reset image. No CPU LCD MMIO map, backlight, LCD scanout,
frame presentation, draw engine or arbitrary color-fill interface is added.

Successful reply: header 0x000B0040, Result 0, remaining command buffer words zero.
GPU ownership or relay registration is not required by the inspected handler.
The control is shared between sessions. An independent service has independent
state. No event, queue, framebuffer pixels, PICA upload, handle count or time is
changed by this request. Existing full response-capacity/readability preflight
runs before handler dispatch. Unsupported request shapes remain untouched stops.

New ctr_lcd_blank_test covers all 256 low-byte values with and without nonzero
upper bytes, the exact original zero request, enable/disable/repeat, shared and
independent services, nonowner and unregistered clients, duplicated handles,
retained event lifetime, a full handle table, protected/partial/write-only response
areas, malformed headers, every GPU word, complete GSP shared bytes/epochs, upload
storage and a nonzero 6 MiB SYNTHETIC VRAM bank. The test bank is not game artwork.

## Last verified original-game state (inherited, NOT this change's test result)

The da5c9a9 original-game trace stopped with:

```text
display_periods=1 guest_now_ns=16713681
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=232
last_ipc_session=gsp::Gpu request_header=0x000b0040
ipc_words= 000b0040 00000000 00000000 00000000 00000000 00000000 00000000 00000000
```

Argument zero requests disabling force-black, not rendering a new image. The entire
6 MiB device VRAM bank was zero under explicit reference-HLE cold-bank policy.
No meaningful screenshot, rendered frame, main menu, executed shader or gameplay
was established. Existing-gamecoin reached the same call at round 224. It is not
save-readback proof. The baseline's private display-checkpoint/ captures are in the
private da5c9a9 archive; retain them rather than replacing them with hosted test logs.

Earlier verified behavior remains in the baseline: real RomFS metadata reads,
game-created gamecoin.dat, non-drawing PICA uploads, actual byte fill/transfer paths,
recorded A32 callback-return suffixes, P3D/PPF/PSC events and one modeled PDC period.
The original callbacks (not the host) changed the display counters from zero to one.
GPU timing is synchronous; opt-in display timing advances only idle guest time.
These features were not newly implemented or privately reverified in this change.

## Next exact work

Restore the new hosted source checkpoint and the separate private inputs below.
Keep current source distinct from older evidence. Build the full 599-page executable
with GCC and Clang, run all tests, and rerun unchanged game code from a NEW empty
shared archive with every explicit policy enabled. Observe the real SetLcdForceBlack
response, both LCD words, unchanged VRAM and the next actual stop. Do not infer it.
Post a screenshot only once meaningful genuine native output exists.

Source implementation changes are CMakeLists.txt, src/services/gsp_gpu_service.h,
and new tests/ctr_lcd_blank_test.cpp. ROM-free packaging/CI support is in
.github/workflows/rom-free-checkpoint.yml and tools/package_romfree_checkpoint.py.
No vendor backend, scheduling, filesystem, game bytes or private AOT is modified.
The hosted package verifies exact tracked blob identities and test evidence; this
is NOT a full original-game build or a replacement for private trace verification.

## Pinned primary references

azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac:

- src/core/hle/service/gsp/gsp_gpu.cpp: SetLcdForceBlack zero-color object and bool;
  blob 6f915e4d6a5d27853321d0a233103afb9d877bc0.
- src/core/hle/ipc_helpers.h: Pop<bool> uses Pop<u8>() != 0;
  blob 9381ae22c8c1f1a557e16363d5eb7727f9ea1a50.
- src/video_core/gpu.cpp: SetColorFill stores top and bottom LCD color-fill words;
  blob 40f29fea0867b0e8cf4d89a8753ddabf02c3b54b.
- src/video_core/pica/regs_lcd.h: RGB bits 0..23, enable bit 24, separate LCD words;
  blob 0f8a622af300c3576284d704bef8e3bfba72d652.

Pinned semantics are not blanket hardware parity. Pop<bool> reads a byte even
though the IPC occupies a word. Do not replace it with a full-word nonzero test.

## Recovery paths and commands

GitHub: GTTeancum/Lego-Undercover-3DS-Recomp. Library: /LEGO-Chase-Recovery/.
The new hosted package contains repo/ plus hosted-test-evidence/, SOURCE-INDEX.json,
CHECKPOINT-MANIFEST.json, verify_checkpoint.py and this handoff. The package receipt
records its exact commit, source tree, run and filenames. It intentionally excludes
all private original-game captures and binary assets. Retrieve the private predecessor
LEGO-Chase-source-checkpoint-da5c9a9.tgz for display-checkpoint/ and nested earlier
source/evidence. Extract historical material separately, never over new repo/.

Target scratch root: /mnt/data/lego_recovery/ (not confirmed accessible in this turn).
Current source should be repo/, private AOT generated2/, original code restored/code.bin,
raw RomFS game/prepared-romfs/romfs.bin. Builds build-gcc/, build-clang/, build-asan/.
Private inputs have existing Library backups: code.bin;
LEGO-Chase-current-AOT-599pages-2026-10-03.tgz; Prepared-RomFS/ two uncompressed parts
and romfs-parts.json. Part sizes: 402653184 and 366526464. No CCI re-extraction needed.

```sh
cd /mnt/data/lego_recovery
# Verify the new archive before extraction; retain its exact source index.
python verify_checkpoint.py LEGO-Chase-source-checkpoint-<commit>-HOSTED.tgz
# Restore private AOT under this root to generated2/; code goes in restored/.
mkdir -p game/prepared-romfs
python repo/tools/restore_romfs_parts.py romfs-library-roundtrip/romfs-parts.json game/prepared-romfs/romfs.bin
python repo/tools/verify_romfs.py game/prepared-romfs/romfs.bin
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
mkdir -p private-state
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/private-state/lcd-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty --romfs game/prepared-romfs/romfs.bin --gpu-vram-mode reference-zero --display-clock-mode reference-idle
```

After this change the actual game stop is UNKNOWN until the run is performed.
Omitting display-clock-mode reproduces the inherited wait at round 228/time zero.
Omitting explicit VRAM keeps the earlier strict transfer stop. No RomFS retains
OpenFileDirectly; no explicit empty PTM retains step-count. Preserve those branches.
Restore tools refuse existing output; verify rather than overwrite unknown assets.

Verified PRIVATE INPUT IDENTITIES FROM THE BASELINE (not rehashed in this turn):
RomFS: 769179648 bytes, view offset 4096, view bytes 769175552; keep integrity tables.
SHA 6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf.
Code SHA 5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f.
AOT SHA 2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9.
Historical CCI SHA 3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525.
Six original archive parts in Game-archive/ remain an alternate recovery route.

## Continuing mandate

Do not describe hosted ROM-free tests as a full 599-page executable build. Do not
claim original inputs were rehashed, game startup advanced, local archive recovery
succeeded, or a Library upload happened unless those actions actually complete.
Keep all private original game bytes, AOT and captures out of public GitHub/Actions.
Publish only verified checkpoints; use a freshly checked remote parent and force=false.
Never replace remote history with a reconstructed local Git snapshot. Actions
artifacts expire after 30 days; preserve the new package in Library when possible.
Keep posting this handoff and a recoverable source/evidence attachment every turn.
