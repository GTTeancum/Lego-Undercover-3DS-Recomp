# LEGO City Undercover: The Chase Begins — 3DS Recomp

A standalone PC build of LEGO City Undercover: The Chase Begins (USA): the
game's code statically recompiled to native code by 3dsrecomp and linked into
the Zakuro 3DS runtime. It boots to gameplay.

This is recompiled emulation, not a source port: the code runs natively but
still works on emulated ARM registers and 3DS memory, and Zakuro still
provides the 3DS's OS services, GPU and audio around it.

## Approach

Built on [3dsrecomp](https://github.com/fearkov/3dsrecomp) and
[Zakuro](https://github.com/fearkov/zakuro):

- 3dsrecomp finds the game's functions and translates them to C.
- Zakuro supplies the 3DS kernel, services, GPU and audio, and runs anything not
  recompiled in its interpreter, so missing code slows the game down instead of
  stopping it.
- `3dsrecomp port` turns the result into a program of its own.

This repo holds the game-specific parts: pinned upstream versions, local
patches, function overrides, scripts and status.

The earlier custom runtime (TriAevum-based, hand-admitted code blocks) is kept
on the [`legacy-triaevum-runtime`](https://github.com/GTTeancum/Lego-Undercover-3DS-Recomp/tree/legacy-triaevum-runtime)
branch.

## Status

| Milestone | State |
|---|---|
| Boots in Zakuro's interpreter | ✅ logos, intro, language select (~frame 840) |
| Title screen / menus | ✅ title, New Game, save slot, autosave |
| Gameplay | ✅ intro cutscenes, then in control in Cherry Tree Hills by ~frame 8100 (interpreter only, headless, software rasterizer) |
| Recompiled build | ✅ 3dsrecomp finds 92.5% of the code (10,778 functions); 62 MB library in ~7 min; reaches Cherry Tree Hills gameplay with 0.02% of instructions interpreted |
| Picture-in-picture layout | ✅ default layout: bottom screen over the top screen's lower right at 22% of its height, inset 10 console px from the corner (minimap-sized) (`patches/zakuro/0002`); checked in an Xvfb window with OpenGL |
| `verify` | ✅ all 10,769 recompiled functions run against the interpreter from the same state: 0 mismatches |
| Standalone port | ✅ `lego-city-undercover` (Linux, 94 MB): fresh save → New Game → intro → Cherry Tree Hills gameplay, headless and in a window (PiP default) |
| Windows build | ✅ cross-compiled from Linux (`scripts/build_port.sh <rom> --target x86_64-pc-windows-gnu`, MinGW): 100 MB exe needing only system DLLs; not yet run on Windows. `build-port.ps1` not yet run on Windows |
| Resolution | ✅ Auto by default (1080p → 5×); 5× rendering checked on Mesa's software Vulkan |

Reproduce: `scripts/run_headless.sh 9000 inputs/boot_to_gameplay.txt --interpreter`,
or after `work/3dsrecomp/target/release/3dsrecomp build game/<rom>.cci work/recomp`,
`scripts/run_headless.sh 9000 inputs/boot_to_gameplay.txt --recompiled work/recomp/00040000000AD500.so`
(about 5 minutes on 4 cores; in the cloud the software rasterizer, not the CPU, is the limit). The only warnings logged were unimplemented
`cecd:u` (StreetPass) commands.

## Layout

- `config/supported_revision.json` — the supported USA ROM (title ID, hashes).
- `config/upstream.json` — pinned 3dsrecomp and Zakuro commits.
- `scripts/setup.sh` — fetch and build both at the pinned commits, applying `patches/`.
- `scripts/verify_rom.sh` — check a ROM against the supported revision.
- `scripts/run_headless.sh` — headless run with screenshots every N frames (`ZAKURO_DUMP_EVERY`) and an optional input script; writes `work/runs/<time>/sheet.png`.
- `inputs/` — scripted input (`<first frame> <last frame> <BUTTON>[+BUTTON]`, or `TOUCH:x,y`).
- `scripts/build_port.sh`, `scripts/windows/build-port.ps1` — build the standalone program.
- `hints/` — addresses Zakuro had to interpret during play, fed to the next build so 3dsrecomp recompiles them (addresses only, no game data).
- `docs/bottom-screen.md` — the code behind the bottom screen: the manager object, its fields and API, and where each screen is loaded; where to hook to hide, change or move them.
- `tools/extract_code.py`, `tools/xrefs.py`, `tools/disasm.py`, `tools/drc_api.py` — reverse-engineering helpers used for that.
- `tools/trace/` — builds a function-entry-counting copy of the recompiled library to find which game functions run during given screens (`tools/trace/build.sh`).
- `patches/` — local changes to 3dsrecomp or Zakuro:
  - `zakuro/0001` headless screenshots and scripted input,
  - `zakuro/0002` picture-in-picture layout (default),
  - `zakuro/0004` started without a ROM (double-click), the program finds its game: the one used last time, then a matching `.cci`/`.3ds`/`.cxi` next to it, in the folder it runs in or a folder inside either, else a file picker; the choice is remembered,
  - `zakuro/0005` a port keeps its saves in `saves/` beside the program (copying earlier saves from the shared data folder once; falling back to it when the folder isn't writable), and controllers use XInput on Windows instead of Windows.Gaming.Input,
  - `zakuro/0006` controller face buttons by letter (3DS A on Xbox A, B on B, X on X, Y on Y); saved settings with the old by-position defaults are moved over once,
  - `zakuro/0008` picture-in-picture hides the bottom screen while the game shows only its idle hexagon screen (cutscenes and scripted scenes), read from the game's bottom-screen manager (see `docs/bottom-screen.md`); a menu checkbox turns it off. Also `ZAKURO_PEEK` for logging guest memory in headless runs,
  - `zakuro/0007` headless trace hook (`ZAKURO_TRACE_LIB`) for `tools/trace`, analysis only,
  - `zakuro/0003` Auto resolution (default): the 3D is drawn tall enough to fill the monitor (1080p → 5×, 1440p → 6×, 4K → 8×) instead of a fixed 3×,
  - `3dsrecomp/0001` `port` writes a Zakuro path that works on Windows,
  - `3dsrecomp/0002` the port is a Windows GUI program, no console window.
- `overrides/` — hand-written C replacements for game functions.
- `game/` — your own decrypted ROM; ignored by Git.

## Building the standalone game

You need your own decrypted dump of the USA cartridge (`.cci`/`.3ds`, SHA-256
in `config/supported_revision.json`), Git, Rust 1.95+ and a C compiler. The
build takes 10–20 minutes.

**Windows** (PowerShell, in the repo folder):

```
powershell -ExecutionPolicy Bypass -File scripts\windows\build-port.ps1 -Rom "C:\path\to\game.cci"
```

Then put `work\port\target\release\lego-city-undercover.exe` next to the ROM
(or in the folder above it) and double-click it. If it doesn't find the game
it asks for the file once and remembers it. A path on the command line still
works too. Saves go in `saves\` next to the exe. Controllers go through
XInput (Xbox and compatible pads; PlayStation/Switch pads need Steam Input or
DS4Windows).

With rustup's GNU toolchain (`rustup default stable-x86_64-pc-windows-gnu`)
it needs MinGW-w64 gcc on PATH; with the default MSVC toolchain it needs
Visual Studio Build Tools plus LLVM (`clang`, `llvm-ar`). See the script's
header.

**Linux** (also needs `pkg-config`, `libasound2-dev`, `libudev-dev`):

```
scripts/build_port.sh game/<rom>.cci
work/port/target/release/lego-city-undercover game/<rom>.cci
```

The program still reads the ROM for everything but the code, so it needs
the ROM around; it finds it as above. It contains the game's code compiled: it is for your own copy and
not something to share.

### Without building a port

```
scripts/setup.sh
work/3dsrecomp/target/release/3dsrecomp build game/<rom>.cci   # installs the library
work/zakuro/target/release/zakuro game/<rom>.cci               # finds it on its own
```

## Picture-in-picture

The default layout. The bottom screen sits over the top screen's lower-right
corner at 22% of the top screen's height (4:3), inset 10 console pixels, sized so the open-world map
reads as a minimap; it stays touchable. Size and inset are `PIP_SHARE` and `PIP_MARGIN` in
`crates/zakuro-gpu/src/backend/mod.rs` (patch 0002). Other layouts are still in
Zakuro's layout menu, or set `layout = "stacked"` etc. in
`~/.config/zakuro/settings.toml` (`%APPDATA%\zakuro\settings.toml` on Windows).

## Resolution

With "Draw the 3D on the GPU" on (the default), the 3D is rendered at a
multiple of the 3DS's 400×240. Resolution defaults to Auto, which picks the
multiple from the height of the monitor the window opens on when the game
starts; set 1–8 in the menu (Graphics) or `resolution = N` in
`settings.toml` to fix it.

## Game data

No game image, RomFS, `code.bin`, generated C, built libraries, saves or
captures are committed. Supply your own legally obtained, decrypted dump.
