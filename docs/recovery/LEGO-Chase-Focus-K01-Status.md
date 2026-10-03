# LEGO Chase — Focus K01: shadow projection isolation

**September 22, 2026. Diagnostic supplement to Recovery J. Not a rendering fix or a new game executable.**

## Result

A small native test executes four **unchanged** generated AOT pages from Recovery J with authored memory/register inputs. It reproduces the historical projection's positive and negative infinity results without a renderer, shader cache, game image, audio, services, saves, or full-game startup. A finite-range control produces the expected finite projection. This establishes a local arithmetic/data-flow reproduction, not the cause of the complete first-load visual defect.

Clang 17 and GCC 14.2 each pass the same **one focused CTest**, with **seven cases and 435 active assertions** per execution. Their JSON results are byte-identical. Each reported CTest test execution took 0.02 seconds; that excludes configuration and compilation. Only ten translation units were built per compiler: the fixture, four generated pages, and five memory/arithmetic support files. No full test-suite rerun or game execution was performed.

## Newly narrowed data flow

Names such as depth range, near, far, and orthographic below are descriptive mathematical interpretations, not recovered debug-symbol names.

1. The routine beginning at `0x001C1D78` saves its original floating-point inputs. Its tail at `0x001C1D98` subtracts the saved lower input from the saved upper input. The store at `0x001C1DAC` writes this difference to **`0x0037677C`**. The original pointer literal at `0x001C1DBC` is `0x00376778`.
2. The shadow caller at `0x002CA7BC` reads that stored difference. With the caller's constant `s16 = 1.0`, the code at `0x002CA7C4` forms `far = float32(1.0 + range)` and supplies `near = 1.0`.
3. The original setter at `0x0023F168` writes the pair to camera offsets **`+0xC4` and `+0xC8`**. It does not reject equal endpoints.
4. In orthographic mode (camera byte `+0xB8 == 1`), projection routine `0x00227300` computes `far - near`, then divides at `0x002273C4` and `0x002273E8`. For coefficient `camera[+0xD4] = 1.0`, the matrix terms are `1/(far-near)` and `-near/(far-near)`.

The isolated zero-range result is:

```text
near          = 0x3f800000 (1.0)
far           = 0x3f800000 (1.0)
matrix[10]    = 0x7f800000 (+infinity)
matrix[14]    = 0xff800000 (-infinity)
FPSCR         = 0x8300001a (divide-by-zero flag set)
```

These two infinity bit patterns match the retained Recovery F nonfinite log. All four selected AOT pages are byte-identical between Recovery F and Recovery J, verified directly against the two supplied archives. This does **not** convert Recovery F's old game trace into a fresh game run.

## Focused cases

| Authored condition | Expected and observed result |
|---|---|
| Stored range zero | Equal planes; positive and negative infinity |
| Stored range 32 | Planes 1 and 33; coefficients +1/32 and -1/32 |
| Zero again, after the finite control | Equal planes and infinities return; no stale finite matrix retained in this isolated calculation |
| Original writer tail given equal endpoints | Stores zero; same invalid projection |
| Original writer tail given endpoints 16 and 48 | Stores 32; finite projection |
| Stored nonzero range 2^-26 | Addition to 1 rounds back to 1; same invalid projection |
| Stored range 2^-23 | Far is the next representable float above 1; finite coefficients |

All 16 output matrix words are checked against explicit expected bits, as are output guard words, the untouched range input, expected branch/return path, and the divide-by-zero flag. Expected finite values are authored exact constants, not the output of a second call to the same implementation.

The writer test intentionally begins at its arithmetic tail, **after** an unrelated vector-copy call. It supplies an authored restore frame; it does not claim to execute the whole containing function. The projection test begins at the depth-setting caller slice and stops on return from the original projection routine. A strict 16-dispatch bound rejects unexpected paths; there is no interpreter or fallback for other addresses.

## What this does and does not establish

The tested data path can generate the same invalid projection before any graphics context or GPU cache exists. That narrows where to inspect the game's initialization inputs. It does **not** rule out GPU-cache, shader, nonfinite-coordinate, or hardware-behavior differences contributing to the black surfaces.

Crucially, the old trace's equal planes do not uniquely prove that the stored range was exactly zero: the tiny-nonzero control demonstrates one alternative. No live value at `0x0037677C` has been captured in this batch, and no first-load/reload pair of values or uniforms is claimed. The fact that the earlier Recovery J reload rendered visible scenery remains a separate historical observation.

No camera constants were replaced in production, no shadow pass disabled, no mission/timer modified, and no second load forced. Synthetic range changes happen only inside the diagnostic fixture.

## Preservation

- All **1,779** original Recovery J manifest entries were checked before and after the work; none changed.
- All **604** original generated files remain unchanged. The probe compiles four existing pages; it does not regenerate them.
- Existing game ELF remains SHA-256 `9f2518d2f893ffd53bc6073169309bbc5a92ae5048d5474412634fcb9b6a827b`; it was neither rebuilt nor run.
- The baseline ZIP SHA-256 is `6920cfa152eda0f05e02aa97810540cb0e0e14ee4a028e28098bebf7d4d881be`.
- New files are confined to `diagnostics/shadow_projection/` and `reports/focus-k01/`.
- `verification.json` records compiler versions, fixture-executable hashes, input hashes and preserved baseline identity. Test binaries are not distributed in this small supplement.

## Package and next boundary

`LEGO-Chase-Focus-K01.zip` is an **additive diagnostic supplement**: merge its two directories into the existing Recovery J root. It is not a replacement for Recovery J and does not change its default build. The diagnostic README gives optional standalone reproduction commands; no user test is required to continue.

The next investigation target is the actual write/use timing and values for this range on first load versus reload. If live observation is needed, it should watch this small set of addresses rather than restore broad draw-state dumps or an unrelated gameplay sweep. This batch deliberately ends at the verified reproducer; the visual defect is still open.

## Evidence within the baseline and supplement

Historical evidence retained in Recovery J:

- `reports/recovery-f/shadow-camera-nonfinite.log`: original divide trace.
- `reports/recovery-f/shadow-camera-transition.json`: original nonfinite versus finite uniform summaries; not paired J reload data.
- `reports/recovery-j/proof/first-load-black-intro.png` and `same-run-reloaded-intro.png`: earlier visual contrast.

Fresh evidence in this supplement:

- `probe-clang.json` and `probe-gcc.json`: exact synthetic-case results.
- `ctest-clang.log` and `ctest-gcc.log`: focused suite completion.
- `configure-*.log`, `build-*.log`, `linked-libraries-*.txt`: build scope and linkage.
- `verification.json`: source/input identity and unchanged-baseline checks.