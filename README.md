# LEGO City Undercover: The Chase Begins — 3DS Recomp

Goal: a native build of LEGO City Undercover: The Chase Begins (USA) that boots
to gameplay, then a standalone port.

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
| Recompiled build | not yet tested |
| Standalone port | — |

Reproduce: `scripts/run_headless.sh 9000 inputs/boot_to_gameplay.txt --interpreter`
(about 5 minutes on 4 cores). The only warnings logged were unimplemented
`cecd:u` (StreetPass) commands.

## Layout

- `config/supported_revision.json` — the supported USA ROM (title ID, hashes).
- `config/upstream.json` — pinned 3dsrecomp and Zakuro commits.
- `scripts/setup.sh` — fetch and build both at the pinned commits, applying `patches/`.
- `scripts/verify_rom.sh` — check a ROM against the supported revision.
- `scripts/run_headless.sh` — headless run with screenshots every N frames (`ZAKURO_DUMP_EVERY`) and an optional input script; writes `work/runs/<time>/sheet.png`.
- `inputs/` — scripted input (`<first frame> <last frame> <BUTTON>[+BUTTON]`, or `TOUCH:x,y`).
- `patches/` — local changes to 3dsrecomp or Zakuro.
- `overrides/` — hand-written C replacements for game functions.
- `game/` — your own decrypted ROM; ignored by Git.

## Usage

Needs Rust 1.95+, a C compiler, and on Linux `pkg-config`, `libasound2-dev`
and `libudev-dev`.

```
scripts/setup.sh
scripts/verify_rom.sh game/<rom>.cci
work/zakuro/target/release/zakuro game/<rom>.cci
```

## Game data

No game image, RomFS, `code.bin`, generated C, built libraries, saves or
captures are committed. Supply your own legally obtained, decrypted dump.
