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

## Other bottom-screen content

Found from the asset paths each loads (`tools/xrefs.py`):

| Screen | Loaded in | Assets |
|---|---|---|
| Map textures | `f_001EE4E0` | `sprites/UI_Bottomscreen/Maps/%s.tga`, `lvl_01_cherrytreehills.tga` |
| Scanner | `f_001BEB1C` | `Sprites/blend/DRC_Scanner/…` |
| Pause menu | `f_00263BD0` | `Sprites/Blend/PauseTop/PauseTop` |
| Safe crack minigame | `f_002FDEAC` | `Sprites/UI_Bottomscreen/Safe_Crack/…` |
| Wiretap minigame | `f_0031D754` | `Sprites/UI_BottomScreen/Wiretap/UI_Wiretap_BG.tga` |
| Boot splash (bottom) | `f_00302144` | `sprites/splashscreens/splash%ubottom…` |
| Save slots | `f_0021FF38` | `Sprites/Blend/SaveGameFlow/Page_SelectSlot` |
| Roundabout menu | `f_002206F0` | `Sprites/UI_RoundaboutMenu/Main` |
| Portrait highlight | `f_00168110` | `Sprites/UI_BottomScreen/UI_PortraitHighlight.tga` |

Strings for screens not yet traced: `DRC_Buttonflash`, `breakAndBuildMenu`,
`PhotoMode/photoMode`, `UI_Retry/Page_AlertText`, `UI_Shop_Topscreen`.

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
tools/trace/build.sh                                                 # trace library
ZAKURO_PEEK=51A214:30 scripts/run_headless.sh ...                    # log memory
```
