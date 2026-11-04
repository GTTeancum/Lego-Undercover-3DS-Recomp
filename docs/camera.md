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
