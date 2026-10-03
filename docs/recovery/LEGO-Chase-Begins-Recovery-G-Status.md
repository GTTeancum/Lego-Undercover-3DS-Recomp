# LEGO Chase Begins — Recovery G

**September 21, 2026. Native Linux desktop window and OS-driven gameplay verified. Development checkpoint, not a finished PC release.**

## What changed since Recovery F

Recovery F reached interactive Cherry Tree Hills through file-driven test controls. Recovery G adds a real SDL2 desktop window, keyboard and controller polling, mouse-to-touch input, and a device-audio queue. The desktop presents the renderer's completed RGBA composite directly from memory; it does not display saved screenshots as a substitute for rendering. The existing statically translated ARM game code and EGL/OpenGL renderer still execute the game.

The lower-screen pointer path was previously disconnected at the runner boundary. It now feeds the existing HID shared-memory implementation with touch-down and original 320×240 coordinates. Window scaling, letterboxing, corner/expanded layout, focus loss, and drag cancellation are handled before those values reach the game.

An initial desktop launch exposed a GL context conflict: SDL's nominally software window surface could select an internally accelerated presentation path while the game used a separate EGL context. The final frontend forces SDL's software framebuffer path before window creation. A component test now creates the EGL context after the SDL window and verifies both actual GL pixel readback and the SDL-presented surface. The failed first launch is retained under `desktop-initial-context-failure`; it is not counted as a successful game run.

No generated game-code, ARM-lowering, service behavior, game-camera parameters, shadow calculations, guest timer values, or mission-state branches were modified for this checkpoint. A source comparison against the delivered Recovery F archive verifies that all 604 original generated files and 149 original vendor files are byte-identical.

## Fresh game execution and interaction

The verified native run used Clang-built Linux x86-64 code, Mesa llvmpipe, an actual SDL window on Xvfb, and genuine X11/XTEST input events. This is a virtual display/device test, not testing on the user's desktop or physical hardware. There is no ARM CPU interpreter or JIT fallback.

Normal scripted HID transitions handled the early startup sequence; subsequent desktop interactions used OS keyboard/mouse events. No live HID, pointer, or map override file supplied those desktop interactions. Relevant input transitions, OS-event action records, and before/after window captures are included in `reports/recovery-g`.

Verified in the game:

- All 296 initializers complete, the opening path runs, and Cherry Tree Hills is reached.
- Keyboard W moves Chase through the 3D street. Subsequent keyboard E (original R shoulder input) rotates the view and reaches the touch-camera instructions.
- Left-mouse dragging inside the displayed lower screen produces bounded original touch coordinates in the guest HID stream. The first drag changes the view; the tutorial subsequently advances to the map instructions. The second recorded drag begins after the map help has already appeared, so that second gesture is not claimed to have caused the tutorial transition. This is a limited interaction check, not exhaustive touch-camera characterization.
- Tab expands the lower screen over the main view, and another press restores the corner inset. Real OS-event component tests also cover focus loss, held-input suppression, resizing, letterboxing, mouse coordinate mapping, and normal window closure.
- The final game run ends through an ordinary `WM_DELETE_WINDOW` event. The runner logs `OPERATOR STOP` and returns 4 by design. That is a deliberate close, not a crash and not an automated full-game-pass exit code.

The final rebuilt and bundled native executable is byte-identical to the immutable executable used for that full desktop run:

```text
SHA-256 4d4d38c0e7c503fb8ed43082c4206cf3dc5607afcbb0355dc1318a29b475bcf6
```

Final counters and duration:

| Measurement | Observed result |
|---|---:|
| Initializer callbacks completed | 296 / 296 |
| Native block entries | 588,474,057 |
| Thread objects at shutdown | 20 |
| Draws at last complete heartbeat | 213,870 |
| Desktop presentations | 2,195 |
| Wall time | 1,442.23 seconds |
| Guest time at last complete heartbeat | 88.87 seconds |

Presentations are host composite submissions, not a measurement of unique game frames or game FPS. The diagnostic/software-rendered run was much slower than real time: the approximately 24-minute wall-time test represents only about 89 seconds of guest time. No real-time-performance claim is made.

## Audio: connected, but physical playback unverified

The existing DSP-generated stereo 16-bit PCM stream is now submitted to SDL at 32,728 Hz as well as to the existing bounded WAV capture. Audio-device open/format failures are explicit; they are not reported as successful playback. Queue backpressure operates on host execution and does not change guest tick values or discard samples. Slow execution can nevertheless starve a real audio device.

The game and component tests used **SDL's dummy audio driver**. They establish successful queueing and draining only, not audible playback, speaker quality, latency, or real-device synchronization.

The complete desktop run submitted 11,640,320 PCM bytes, the same number captured in the finalized WAV, with zero bytes left in the queue at shutdown. It recorded 18,188 DSP ProcessFrame calls and 3,678,976 nonzero samples. PCM SHA-256 is `a9acb1ff86136ebf748909e3676965ff0ae6b1ad529f7d4286620ac774543f86`. Original game audio is not included in the package; the measurements and hashes are.

## Regression and equivalence checks

**16 / 16 registered native CTest suites and 17 / 17 Python tests pass** in the final source/build environment. Existing memory, services, scheduling, AOT instruction, screen-layout, shader, cube-texture, and packed-shadow checks remain in place. New tests cover pointer parsing/routing, keyboard/controller mapping, real SDL surface presentation/audio queue behavior, EGL context coexistence, and real OS-event delivery. Component fixtures are distinct from the supplied game's execution proof. The OS-event tests require Xvfb and XTEST; their registration is conditional on those tools being present.

A deterministic headless-versus-desktop startup comparison matched **219,672 diagnostic lines** and **8,408,960 PCM bytes**. Only the absolute isolated run-directory prefixes were normalized in the diagnostic text, because save backup paths differed. No program counters, tick values, results, or input states were normalized. The compared PCM prefix SHA-256 is `dd4bf3b89927dac6902c9af09b24353174d8618809a78c2df67c9b0b2c9529c6`.

This comparison covers the retained common startup prefix only. Later desktop OS input deliberately diverges; this is not proof of full-game equivalence. The headless baseline was deliberately stopped after its retained prefix, rather than being a second completed gameplay run.

Build/test logs, complete retained game logs, immutable binary fingerprints, source comparison, action records, and captures are supplied. Test harnesses under `reports/recovery-g/test-harness` retain their original container paths for provenance; they are not user-facing launchers.

## Controls and launchers

| Host control | Game / presentation action |
|---|---|
| W / A / S / D | Circle Pad movement |
| J / K / U / I | Original A / B / X / Y |
| Q / E | Original L / R shoulders |
| Arrow keys | Directional pad |
| Enter / Backspace | Start / Select |
| Tab | Lower-screen expansion toggle |
| Left click / drag on lower-screen view | Original touchscreen input |

Controller polling and standard SDL mapping are implemented and unit-tested, including the analog dead zone and Back/View presentation toggle. **A physical controller was not tested.** There is no remapping/settings interface yet.

`Run-Linux.sh /absolute/path/to/prepared` chooses a locally built executable or the included `bin/linux-x86_64/LEGOChaseNative`. Both launcher selection paths were actually smoke-tested; the bundled path was checked with a one-iteration scheduler bound, not a second complete gameplay run. A dedicated per-user state directory keeps proof output and saves separate from original game assets. The launcher does not delete saves or install dependencies. Do not run two game instances against the same state directory.

`Build-Linux.sh` runs CMake, the native tests, and Python tests. Its shell syntax and constituent build/test commands were checked here. The default build parallelism is two workers. All required source is present; CMake does not fetch dependencies from the network.

The exact included ELF was tested on Debian 13 with SDL 2.32.4, Clang 17, and Mesa 25.0.7 llvmpipe. Its versioned symbol requirements include GLIBC 2.38 and GLIBCXX 3.4.32. SDL2, EGL, OpenGL and system C/C++ libraries are not bundled. This is not a universal Linux binary; older hosts may need a local rebuild. `LEGO_DESKTOP_NO_AUDIO` is a presence flag: set it to disable device audio, and unset it to enable audio again. WAV capture remains enabled either way.

## Remaining defects and unverified areas

**The opening real-time 3D cutscene still has black environmental surfaces.** Recovery F traced invalid shadow projection values during that scene; this checkpoint did not establish the correct hardware-equivalent handling and did not fix the defect. The game's later gameplay shadow-camera initialization still produces a visible world and shadows. No replacement matrix, forced finite values, disabled shadow pass, or fabricated graphics completion was added.

Both original screens continue running internally. Current single-screen layouts composite whole LCD images. Independent menu-item/background extraction, per-menu recentering, and other element-level hooks remain future case-by-case work. There is no true widescreen camera projection patch.

This is a slow Linux development checkpoint. **No Windows executable, Windows build/execution, physical speaker test, physical gamepad test, completed mission, save/reload round trip, combat validation, multiplayer, long-session stability, or full-game compatibility is claimed.** The existing EGL renderer is still Linux-specific. The prior narrow CECD compatibility no-op remains explicitly documented in README; StreetPass is not implemented.

## Package and provenance

Recovery G contains cumulative source, all generated AOT pages, selected pinned framework support, tests, desktop implementation, Linux launch/build scripts, the exact tested Linux executable, logs, and game captures. It excludes the original game image, code.bin, RomFS, raw texture/memory dumps, extracted game asset containers, local saves, and captured game audio. The user-supplied decrypted USA revision remains required.

The imported framework remains pinned to TriAevum `a9b447709d4405848d75352354891059cebb9ff8`. Original licenses, notices, and attribution are retained. Supplying the native test binary does not relicense the user-derived game code; see `LICENSE_SCOPE.md`.

`FILE-CHECKSUMS.sha256` covers the final package contents other than itself. Older checkpoints and reports remain historical. `reports/CURRENT-STATUS.md` and `reports/recovery-g` describe this fresh checkpoint. No native game run remains active. The game/framework need not be uploaded again and the obsolete Stage 2 test need not be repeated.