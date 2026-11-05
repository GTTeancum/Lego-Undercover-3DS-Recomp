# The camera and the right stick

How the on-foot camera takes input in LEGO City Undercover: The Chase
Begins (USA), and how the right stick is wired into it. Found with the
tools in `tools/` (see `docs/bottom-screen.md` for them).

## What the game already has

- **Camera object:** a global at `0x005899E0` (position at +0x04/+0x0C,
  view matrices further in).
- **Per-frame camera input** (consumed and cleared by the camera later the
  same frame, after its own limits and smoothing):
  - `+0x140` yaw rate
  - `+0x144` elevation rate (raises and pulls back, capped near 22°)
  - `+0x13C` a third rate fed from analog channel 0 with a ±0.25 deadzone
- **Who fills them:** `f_00200444`, run each frame for every character; for
  the player (`[0x00377228]`) it adds
  - `2·ch8 − 2·ch10` to yaw (×−1 with the invert option in `0x0051B400`+3
    bit 2). Channel 8 is **L** and channel 10 is **R** (1.0 while held);
  - the stylus drag, from **TouchCameraSystem** (global at `0x00527340`,
    pointer at `0x00375F34`, update `f_002F77D4`): `+0x34` to yaw and
    `+0x38` to pitch, rate-controlled by how far the stylus is from where
    it touched down, tuned by three floats at `0x00375F38`
    (0.05, 5.0 clamp, 0.001).
- **Input channels:** the input manager (pointer at `0x00376954`, an
  object on the heap) holds 43 channels of 20 bytes from `+0x14`:
  `{f32 value, u32 kind, f32 0.1, f32 0.75, u32}`. Channels 0–3 are
  analog axes (kinds 1, 2, 1, 2): 0/1 the circle pad, 2/3 a second stick
  left over from the Wii U version, which **no code reads** (their index
  globals `0x00376998`/`0x0037699C` have no references). The camera's
  channel numbers are kept in globals `0x00376958`–`0x0037699C`.

So the 3DS game turns the camera with L/R (yaw only) and the stylus (yaw
and pitch); the Wii U's right stick axes exist but are not wired.

## The right stick

1. Zakuro writes the right stick every frame into the 3DS shared page at
   `0x1FF81F00`: `"ZKRS"`, then x and y as f32 (−1..1, up positive)
   (Zakuro patch 0009, `memory::config::HOST_RIGHT_STICK`). The console
   leaves that end of the page unused.
2. `overrides/camera_right_stick.c` overrides `f_00200444`: it runs the
   original, then for the player only, and not while a cutscene holds the
   bottom-screen wipe, adds the stick's x to the frame's yaw input
   (camera `0x005899E0` `+0x140`): yaw += x · 2.0 (full stick turns as
   fast as holding L or R), with a 0.15 deadzone, rescaled.
3. Up/down does not use the game's pitch input (`+0x144`): that one only
   raises the camera to about 22° and pulls it back as it goes (15 to 21
   units away, which reads as a zoom). Instead the override keeps its own
   elevation (5° to 88°, 3° a frame at full stick, starting from the
   game's angle the first time the stick moves it) and tilts the view to
   it around the frame's look-at point, the player, at the camera's own
   distance. It overrides `f_0022479C`, which copies the frame's camera
   matrix to the view (`0x0052AB80`: right, up, forward rows and the eye,
   row-major 4x4), and rewrites both the copy and the source. The look-at
   is `0x0058D39C` (the eye at `0x0058D3C0`, then the FOV). It only acts
   when the view's eye is the gameplay camera's (`0x005899E0 +0x004`), so
   cutscene cameras are left alone.

**Status: works.** Headless test in gameplay: left/right orbit both ways;
stick up swings the camera over the player until it looks straight down
on him, with the player kept in the middle and the distance unchanged;
down brings it back; turning still works while looking down; the angle
holds after release.

Tuning: `YAW_PER_FRAME`, `TILT_PER_FRAME`, `LOWEST`, `HIGHEST`,
`DEADZONE` at the top of the override; signs flip there too.

## Motion controls

The game asks for the console to be moved at its observation points
("Move your Nintendo 3DS to look around") and uses the gyroscope in the
wiretap (`f_0031C7F8` sets the gyroscope's reference direction through
`f_002C57F0`; the sensors are set up at boot by `f_002CDF48`, globals at
`0x00376034`: +0x0C accelerometer, +0x10 gyroscope). There is no stick in
their place, so Zakuro (patch 0025, `title_hooks::stick_motion`) makes the
right stick move the console: left/right turn it about its upright axis
(`InputState::turn`), up/down tilt it to look up and down (`tilt[1]`, up to
about 70 degrees), 90 degrees a second at full stick. The sensors report
it as the console being moved. Gameplay reads neither sensor, so the stick
keeps driving the camera there. Not while map mode or the wheel has the
stick. Status: the sensor model is unit-tested (a turn reads on the
gyroscope's upright axis, gravity unchanged); not yet seen at an
observation point or the wiretap, so the direction of each axis may need
flipping (`MOTION_SPEED` sign, `input.tilt[1] -=`).

## Draw distance (doubled)

`overrides/draw_distance.c` scales three things by `DRAW_DISTANCE_SCALE`
(2.0):

- **Far clip.** A level's camera settings give `ZFarClip` (default 70,
  capped at 600 by `f_002F67FC`; the hub uses 260). The cap applied, it goes
  to the top screen's camera (`f_0023F168(camera, near, far)`, called from
  0x002F711C) and to the gameplay camera (`f_002E08DC`); both are scaled.
  The gameplay camera hands its far plane on to `f_0023F168` each frame,
  which is left alone (scaling it there too made it 4x), as are the other
  cameras (the map's, 2D ones).
- **Fog.** `FogStart`, `FogEnd`, `DS_FogStart`, `DS_FogEnd` as the settings
  getter `f_00241280` returns them (`f_001A293C` reads them); values of 0 or
  below (no fog, markers) are left alone. Hub: 220-260 becomes 440-520.
- **Model detail.** Each object's LOD switch distances, passed in pairs to
  `f_002282B8`.

Seen headless: the hub's far street, buildings and traffic draw past where
they stopped, with less haze. Streaming (what the game has loaded) is not
changed, so beyond what it keeps loaded there is nothing more to draw.
