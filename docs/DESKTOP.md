# Optional desktop display

The SDL3 frontend displays both original LCD images in one resizable window. The main screen keeps its proportions; the whole touchscreen appears at bottom right at approximately 28% of window width. Tab enlarges/restores it, F11 toggles fullscreen, and mouse clicks map to the original 320x240 touch coordinates.

## Current verification scope

Added the missing arithmetic support and fixed a code-table lookup crash exposed by tests. Five more startup sections now run, including both processing loops and their returns. All 5 fresh headless game runs passed their preceding stopping points. Each exact native passed its enabled tests once (84/84 (round 01), 86/86 (round 02), 86/86 (round 03), 86/86 (round 04), 86/86 (round 05)), with no failures, errors or skips, and was saved privately before its game run.

These were headless game runs. No new screenshot or visual progress is claimed; the earlier loading image remains the last verified windowed result. Speaker playback and Windows execution remain unverified. The pinned SDL3 archive could not be downloaded in this workspace; desktop configuration is unchanged. No new windowed original-game run was performed.

Final headless stop: **MissingBlock `0x00151B6C`**. This batch adds 5 sections / 128 words and advances 23,764 executed instructions beyond the prior same-mode checkpoint. The complete prior audio capture is preserved byte-for-byte as an exact prefix; the final capture adds 4 well-formed diagnostic records. Complete state and save contents match the prior same-mode checkpoint. This is a longer diagnostic capture, not evidence of speaker playback. This is not a visual or gameplay milestone.

Next interval: **`[0x00151B6C, 0x00151D10)`**. Add the next 105-instruction helper, which combines neighboring byte values into row words, restores its working pointer and returns through a stack-loaded address. The future memory values and return target remain unobserved. Existing handlers accept this reviewed interval.

See [CHECKPOINT.md](../CHECKPOINT.md) and [current recovery record](recovery/CURRENT-REBUILD.md). Original inputs and all generated game code, executables and runtime evidence stay private.

## Build

Desktop support defaults to off. The headless build does not require SDL. With the public source in `repo`, restored private AOT in `restored-aot/generated2`, and authenticated private inputs:

```bash
cmake -S repo -B build-desktop -G Ninja -DCMAKE_BUILD_TYPE=Release   -DBUILD_TESTING=ON -DLEGO_BUILD_DESKTOP=ON   -DLEGO_AOT_DIR="$PWD/restored-aot/generated2"   -DLEGO_SUPPLEMENT_CODE_BIN="$PWD/recovered-inputs/LEGO-Chase-Recovery/code.bin"
cmake --build build-desktop --parallel 4
ctest --test-dir build-desktop --output-on-failure
```

Use a C++20 toolchain, CMake, Ninja, Python 3, Clang and llvm-objdump. CMake looks for installed SDL3 3.2 or newer; otherwise `LEGO_FETCH_SDL3=ON` fetches the pinned SDL 3.4.18 archive. For an existing installation, pass `-DSDL3_DIR=/path/to/lib/cmake/SDL3 -DLEGO_FETCH_SDL3=OFF`. Linux window support also needs the system development libraries for its selected backend.

## Run and controls

Use `--frontend sdl` to enable the window and input producer. It requires `--cpu-mode diagnostic-dual`, `--display-clock-mode reference-idle` and `--gpu-vram-mode reference-zero`. Omitting the frontend option retains headless execution. Reuse the authenticated input and diagnostic profile described in the private continuation, but create fresh owned state/save and output paths for each run.

Optional `--desktop-screenshot <new-path.bmp>` saves the actual first nonblack SDL render target and refuses an existing output. It writes no screenshot if no such frame is reached and never inserts a game image.

| PC control | Action |
| --- | --- |
| Left mouse inside lower panel | Touchscreen press |
| Tab | Enlarge/collapse lower panel |
| F11 | Toggle fullscreen |
| Escape / close window | Stop desktop run |
| Z / X / S / A | 3DS A / B / X / Y |
| Q / W | 3DS L / R |
| Enter / Backspace | Start / Select |
| Arrow keys | D-pad |
| I / J / K / L | Circle pad up / left / down / right |

Short presses and clicks remain pending until the next committed HID sample. Focus loss releases input; leaving the panel or changing layout cancels touch input. Minimizing defers presentation and releases input while retaining the latest frame for restore.

## Remaining limits

Scanout supports verified rotated RGB8 buffers in device VRAM and explicit LCD force-black. Unsupported formats, fills, selectors and bounds stop explicitly. Input is published at modeled display periods, not verified hardware speed. Audio is captured to the DSP file; this frontend does not implement speaker playback. Windows desktop execution is untested, and the SaveData/SharedExtData backend is not implemented for Windows. No game-specific minimap crop is enabled.

Keep original inputs, generated game code, executables, screenshots, audio and saves private. Hosted synthetic desktop checks are not original-game visual evidence.
