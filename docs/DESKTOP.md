# Optional desktop display

The SDL3 frontend displays both original LCD images in one resizable window. The main screen keeps its proportions; the whole touchscreen appears at bottom right at approximately 28% of window width. Tab enlarges/restores it, F11 toggles fullscreen, and mouse clicks map to the original 320x240 touch coordinates.

## Current verification scope

Seven more startup gaps are filled: two calls, their setup and dispatch, a complete 16-row processing helper, its return and the next caller dispatch. Fresh execution now reaches the next missing helper. All 7 fresh headless game runs passed their preceding stops. All seven builds passed 89/89 enabled tests once. The final post-stop captures remain completely black and are retained only as private diagnostics. No new screenshot is posted and no fresh windowed run was taken. No visual or gameplay milestone is claimed; speaker playback and Windows execution remain unverified.

Final stop: **MissingBlock `0x00155008`**. This batch adds 7 sections / 102 words and advances 388 executed instructions beyond the prior same-mode checkpoint. Complete audio, state and save contents are byte-for-byte identical to the preceding same-mode checkpoint. This does not establish speaker playback. This is not a visual or gameplay milestone. Desktop configuration is unchanged.

Next interval: **`[0x155008, 0x155040)`**. Add the reviewed 14-instruction helper section that loads a mask, combines neighboring guest word and halfword values into one output row word, advances pointers, decrements the row count and conditionally loops to an interior suffix at 0x00155010. The actual future memory values, loop completion and fallthrough at 0x00155040 remain unobserved. Do not include that successor body or invent a return target. Existing handlers accept this reviewed interval.

See [CHECKPOINT.md](../CHECKPOINT.md) and [current recovery record](recovery/CURRENT-REBUILD.md). Original inputs, generated code, executables and captures stay private.

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
