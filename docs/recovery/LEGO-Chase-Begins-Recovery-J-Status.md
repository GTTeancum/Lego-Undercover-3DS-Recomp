# LEGO Chase Begins — Recovery J

**September 22, 2026. Native Linux crash-hunt checkpoint. Cumulative source and the exact tested executable; not a finished Windows release.**

## Baseline and executable identity

Recovery I's uploaded ZIP passed CRC and all original manifest checks before changes. Its archive SHA-256 is `ddc189ef30817e142e5a6e6f1e9f5b55333557aab2e99fe71cd6bf00efb71460`. The supplied split game image was reassembled and extracted locally; the original CCI hash and decompressed executable revision remain unchanged. No replacement upload was needed.

The included Linux x86-64 ELF is the one used in the final startup comparison and the entire final game/exit/reload run:

```text
SHA-256 9f2518d2f893ffd53bc6073169309bbc5a92ae5048d5474412634fcb9b6a827b
Bytes   101,217,616
```

All **604 generated AOT files are byte-identical to Recovery I**. No shader, camera matrix, guest timer, mission branch or screen-composition algorithm was patched. No ARM interpreter/JIT fallback was introduced. The existing resource-lifetime, DSP safety, event-pointer, save-write, state-lock, desktop/input/audio and diagnostic work remains included.

## Four reproduced allocation-before-validation paths

These are native-host robustness defects reproduced with authored, ROM-free fixtures. They are **not** four claimed crashes observed in ordinary retail gameplay. Test-only allocation guards record large allocation attempts and throw before allocating them; the tests never exhaust the host to establish a failure.

| Path | Reproduced behavior before the correction | Corrected behavior |
|---|---|---|
| GSP DMA | A 16,777,216-byte allocation was attempted before discovering an unmapped source. Wrapping and mapped-but-unreadable cases also reproduced premature allocation. | Validate the entire readable source and writable destination first. Preserve zero-size no-dereference and complete source snapshot for overlap/aliases. |
| GSP command lists | A 1,048,576-byte list allocation occurred before validating an absent or unreadable source. | Retain existing alignment/size bounds and check full source readability before resizing. |
| Strided texture copies | Span-vector growth attempted a 393,216-byte allocation before discovering inaccessible payload ranges. | Walk the compact descriptor without allocation, validate every actual payload span, then build and execute the plan. |
| Renderer reads | Invalid scanout addresses/strides attempted a 26,214,400-byte read-buffer allocation before failing. | Validate the full translated readable span before creating the read buffer. |

An intermediate correction checked map membership only. New fixtures then reproduced failures for **mapped-but-protected** sources. The final code uses `GetReadPointer(address, size)` as the full readability check; global `IsMapped` semantics were not changed. Logs of both the original reproductions and that intermediate failed candidate are retained, rather than presenting the first attempt as sufficient.

Texture-copy gaps are deliberately not treated as payload. Unmapped gaps remain valid, destination gap bytes are preserved, and shared aliases/overlapping DMA retain their original snapshot semantics. The production queue opts into the new prevalidated texture planner; the structural-only planner API remains available for nonexecuting callers. This is not an exhaustive audit of every allocation or transfer API.

Invalid requests remain explicit diagnostic failures. No dropped request is reported as a successfully completed GPU operation.

## Regression results

| Check | Observed result |
|---|---:|
| Fresh Release native CTest suites | 24 / 24 passed |
| Python tests | 28 / 28 passed |
| New GPU suites in a separate GCC component build | 2 / 2 passed |
| Selected ASan/UBSan suites with leak checking | 14 / 14 passed |
| Additional real-renderer ASan/UBSan suite, leak checking disabled | 1 / 1 passed |
| GSP fixture assertions | 143,545 |
| Deterministic strided-copy cases, independent byte oracle | 512 |
| Real-renderer scanout assertions | 790 |

The 512 texture cases compare full destination buffers, including untouched gaps, against an independently written byte-stream oracle. The scanout fixture repeatedly rejects invalid/protected ranges and then renders a valid authored image through real EGL/OpenGL. These are fixtures, not game screenshots. The GCC build covers the two new components; it is **not** a full GCC game build. Its compiler warnings are retained in the build log.

The full native game run below uses Release code, not a whole-program sanitizer build. No new Windows/MSVC, physical controller or speaker test is claimed.

### Open graphics-context leak

The leak-enabled renderer test reports **2,824 bytes in two allocations** at process teardown. A control that constructs/destroys only the unchanged `GlApi`, without PICA, guest memory or a game, reproduces the same report. An unshipped scratch experiment calling `eglReleaseThread` also reproduced it. The context path is isolated, but the exact origin inside the unloaded graphics-library stack is not established. This remains open; the renderer must not be called leak-free.

The separate renderer bounds/undefined-behavior run uses `ASAN_OPTIONS=detect_leaks=0`; the 14 other sanitizer suites use leak checking. `leak-audit.json` and the retained logs distinguish those outcomes.

## Final native game run

The final executable started from an isolated new state directory with the verified USA data. It completed all 296 initializers, started New Game and reached Cherry Tree Hills. The startup fixture and subsequent file-driven HID/pointer/presentation controls are ordinary inputs. Guest code, memory, mission state, timing and cameras were not patched to navigate.

The run then exercised movement, camera control, lower-screen expansion, the pause menu, an ordinary confirmed exit to slot selection, and selection of the existing **Cherry Tree Hills** slot. `actions/` retains the before/after frames and observed heartbeat times for each input. `verified-interactions.json` is the final visual review, not a claim inferred solely from inputs. The slot-selection screen showed an existing slot with 00:42 and 0.0%; this does not establish mission completion or preservation of the exact walking position.

After loading the existing slot, the run returned to interactive 3D; further movement and R-button camera rotation were visually verified in `actions/11-reloaded-forward` and `actions/12-reloaded-camera`. The capture/heartbeat interface is asynchronous, so requested guest hold times are not exact input-edge measurements. Logs are bounded and rotated; the retained log tail is not a complete input/IPC history.

This is **in-process exit/reload coverage**, not a cold application-restart save-recovery test or full save-format validation. Test saves and unsaved movement were created in this isolated run, not taken from the user's own save directory.

| Final run measurement | Observed value |
|---|---:|
| Initializers | 296 / 296 |
| Native block entries | 1,236,963,369 |
| Draw calls | 497,409 |
| Guest time at stop | 164.412 seconds |
| Wall time | 1714.956 seconds |
| Maximum sampled process RSS | 499,941,376 bytes |
| Terminal result | `operator_stop`, exit 4 |

The deliberate requested stop is not a crash, unsupported-service failure, full-game pass or real-time-performance result. Guest time includes startup, loading and menu/paused time; wall duration is not continuous active gameplay duration. Sampled RSS is not an allocator leak measurement. No long-duration soak, completed mission, combat or full-game compatibility is established.

### Rendering clue, not a cutscene fix

The first-load real-time introduction still rendered black environmental surfaces. In the **same final executable and process**, after returning to slot selection and loading that slot, the matching opening scene had visible environmental surfaces. `proof/first-load-black-intro.png` and `proof/same-run-reloaded-intro.png` preserve this contrast.

This points to initialization/persisted state as an investigation lead; it does not by itself identify the bad state or prove that GPU caches versus game camera state are responsible. The earlier Recovery F NaN shadow-camera findings remain historical evidence. No fresh uniform dump is claimed here and no replacement camera/matrix, shadow-disable switch, or forced second load was added. The first-load defect is **not fixed**.

## Final startup preservation check

The original Recovery I ELF and exact final Recovery J ELF separately ran a bounded 2,500-iteration no-input startup. Both ended with the same 23,976,133 block entries, 2,536 draws and all 296 initializers. The top LCD, bottom LCD, composed image and **1,603,244-byte WAV file** are byte-identical. The WAV count includes its header. `final-startup-equivalence.json` records the hashes, bounds and distinct binary identities.

This preserves this deterministic startup interval only; it is not full-game, hardware or audio-device equivalence. Intermediate candidate runs have separate filenames/identities and are not substituted for the final executable.

## Remaining work and package scope

The opening first-load rendering defect, graphics-context teardown leak, deeper transfer/permission auditing, long-session resource/cache growth, mission progression and cold save/reload testing remain open. Windows presentation/build execution, physical speaker/controller testing, performance work and individual second-screen UI-element relocation are still outstanding. Existing corner/expanded presentation continues to operate on whole LCD images, not isolated menu elements or a widescreen camera.

The package includes cumulative source, generated AOT pages, tests, launch/build scripts, the exact tested Linux ELF, reports and actual screenshots. It excludes the original game image, code.bin, RomFS, game assets, raw memory/GPU dumps, local saves and WAV recordings. Historical reports remain for provenance; this report and `reports/CURRENT-STATUS.md` describe Recovery J.

No native test process remains running at delivery. The user does not need to re-upload the game/framework or repeat the old startup tests.