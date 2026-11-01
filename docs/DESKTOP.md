# Optional desktop display

## Current verified status

Resume from [CHECKPOINT.md](../CHECKPOINT.md). This continuation starts from `7f3f4c7`.

Eight more observed startup sections now execute. Additional row-processing loops, their register and stack returns, and a caller entry resume original control flow. All eight accepted headless Release candidates passed 90/90 enabled tests, with complete passing JUnit reports and explicit build/test exit 0. Startup advances 147,594 instructions / 550,494 diagnostic ns to MissingBlock `0x0014F608`; those counters are not host-performance measurements or gameplay percentages.

The final capture contains 285,866 records / 4,573,888 bytes. The entire baseline of 285,850 records / 4,573,632 bytes is an exact prefix, followed by 16 valid records whose two samples are zero. State and save contents remain unchanged. This is complete diagnostic capture evidence and does not establish speaker playback or audible content.

One round-27 attempt could not start a game process because the unchanged frozen native lacked execute permission. Its rejected evidence was preserved; restoring permission and using fresh storage produced the accepted run. There were no failed build/test gates. Rounds 23 and 24 retain text logs ending after test 88, while their complete JUnit reports and explicit successful CTest exits support the accepted gates. A separate integrity check rejected the original round-28 backup archive. The failed archive was preserved and a fully verified replacement saved; the tested native and successful run remain intact. Hosted checks are pending at source-preparation time; actual results are recorded separately against the published commit.

Menus, gameplay, speaker playback and Windows execution remain unverified. No new screen capture or windowed run was performed. No new capture-on/off comparison was performed. Earlier black captures remain private historical evidence; no fresh visible milestone is claimed.

## Capture current screen pixels without SDL

Add `--scanout-snapshot NEW_DIRECTORY` to the existing diagnostic launch. It is
optional and defaults to off; a headless build can use it. The parent directory
must already exist, but `NEW_DIRECTORY` must not exist. For example, add:

```bash
--frontend headless --scanout-snapshot "$LEGO_DESKTOP_RUN/final-scanout"
```

Use the authenticated inputs and diagnostic profile in the private continuation. Do not
reuse the state, save, audio, screenshot or snapshot paths from an earlier run.
The option writes `top.ppm` (400x240) and `bottom.ppm` (320x240), using the existing
scanout decoder only after `Run` has returned. The files contain raw RGB samples
in P6 order, without resampling or an asserted transfer characteristic. Original
runtime diagnostics are printed before the additional snapshot result. The
normal diagnostic exit code is preserved on successful capture.

This is not the SDL window, not a first-nonblack capture, and not a boot-success
signal. Existing files/directories are refused; invalid frames or write failures
report an error and retain partial output. The capture does not synthesize a
frame, attach an input producer, advance a guest event or issue a render command.
The most recent saved stopped-screen captures (checkpoint `7fb60ad`) are black. The earlier loading picture was captured
at a different time, so comparing those two capture points does not demonstrate a
regression. No fresh windowed game execution is claimed.

At checkpoint `0755bde`, a deliberate capture-disabled run of the same final native
matched all original
diagnostics after path normalization, all counters, the stop, and complete audio,
state and save contents. Two selected snapshot/display tests also passed Clang
address/undefined-behavior sanitizers; six command-line validation cases passed.
Those are retained earlier results, not a new paired run in the current checkpoint.
Keep completely black captures private rather than posting them in user updates.

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
