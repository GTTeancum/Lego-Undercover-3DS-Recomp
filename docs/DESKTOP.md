# Optional desktop display

The SDL3 frontend displays both original LCD images in one resizable window.
The main screen keeps its original proportions; the whole touchscreen appears
in the bottom-right corner at approximately 28% of the window width. **Tab**
enlarges the touchscreen and restores the inset. **F11** toggles fullscreen.
Mouse clicks inside the panel are mapped back to the original 320×240 touch
coordinates.

## Build

Desktop support is optional and defaults to off. The existing headless build
does not require SDL. From the recovery directory described in
`docs/recovery/CURRENT-REBUILD.md`, with the separately supplied private inputs:

```bash
cmake -S repo -B build-desktop -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON -DLEGO_BUILD_DESKTOP=ON \
  -DLEGO_AOT_DIR="$PWD/restored-aot/generated2" \
  -DLEGO_SUPPLEMENT_CODE_BIN="$PWD/recovered-inputs/LEGO-Chase-Recovery/code.bin"
cmake --build build-desktop --parallel 4
ctest --test-dir build-desktop --output-on-failure
```

Use a C++20 toolchain, CMake, Ninja, Python 3, Clang and `llvm-objdump` as for the
existing native build. CMake first looks for an installed SDL3 version 3.2 or
newer. Otherwise, `LEGO_FETCH_SDL3=ON` downloads the SHA-256-pinned official SDL
3.4.18 release. For an existing SDL installation, pass
`-DSDL3_DIR=/path/to/lib/cmake/SDL3 -DLEGO_FETCH_SDL3=OFF`. SDL's Linux window
backend also needs the appropriate system development libraries.

The current Linux build uses SDL 3.4.18. All 84 enabled suites have passing
results, including the ROM-free desktop smoke. The initial full run passed 83;
one test was not launched because its executable lacked execute permission.
Only that file's mode was corrected, with identical bytes, and only that test
was then run. The original failed run and both real JUnits are preserved.
The desktop smoke uses synthetic panels; actual game evidence is below.

## Run the current diagnostic profile

`--frontend sdl` explicitly enables the window and keyboard/touch producer.
It requires `diagnostic-dual` CPU execution, the `reference-idle` display clock
and `reference-zero` VRAM. Omitting the option, or choosing
`--frontend headless`, retains the existing diagnostic mode.

This Linux example creates fresh owned storage and unique output paths:

```bash
LEGO_DESKTOP_RUN="$(mktemp -d "$PWD/desktop-run.XXXXXX")"
mkdir -p "$LEGO_DESKTOP_RUN/shared/00048000/F000000B/user" "$LEGO_DESKTOP_RUN/saves"
./build-desktop/LEGOChaseNative recovered-inputs/LEGO-Chase-Recovery/code.bin \
  --frontend sdl --desktop-screenshot "$LEGO_DESKTOP_RUN/first-nonblack.bmp" \
  --shared-extdata-root "$LEGO_DESKTOP_RUN/shared" \
  --save-data-root "$LEGO_DESKTOP_RUN/saves" --ptm-step-mode empty \
  --romfs romfs.bin --exheader recovered-inputs/LEGO-Chase-Recovery/Prepared-Launch/exheader.bin \
  --gpu-vram-mode reference-zero --display-clock-mode reference-idle \
  --cfg-profile reference-stereo --cfg-sound-mode stereo --cfg-language en \
  --cpu-mode diagnostic-dual --block-limit 100000000 --host-event-limit 131072 \
  --dsp-special-profile empty-config --dsp-executor live-teakra \
  --dsp-reset-profile reference-zero-data --dsp-transmit-profile reference-stereo \
  --dsp-audio-mode capture --dsp-boot-mode reference-slice \
  --dsp-audio-file "$LEGO_DESKTOP_RUN/audio.dspaud"
```

`--desktop-screenshot` is optional. It saves the actual SDL render target when
the first nonblack decoded LCD frame is presented; it refuses an existing output
file. If no such frame is reached, no screenshot is written. It does not insert
game imagery or simulate a successful boot. Diagnostic stop reasons remain
visible in the terminal.

## Controls

| PC control | Action |
| --- | --- |
| Left mouse button inside lower panel | Touchscreen press |
| Tab | Enlarge/collapse lower panel |
| F11 | Fullscreen toggle |
| Escape or window close | Stop desktop run |
| Z / X / S / A | 3DS A / B / X / Y |
| Q / W | 3DS L / R |
| Enter / Backspace | Start / Select |
| Arrow keys | D-pad |
| I / J / K / L | Circle pad up / left / down / right |

Short button presses and clicks are retained until the next committed guest HID
sample. Focus loss releases input. Moving outside the panel cancels the current
touch; changing layouts also cancels pending touch input. Minimizing the window
defers presentation and releases input while retaining the latest frame for
restore.

## Scope and remaining work

- Scanout currently supports the verified rotated RGB8 buffers in device VRAM
  and explicit LCD force-black. Unsupported formats, LCD fills, selectors or
  memory bounds stop with an error.
- Keyboard and mouse input are published at modeled display periods. This
  remains a diagnostic execution profile, with no hardware-speed or real-time
  performance claim.
- Audio is captured to the existing DSP record file. Speaker playback is not
  implemented in this frontend.
- Linux is the verified host. Windows desktop execution is untested, and the
  current SaveData/SharedExtData storage backend is not implemented for Windows.
  This is not yet a ready-to-play Windows release.
- The layout helper keeps source cropping separate from panel placement.
  A future verified minimap crop can use that seam; no game-specific minimap
  crop or automatic screen/menu integration is enabled now.

The current original-game desktop run presents 524 frames and reaches
**MissingBlock `0x00152AFC`**, as does the fresh headless run. Both modes advance
902 core-0 instructions / 3,364 diagnostic ns beyond their respective
`db19fee` captures. Native children and outer launchers each exited 3, with no
timeout, stderr, cleanup signals, or remaining live owned process.

The first-nonblack screenshot shows the lower-screen loading indicator in the
bottom-right inset. Its raw BMP and decoded 1280×720 RGBA pixels match the prior
capture. This is an initial startup image, not a terminal-frame or gameplay
observation. Current final registers, diagnostic time, complete audio, and
actual state/save contents agree across modes. Desktop retains the unexplained
3,883-instruction core-0 difference.

The next bounded task is the nine-word dispatcher at
`0x00152AFC..0x00152B20`, using existing instruction support. Its conditional
target `0x001529C4` and captured return `0x00150E6C` are still unrecorded.
See [the short checkpoint](../CHECKPOINT.md) for the current continuation and
[the recovery record](recovery/CURRENT-REBUILD.md) for detailed evidence.
Original inputs, generated AOT, executables, saves, screenshots, and audio
remain private and are not published with the source repository.
