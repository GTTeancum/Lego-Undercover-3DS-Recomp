# The bottom screen: what draws it and where to hook it

Map of the code behind the 3DS bottom screen in LEGO City Undercover: The
Chase Begins (USA, `00040000000AD500`), worked out with the tools in
`tools/`. Addresses are the game's (guest) addresses; `f_XXXXXXXX` is the
function 3dsrecomp generates for an address, which is also what an override
replaces (`RECOMP_OVERRIDE(0xXXXXXXXX)`, see 3dsrecomp's `docs/overrides.md`).

Names in **bold** are ours, inferred from strings, fields and behaviour; the
game ships without symbols. Confidence is noted where it matters.

## The manager ("DRC")

The bottom screen is the Wii U version's GamePad screen ("DRC", Display
Remote Controller) carried over. One object runs it:

- **Global:** `0x00519D94` (static, so the same address every run).
- **Vtable:** `0x00361D70`–`0x00361DBC`. The class-specific entries (the
  rest are the base class's empty stubs at `0x00339Fxx`):

| Vtable slot | Function | Role |
|---|---|---|
| `0x00361D74` | `f_002DB998` | not yet identified |
| `0x00361D80` | `f_002DC270` | small (6 instructions) |
| `0x00361D9C` | `f_002D93D0` | uses the dialog box sprites (+47C/+480): **draws the dialogue box** (likely) |
| `0x00361DA4` | `f_002DDD64` | **per-frame update**: wipe, button bar, dialog timing |
| `0x00361DB4` | `f_002DDA48` | **load**: calls `f_002DC284`, which builds everything below |

- **Setup:** `f_002DC284` (0x17C4 bytes, several functions 3dsrecomp
  treats as one) loads the map (`sprites/UI_Bottomscreen/Maps/…`,
  `minimapscene`, `camera`, player icon), the map buttons
  (`Sprites/blend/mapButtons/…`, portraits per character type), the dialog
  sprites and the `DRC_Wipe` scene, and stores animation handles by name
  (`f_0023CADC` finds an animation in a scene).
- **Unload/reset:** `f_002DD5EC` touches the same fields (+400–414, +42C,
  +430, +47C/480, +488–494): likely teardown.

### Fields

| Offset | What | Evidence |
|---|---|---|
| +024 | pointer to a second state object (its +369, +9A, +85, +84 are read with the wipe) | update, setters |
| +0D0, +0D4 | `minimapscene`, `camera` | setup |
| +3D4 / +3D8 | `charsOpen` / `charsClose` animations | setup |
| +3DC / +3E0 | `fastTravelOn` / `fastTravelOff` | setup |
| +3E4 / +3E8 | Scan button `ScanOnDelay` / `ScanOff` | setup |
| +3EC / +3F0 | Chars button `CharsOnDelay` / `CharsOff` | setup |
| +3F4 / +3F8 | Red Bricks button `RedBricksOnDelay` / `RedBricksOff` | setup |
| +3FC / +400 | Audio button `AudioOnDelay` / `AudioOff` | setup |
| +404 / +408 | Exit button `ExitOnDelay` / `ExitOff` | setup |
| +40C / +410 | Free Play button `FreeplayOnDelay` / `FreeplayOff` | setup |
| +414 | `HintOff` | setup |
| +424, +428, +42C, +430 | set by `f_002631C8`, `f_0027BAC4`, `f_0019465C`, `f_002C2650` (+424/+428 also set +024→+369) | setters |
| +472–+478 | per-button shown flags (+476 Scan, +477 Red Bricks, +478 Free Play; +472/+473/+474/+475 bar/fast travel) | button functions below |
| +47C / +480 | `UI_Dialog_A.tga` / `UI_Dialog_B.tga` | setup |
| +484, +499, +49A | one-byte flags set by `f_0019464C`, `f_001A21DC`, `f_00187F28` (callers not yet traced) | setters |
| +485 | **wipe shut**: 1 while the hexagon wipe covers the bottom screen (the idle badge screen) | update; checked against screenshots, 178/181 frames agree, the 3 others mid-animation |
| +488 | `DRC_Wipe` scene | setup |
| +490 / +494 | wipe `TransitionOn` (close) / `TransitionOff` (open) | setup |
| +498 | **wipe countdown**: while non-zero the update keeps the wipe shut; it counts down one per frame | update |
| +49B, +49C, +49D, +4A4 | wipe/bar timing flags | update |
| +4DC | `TEXT_AreaName` | setup |
| +544, +548 | set together by `f_00290FE8` | setter |

### The idle badge screen (hexagon wipe)

In `f_002DDD64` each frame:

1. If `f_00236104()` is true, +498 = 10; otherwise +498 counts down.
   `f_00236104` is a predicate used by ~30 functions: true while a cutscene
   or scripted sequence owns the camera (checks a scene-camera global at
   `0x0037682C` against `0x0037680C`/`0x00376808`, or `f_002297B0(0)`).
2. With +498 non-zero it stops `TransitionOff`, plays `TransitionOn`
   (`f_00245B40` plays an animation, `f_00252828` stops one) and sets +485.
3. When +498 reaches 0 it plays `TransitionOff` and clears +485.

`f_001EF45C` (**DRC_SetWipe**) writes its argument to +498 directly; the
crane minigame (`f_002D7570`, `f_002D7FD0`, vtable `0x00361C2C`) uses it.

**Used by:** Zakuro patch 0008 reads +485 each frame and leaves the
picture-in-picture bottom screen out while it is set
(`crates/zakuro/src/title_hooks.rs`).

**The map showing:** `f_001EE730` is the map's per-frame function (in
traces: every map window, no pause/tips/wipe window).
`overrides/map_frame_counter.c` counts its runs at `0x005A4F00` (unused end
of the page holding the end of `.bss`); Zakuro counts the map as showing
while that moved within the last 4 frames (the game logic skips frames),
the map fade at +0x4A0 is 1.0 and the wipe is open, and then draws the
picture-in-picture bottom screen round (patch 0010).

**Notices** (postcard collected, tutorial tips, script `ShowTipTextOnce`/
`ShowTipTextTimed`): `f_00235F30(text, colour, …, seconds)` shows one; the
state is at `0x00517E68`, +0x10 non-zero while one is up (the game's own
check is `f_001EE9A0`), +0x04 the notice object, +0x30 its text. The box
sits at x 10–309, y 66–173 of the bottom screen, its "!" and badge reaching
y 50–200. Zakuro patch 0011 draws just that part, centred on the top
screen at 0.8× its pixel scale, in place of the corner screen.

**Tutorial tips** (the notices `ShowTipTextOnce`/`ShowTipTextTimed` show,
told apart by `f_00235F30`'s return address, 0x002638D0 or 0x00270068,
which `overrides/screen_frame_counters.c` keeps at 0x005A4F44): Zakuro hides
the bottom screen, and `overrides/tip_text.c` has the game print the tip's
text, button icons included, at the bottom middle of the top screen in its
own font. Status: works (headless, both opening tips).

### Drawing on a screen from an override

- In gameplay the renderer `f_002E4858(this, phase)` draws one screen a
  frame: `0x00529DA0 +0x3C8` is 0 for the top, 1 for the bottom, flipped
  every frame (`+0x3C4` mode 2). Phase 6 sets up 2D and ends in the panel
  dispatcher `f_00245F9C(list)` (via `f_001EEBB4`); list `0x00518188` is
  the top screen's panels, `0x00518198` the bottom's. Each panel
  (`f_0023CB90` registers one: draw callback at +0x20) draws immediately.
- Text: `f_00235CC4(fmt, …)` is the printf: it formats into a stack
  buffer, `f_002244D8` turns tags such as `<l>` into button glyphs, and
  `f_00195408(font, 0, buffer)` draws at the text position (`f_00254D80`,
  0x00376624) with the current font (`f_00255244`, 0x0037662C). The UI font
  is `[0x003757A0]`. Text is clipped to `0x00529DA0 +0xE4` (x0, y0, x1, y1,
  w, h) and to the text box at 0x0052EC1C.
- Calling game code from an override: set r15 to the function's address
  (generated code starts from the pc), and after it returns keep running
  `recomp_call` until r15 is back at the return address you put in r14,
  as generated callers do (the printf's formatter, for one, returns
  halfway). Lift `ctx->budget` for the call, or a time slice can end
  inside it. See `game_call` in `overrides/tip_text.c`.

**To change it:** override `f_00236104`'s use here, or `f_001EF45C`, or
`f_002DDD64` itself (call the original, then adjust +485/+498).

## Functions that drive the manager

Every function that loads the manager's address (`tools/drc_api.py`). The
short setters are effectively its API.

| Function | Size | Fields | Role (inferred) |
|---|---|---|---|
| `f_001EF45C` | 3 insns | writes +498 | **DRC_SetWipe(frames)** |
| `f_00187F28` | 3 insns | writes +49A | set flag |
| `f_0019464C` | 3 insns | writes +484 | set flag |
| `f_001A21DC` | 3 insns | writes +499 | set flag |
| `f_00290FE8` | 4 insns | writes +544, +548 | set pair |
| `f_001CD55C` | 4 insns | clears +473, +475 | reset fast travel/bar |
| `f_0015B7E4` | | clears +49B, +428, +475, +544 | reset (level start?) |
| `f_002631C8` | | writes +424, +024→+369 | |
| `f_0027BAC4` | | writes +428, +024→+369 | |
| `f_0019465C` | | writes +42C and a vector | |
| `f_002C2650` | | reads/writes +430 | |
| `f_001B1F78` | | Scan button anim (+3E4), +476 | **show Scan button** |
| `f_0027B9F0` | | +3E4/+3E8, +476 | **toggle Scan button** |
| `f_001CD574` | | Red Bricks anim (+3F4), +477 | **show Red Bricks button** |
| `f_0029DB7C` | | Free Play anims (+40C/+410), +478 | **Free Play button** |
| `f_001E761C` | | Chars close + all *Off anims, +472 | **hide the button bar** |
| `f_00262D54` | | all On/Off anims, +472–474 | **show/refresh the button bar** |
| `f_001CD170` | | `fastTravelOn`, *Off anims, `TEXT_AreaName`, map button icons, +473/+475/+49B–49D/+4A4 | **fast travel / area map mode** |
| `f_002D9F84` | 0x1158 | +42C–+4FC, calls `f_00236104` | **map/HUD update** (large) |
| `f_002DBB04` | 0x618 | +424/+428/+42C | map drawing or markers |
| `f_002AD580` | | `ShowOnMap` | **map marker for an object** |
| thunks | | | `f_0015F820`→`f_002DD5EC`, `f_001FCE38`→`f_001F886C`, `f_00235F18`→`f_002D9BB4` (+454/+455/+458), `f_002CA638`→`f_001F40B0`, `f_0029DE40`→`f_0033DC7C` |

Not yet looked at: `f_00158580`, `f_001A2114`, `f_001F3FF0` (`weaponRight`),
`f_00205FC4`, `f_00225790`, `f_00229B9C`, `f_0027B968`, `f_00285B40`,
`f_00285B90`, `f_0029DDF4`.

## Every UI scene and screen asset

The game's files are in `lego_city3ds.fib` (TT "FUSE" archive) in the RomFS;
`lego_city3ds.csv` beside it lists all 12,825 files with sizes and offsets
(`tools/romfs.py game/<rom>.cci list`). UI scenes are `.bxaml` "blend"
scenes under `sprites/blend/`, loaded by `f_00217430(path, …)`; sprites are
`.btga/.bpng`. Loader = the function that names the asset
(`tools/xrefs.py`). Screen = where it was seen or where its name places it;
"?" = not yet confirmed at runtime.

| Content | Files | Loaded in | Screen | Touch |
|---|---|---|---|---|
| **Map** (minimap scene, level map textures, player icon, mission and pulse icons) | `sprites/ui_bottomscreen/maps/` (70), `mapiconmission`, `mapiconpulse` | `f_002DC284` (DRC setup), `f_001EE4E0` (map textures) | bottom | yes: `f_002D9F84`/`f_001EE730` via `f_0020A620`; stylus drag = camera (TouchCameraSystem) |
| **Map button bar**: Scan, Chars (character switch), Audio, Exit, Red Bricks, Free Play, Hint, fast travel, zoom in/out, portraits per character class | `sprites/blend/mapbuttons/` (37), `ui_bottomscreen/drc_*`, `character_*` | `f_002DC284`; shown/hidden by the functions in the API table | bottom | yes (buttons) |
| **Hexagon wipe** (idle badge screen, map open/close transition) | `sprites/blend/drc_wipe/` (13) | `f_002DC284` (+488), `f_00194830` (`MainWindow`) | bottom | no |
| **Dialogue box** (story/radio text on the bottom screen) | `sprites/ui_dialog/` (12), `ui_bottomscreena/b` | `f_002DC284` (+47C/+480), `f_001A0494`; drawn by vtable slot `f_002D93D0` | bottom | ? |
| **Scanner** (visual scan, audio scan, conversation decrypt, distance meter, crosshair, scan meter, object icons) | `sprites/blend/drc_scanner/` (156) | `f_001BEB1C` | ? (Wii U: GamePad) | ? |
| **Safe Crack minigame** | `sprites/ui_bottomscreen/safe_crack/` (14) | `f_002FDEAC`; class vtable `0x003633F8` | bottom | yes: `f_002FD52C` (vtable +0x48) |
| **Wiretap minigame** | `ui_bottomscreen/wiretap` | `f_0031D754` | bottom | ? |
| **Crane minigame** | (`CraneMiniGame_*` objects in levels) | vtable `0x00361C2C` (`f_002D7590`, `f_002D7570`, `f_002D7FD0`); calls `DRC_SetWipe` | top (bottom wiped) | no |
| **Pause menu** | `sprites/blend/pausetop/` (27), status-screen icons | `f_00263BD0` | top + bottom (seen: "Paused / Pause Menu" on bottom) | yes: `f_00300378` |
| **Retry / alert pages** | `sprites/blend/ui_retry/` (45) | `f_0026E9D8` | ? | ? |
| **Save game flow** (select slot, alerts) | `sprites/blend/savegameflow/` (18) | `f_0021FF38`, `f_0026E9D8` | bottom (seen) | yes |
| **Award viewer** | `sprites/blend/awardviewertop/` (17) | ? | top | ? |
| **Break-and-build menu** (Super Build choice) | `sprites/blend/breakandbuildmenu/` (21) | `f_00290B1C` | ? | ? |
| **Photo mode** | `sprites/blend/photomode/` (7) | `f_002EA1C0` (also `Photo_Meter`) | ? | ? |
| **Race start** | `sprites/blend/racestart/` (5) | `f_0017C1A8` | top? | no |
| **Frontend shortcuts / buttons** | `sprites/blend/ui_frontend/` (7), `ui_frontend/` (12) | `f_002203AC` | bottom | yes |
| **Roundabout menu** (main menu, code entry A–Z) | `sprites/ui_roundaboutmenu/` (65) | `f_002206F0` (main), `f_002F3ED4` (code buttons; class vtable `0x00362D00`) | bottom | yes: `f_002F43EC` |
| **Shop** (codes, gifts, postcards, story clips, red bricks, character classes) | `sprites/ui_shop/` (50) | `f_001A0BF4`, `f_001D2FC4`, `f_001B0F5C`, `f_002F4CC4`, `f_002FE724`, `f_00305390`, `f_00308FAC`, `f_00314DE8`, `f_00316E68`, `f_0031C004`, `f_0031EEC4`, `f_00321ED8`, `f_003207BC` | top banner + bottom | yes |
| **Status screen** (options, languages, progress icons; resume/options/credits/exit) | `sprites/ui_statusscreen/` (22) | `f_00309A08`, `f_0031AE68`, `f_0031DB88` | ? | yes |
| **Level select** | `sprites/ui_levelselect/` (11) | `f_00304E20` | ? | ? |
| **Character select** | `sprites/ui_characterselect/` (13) | `f_00168110` (`Main`, portrait highlight), `f_00167FB8` | bottom? | ? |
| **Customisation** | `sprites/ui_customisation/` (7) | `f_0030DC20`, `f_001E5868` | ? | ? |
| **Help / hints** | `sprites/ui_help/` (8), `ui_hints/` | `f_002FC580` (hints) | ? | ? |
| **HUD** (hearts, crosshair, button prompts, tallies) | `ui_hud/` (57), `ui_buttonprompt/` (9) | `f_0017BB14`, `f_002E83D8`, `f_001A1444`, `f_0031A918` | top | prompts touchable? (`f_00258850`) |
| **Loading screens / tips** | `ui_loadingscreens/`, loading tip strings | `UI_LS_%s` | bottom (seen) | no |
| **Boot splash** | `splashscreens/` | `f_00302144` | both | skip by touch: `f_003023B8` via `f_001E7F08` |
| Touch gestures | none in the archive (`TouchGestures/%s.dat` missing) | `leSGOTouchGestureSystem` vtable `0x00364590` | — | unused in this port |

## Safe cracking

Class vtable `0x003633F8`; its update `f_002FD52C` (+0x48) reads the
stylus, takes touch − dial centre (centre = 320 × `[0x003758B8]`,
240 × `[0x003758BC]` = 201.6, 120 on the bottom screen), and if the length
is between `[+0x40]` 38 and `[+0x3C]` 116 px turns it into a 16-bit dial
angle, `atan2(x, −y) · 65536 / 2π` (0 straight up, clockwise), at +0xFA of
its state (`[this+0x24]`). Within 0x1000 (about 22°) of the knob's angle
(+0xF8) it grabs the knob (+0xFE = 1) and the knob follows.
`overrides/safecrack_left_stick.c` gives that update a stylus point on the
dial while the left stick is pushed and the stylus is up: first on the
knob, so it grabs, then turning toward the stick's direction at up to about
8° a frame. The corner screen stays where it is.

## Touch input

All stylus input comes from one table at `0x0052A448`, 32 bytes per entry
(entry 0 used; 1 mirrors it): current x, y (f32, bottom-screen pixels),
touch-down x, y, two more floats, phase byte at +0x1D (2 = held, 0 = none).
Checked with a scripted drag (x 108 → 148 → 198 from a touch-down at 100,150).

- `f_0025255C(out, entry)` copies an entry; `f_001EE970(phase)` asks if the
  phase is that value. Direct readers of the table: `f_00108F30`,
  `f_0010F7C0`, `f_002523CC` (likely the HID update).
- Everything that reads touch (`tools/callers.py work/code.bin work/recomp
  0x0025255C 3 --strings`):
  - TouchCameraSystem update `f_002F77D4` (stylus camera, `docs/camera.md`)
  - DRC map/HUD update `f_002D9F84` (map taps; ignores taps while the
    stylus camera drags), and `f_001EE730`
  - shared button helpers `f_0020A620` (users `f_001DAF9C`, `f_001F0AE4`,
    `f_002AB394`), `f_0020ACFC` (users: splash skip `f_001E7F08`, code
    entry `f_002F43EC`, `f_00322F28`), `f_00258850` (sprite hit tests used
    by HUD prompts, hints, crosshair via `f_0025AFC4`, and menu pages
    `f_001F1A78`, `f_0017CEAC`, `f_00314FA4`/`f_0031657C`)
  - menu pages calling those: `f_002D5874`, `f_002D8984`, `f_002F26C8`,
    `f_00300378` (pause), `f_00313BE8`, `f_003076D0`, `f_0030966C`,
    `f_00312FC4`, `f_00313314`, `f_00319C64`, `f_0030DEEC`
  - Safe Crack `f_002FD52C`; level manager `f_002E1BFC` and
    `ScriptCheckExtras` `f_002E5158`; player/camera `f_001D92A4` (from
    `f_00200444`)

To take touch away from something, override its reader; to feed it
virtual input, write the table (as Zakuro's HID does).

## Which screen things draw on

Each screen has a render camera by name: `camera_top` and `camera_bottom`
(`f_00258744` finds one by name). The frontend keeps them in globals
`0x003774B4` (top) and `0x003774B8` (bottom), set in `f_001820D0`; in
gameplay `f_002D5430` keeps them at +0x40/+0x44 of its render state.
`f_0015BB90(top, bottom, …, mode)` binds them (mode 4 in the frontend, 2 in
gameplay). Moving a bottom-screen scene to the top screen means drawing it
with the top camera instead; next step is to find where scenes pick their
camera, which is the general switch for hiding, moving or re-skinning a
screen.

## States seen in a run

`inputs/explore_bottom_screen.txt` from a fresh save, every 10 frames
(`tools/trace`): language select, saving, copyright, loading with spinner,
StreetPass system messages, story dialogue box, save slots, loading tips,
map, map wipe transition, idle wipe, tutorial tips over the map, pause menu,
dimmed map. `tools/trace/analyze.py` lists the functions that run only in
each (`tools/trace/labels_t1.json`). Those lists mostly reflect what the
game was doing at the time rather than the screen itself, so the static map
above is the better guide; use the traces to confirm a guess.

## Tools

```
python3 tools/extract_code.py game/<rom>.cci work/code.bin          # once
python3 tools/xrefs.py work/code.bin work/recomp "Some/Asset" 0xADDR --sites
python3 tools/disasm.py work/code.bin work/recomp 0x002DDD64         # needs capstone
python3 tools/drc_api.py work/code.bin work/recomp                   # needs capstone
python3 tools/callers.py work/code.bin work/recomp 0xADDR 3 --strings
python3 tools/vtables.py work/code.bin [0xSLOT ...]                  # class names
python3 tools/vtable_dump.py work/code.bin work/recomp 0xVTABLE
python3 tools/romfs.py game/<rom>.cci list [substring]
tools/trace/build.sh                                                 # trace library
ZAKURO_PEEK=51A214:30 scripts/run_headless.sh ...                    # log memory
```
