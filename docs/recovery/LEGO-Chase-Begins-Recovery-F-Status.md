# LEGO Chase Begins — Recovery F

**September 21, 2026. Interactive 3D reached. Development checkpoint, not a finished PC release.**

## Verified native execution

A fresh Linux/Clang build executed the supplied USA game's statically translated ARM code. No ARM CPU interpreter or JIT fallback was introduced. It passed all 296 global initializers, displayed the startup and title screens, started New Game, ran the opening media path, and reached the real-time street scene in Cherry Tree Hills. The prerecorded introduction was skipped using ordinary Start HID input; no game branch, timer, mission state, or camera parameter was patched to reach the scene.

**Player control is verified, not inferred from an input log alone:** a forward Circle Pad sample moved Chase from the station approach into the street. The game changed its lower-screen tutorial from “Use the Circle Pad to move Chase” to the L/R camera tutorial. A subsequent R-button sample rotated the actual 3D view, and the game advanced to its touch-camera tutorial. The before/after captures are in `reports/recovery-f/proof`. The world, buildings, textured characters, pedestrians, vehicles, HUD, and visible ground shadows appear in those captures. This does not establish complete renderer accuracy or mission completion.

Actual input transitions, native executable fingerprint, final counters and runtime-log hash are in `runtime-summary.json`. The run was stopped deliberately with SIGTERM after those checks. This runner prints `RUN LIMIT` and returns 4 for a requested stop as well as its scheduler bound; that exit is not a crash and is not itself a gameplay test result. No native run remains active.

## Renderer and test changes

- Added packed 24-bit depth / 8-bit penumbra shadow-buffer decoding, integer render targets, shadow production/readback, and Shadow2D sampling. The producer executes the generated GLSL rather than pretending that the GPU completed a dropped draw.
- Added six-face cube textures, shared high address bits, face uploads/orientation, and the specialized samplerCube shader route. Procedural/parametric cube routes remain unsupported where explicitly rejected.
- Cached shader source/programs using structural keys, while updating uniforms for every draw. An optional audit path regenerates shaders and checks cache equivalence. A bounded 700-iteration baseline comparison produced identical top, bottom, composite and audio hashes; this is not a full-game equivalence test.
- Added strict optional analog axes to the live HID file, and renderer/CPU floating-point diagnostics. These diagnostics read state; they do not change game values.

**Fresh validation: 12/12 native CTest suites and 17/17 Python tests pass.** These include packed-shadow tests, actual OpenGL shadow shader execution/readback, all six cube-face/address tests, actual OpenGL cube shader tests, memory/kernel, services, generated AOT short-vector/media instructions, scheduling, layout, and input tests. The GL component tests use synthetic fixtures; the gameplay screenshots separately come from the supplied game. No fresh Windows or full GCC execution is claimed.

## Important remaining defect

The opening *real-time 3D* sequence has black world surfaces even though characters and dialogue are visible. This is separate from the prerecorded introduction. The captured shadow matrices contain NaNs. Optional CPU tracing found an orthographic projection calculation with equal near/far values of 1.0, producing positive and negative infinity at the divides in block `0x002273A8` (caller return `0x002CA7DC`). The underlying projection routine is at `0x00227300`; the relevant divides are at `0x002273C4` and `0x002273E8`.

The game's shadow-camera configuration is initialized later for gameplay, where the world and shadows become visible. Captured gameplay shadow draws 149662, 161315 and 163373 have zero nonfinite vertex uniforms, unlike the opening draws; `reports/recovery-f/shadow-camera-transition.json` retains those numerical checks. **This does not mean the opening-scene defect has been fixed.** No replacement matrix, forced finite value, permanently disabled shadow pass, or fictional graphics success was added. The precise console behavior for this transient invalid shadow state and the correct desktop handling still need investigation. The traces establish the observed calculation, not a proven ARM translation error.

## Single-screen / element-level UI

The existing top-only, corner-inset, expanded-lower-screen, and stacked-debug compositor remains intact. It operates on the complete LCD images. **Individual menu items have not yet been extracted or repositioned independently of their backgrounds.** A future per-screen hook can identify the menu/text draws, retain or suppress backgrounds selectively, recenter foreground elements, and remap input/hit regions. That remains case-by-case work.

The lower screen is demonstrably not map-only: this run uses it for movement/camera tutorials and documents touch-drag camera control. A finished one-screen UI must preserve those interactions. Both original LCDs still run internally. The camera is not patched to widescreen.

## Delivery scope

This is a cumulative source package: generated game-code pages, translator, runtime, services, EGL/OpenGL proof renderer, audio support, tests, reports and captures. It excludes the original game image, code.bin, RomFS, extracted asset containers, raw memory/texture diagnostic dumps, local saves, and a prebuilt binary.

The verified target is Linux x86-64 headless, with a real software OpenGL driver. It does not yet provide an OS game window, native keyboard/controller devices, direct speaker playback, or a playable Windows executable. Audio is captured to WAV. No claims are made about combat, later missions, completed saves/reloads, networking, long sessions, or full-game performance.

The previous Stage 2 test does not need to be repeated, and the game/framework do not need uploading again in this conversation. Existing old reports remain historical; `reports/recovery-f` and this status describe the fresh checkpoint.