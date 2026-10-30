# October 30, 2026 — startup stack-return continuations

## Current continuation

Three metadata-only supplements now cover `0x00150CE8..0x00150D18`:
**12 instruction words / 48 bytes**, split at `0x00150D00` and `0x00150D0C`.
They use the existing fast, memory, TST-immediate and shifted-MOV support.
Runtime evaluators, generator policy and the prior 18 manifest records are
unchanged. The tested candidate was based on **`fe376d0`** with only its manifest
modified; these three status documents were updated after both game runs.
The final source-to-native binding checks all 518 non-documentation files.

Both fresh runs now stop at **MissingBlock `0x00151604`**, each advancing
**457 core-0 instructions / 1,704 diagnostic ns** from its preserved same-mode
post-reset baseline. The source contains **21 supplements / 271 words**;
the native verifies **111,064 blocks / 545,382 instruction words** across the
unchanged **599 original pages / 603 AOT files**.

| Actual fresh measurement | Headless | SDL desktop |
| --- | ---: | ---: |
| Diagnostic stop | MissingBlock `0x00151604` | MissingBlock `0x00151604` |
| Core-0 instructions | 61,371,016 | 61,367,133 |
| Core-1 instructions | 99 | 99 |
| Diagnostic CPU ticks | 2,345,552,963 | 2,345,552,963 |
| Diagnostic guest time, ns | 8,748,411,943 | 8,748,411,943 |
| Thread / dispatch round | 7 / 21,843 | 7 / 21,843 |
| Display periods / presentations | 523 / N/A | 523 / 524 |
| Independently recorded native-child exit | 3 | 3 |
| Outer launcher exit | 3 | 3 |
| Wall timeout | No | No |
| Host duration, seconds | 127.449 | 136.562 |

Exit 3 is the reported diagnostic MissingBlock stop, not a successful boot.
Both modes have empty stderr, and both owned process groups have no live
processes left. No cleanup signal was needed. The desktop's residual Xvfb entry
was a non-live zombie. The current child-status recorder removes the earlier
wrapper/child ambiguity; it does not retroactively establish the prior desktop
child's exit.

## What was tested

A fresh Release build used GCC/G++ 13.3.0, Clang/LLVM 18.1.3, CMake 3.28.3,
Ninja 1.11.1 and the pinned SDL 3.4.18 source, with X11 explicitly enabled.
Configure took 15.48 seconds and the build 92.98 seconds. The first full CTest
execution ran 84 suites: **82 passed; two failed** in their isolated
permission-test children. An owned diagnostic reproduced `setgid(65534)`
failing with `EINVAL` in the default sandbox, while the approved cloud context
allowed both the group and user changes.

Only `ctr_fs_savedata_test` and `ctr_fs_open_test` were rerun in that approved
context. Both passed in 0.114 seconds with unchanged binaries, source and all
606 original inputs. The aggregate therefore records **82 original passes +
2 explicitly corrected passes**. It retains the original exit 8, failed-gate
receipt and both actual JUnits. It does not claim a first-pass 84-suite success,
a rebuild or a repeated full suite. Use the approved cloud context for the next
full native gate so these permission tests can execute their intended paths.

A separate private verifier checks all 271 supplemental CPP/JSON records against
the authentic game words, the 18 preserved manifest records and the original
AOT identities. The intermediate 18-supplement raw bodies lost in the earlier
reset remain absent; their surviving receipt is distinguished from current
actual generated bodies. Current verified supplemental CPP/JSON copies are now
preserved with the new build evidence.

The exact tested native is **15,672,920 bytes**, mode 0755, SHA-256
`75bc8ce7adced4805cec7f0c7904418acedcb9cb5f61d94111d3c3bfd163602e`.
Its full **403,064-byte** build/test receipt, frozen manifest bytes, actual raw
gate logs, JUnits and generated supplemental proofs were saved with the
executable before either game run. Each game run independently checked the
frozen 521-source-file candidate and 606 original inputs before and after,
used fresh owned storage, and had a 600-second bound. Neither timed out.
No additional game rerun was used.

## Comparison and display limits

The two current modes agree on stop context, final registers/TLS, diagnostic
guest time, DSP counters, all **285,726 audio records**, and actual state/save
inventories. Complete audio files match both preserved same-mode baselines.
The desktop-minus-headless **3,883-instruction** difference remains unexplained;
this is not an instruction-for-instruction or hardware-equivalence claim.

The actual first-nonblack desktop image is **1280×720** and shows the startup
loading indicator in the lower-screen inset. Its decoded RGBA pixels exactly
match the preceding capture. The BMP container changed from **32 bits /
3,686,538 bytes** to **24 bits / 2,764,854 bytes**, so raw BMP equality is not
claimed. The lossless PNG remains SHA-256
`d7328d0788266b04023979e5ef8a914fda86afe9ecf315c5268531667786a9a7`.
It is an initial startup capture, not proof of the terminal visual state,
usable menus, gameplay, speaker playback or a working Windows release.

The whole lower screen remains bottom-right. Tab enlarges it, F11 toggles
fullscreen, and the mouse maps to touchscreen coordinates. The future
minimap-only and case-by-case integrations remain separate work.

## Next bounded task

The newly observed gap is one minimal block,
**`0x00151604..0x001516CC` — 50 words / 200 bytes**, ending at its first control
exit before the next existing original block. The checked forms use
**18 fast instructions, 16 CoreMemory instructions and 16 already-admitted
shifted ORRs**. No new opcode policy or evaluator is identified as necessary.

This helper loads eight unknown bytes, replicates each into two words and
stores them at the checked stride. Normal memory checks remain strict; a
multiword store can partially complete before a later fault. If all accesses
succeed and the final return executes from the captured state, unchanged LR
selects the already supplied `0x00150CE8`. No loaded value, completed helper,
exact incoming dispatch site or future successful return is claimed.

Append only this reviewed range in the next implementation, preserving all
21 current entries and all original AOT files. The conditional next totals
would be 22 supplements / 321 words and 111,065 blocks / 545,432 words.

## Current recovery records

Fresh evidence is under `evidence/stack-return-20261030/`: the frozen static
change, initial and corrected gate, actual generated proof, durable native
receipt, saved-native selection, two closed runs, full comparison, screenshot
provenance, and actual-observation-bound next-gap proof. Both closed-run archives
include their real state and save bodies. The combined checkpoint retains
current evidence and prior payloads; original inputs and the native are
separately referenced private assets.

The preceding complete `fe376d0` archive has **9,883 payloads**. Its four leading
metadata files are preserved under
`recovery-records/prior-checkpoint-fe376d0/`; the final packager/verification
must retain every previous payload identity. No current workspace loss occurred
during this batch. The older losses and their unavailable raw bodies remain
explicitly documented below and are not relabeled as recovered.

Publication and automatic hosted-check results are bound to the final commit
in the private continuation receipt. This manifest-plus-documentation change
selects the ROM-free checkpoint and source-recovery workflows; it does not
select the hosted CTR/SDL workflow. Keep that scope separate from the actual
local desktop execution above.

---

Previous checkpoints follow verbatim.

---

# October 30, 2026 — preserved build and fresh evidence after another workspace loss

## Current continuation

The TST implementation is published in **`f37e172`**, with sole parent
**`2fec57b`**. Its seven new blocks / 29 words remain unchanged. The exact saved
native executable has now reproduced **MissingBlock `0x00150CE8`** in fresh
headless and desktop runs after a second temporary-workspace loss. Both advance
**238 core-0 instructions / 888 diagnostic ns** from their preserved `2fec57b`
same-mode baselines. This follow-up changes status documentation only.

| Fresh measurement | Headless | SDL desktop |
| --- | ---: | ---: |
| Guest-reported stop | MissingBlock `0x00150CE8` | MissingBlock `0x00150CE8` |
| Issued core-0 instructions | 61,370,559 | 61,366,676 |
| Issued core-1 instructions | 99 | 99 |
| Diagnostic CPU ticks | 2,345,552,506 | 2,345,552,506 |
| Diagnostic guest time, ns | 8,748,410,239 | 8,748,410,239 |
| Thread / dispatch round | 7 / 21,843 | 7 / 21,843 |
| Display periods / SDL presentations | 523 / N/A | 523 / 524 |
| Observed process exit | 3 | 5, wrapper cleanup failure |
| Wall timeout | No | No |
| Wall duration, seconds | 151.019 | 142.722 |

Desktop writes its complete terminal diagnostics, **285,726 audio records** with
`close_ok=1`, and the expected screenshot. Its `xvfb-run` wrapper subsequently
reports a temporary-directory cleanup error and returns **5**. Inspection of the
installed wrapper confirms that its EXIT trap replaces the saved child status
with 5 on this cleanup failure. The guest child exit was not independently
recorded and is not relabeled as 3. The remaining Xvfb process in that run's
recorded process group was closed separately; no live process remains in the
group. The cleanup failure's underlying filesystem cause is untraced. The game
was not rerun to obtain a cleaner wrapper status.

The two fresh modes agree on stop context, terminal registers/TLS, guest time,
complete audio bytes and current filesystem inventories. The **3,883-instruction**
desktop/headless difference remains untraced. Audio matches the preserved prior
files byte-for-byte. The **1280×720** first-nonblack desktop BMP also matches the
prior `2fec57b` capture byte-for-byte; its lossless PNG preserves every pixel.
It shows the lower-screen loading indicator in the bottom-right inset. Menus,
gameplay, speaker playback and a working Windows release remain unproven.

## What survived and what was lost

During publication, the temporary source checkout, inputs, build tree and current
raw evidence disappeared. The cause of this second loss was not established.
The published source, separately saved native archive and preceding complete
checkpoint survived. Recovery authenticated **all 9,527 prior payload bodies**,
the prior manifest, **521 prior Git source blobs**, **606 original input entries**
and the exact current executable. The older `2fec57b` game runs were not repeated.

The saved executable archive contains an authentic **285,392-byte build-test
receipt** with the original successful **84-suite** gate result, its source/input
snapshots, generated-supplement verification, **26 Python test-method** count and
**55 fixed TST-vector** count. Its SHA-256 is
`cae2997a319bd6740da42d291361fb41e9f6030df08ecc4e4632163ba52043af`.
The executable remains SHA-256
`e7743a69587f0edc0caf09ba07966870ab7568cecd9d68f7049d81aac6f8589a`.
All **518 non-documentation source files** match that saved tested snapshot.

The pre-reset current raw build/CTest/JUnit logs, game captures, setup-failure
outputs, local-closure body and detailed next-gap proofs were **not recovered**.
Their historical descriptions below record observations made before the loss;
they are not proof that those bodies now exist. The durable build receipt retains
the build/test and generation summaries with their recorded identities. The
pre-reset current game results and later validation steps survive only as
conversation observations, not as recovered raw evidence.
The new runs, comparisons, closure and static next-gap map are explicitly marked
as fresh post-reset evidence. The separately restored `2fec57b` historical
checkpoint is intact.

Each fresh run authenticates the saved native, all 606 original inputs and its
clean **521-file** `f37e172` checkout before execution, then preserves before/after
proofs. Both use the same finite profile, fresh owned storage and 600-second
process-group bound. The executable is reused directly. The original local test
gate was not repeated. Complete fresh captures, including current state bodies,
were saved separately as soon as each closed.

Prior `2fec57b` SharedExtData state bodies remain absent because that archive
deliberately excluded `state/`; old-state comparisons use authenticated receipt
inventories and hashes. Prior audio and SaveData comparisons use restored actual
files. Current fresh storage comparisons use actual files. These scopes remain
distinct in the result verifier.

The exact implementation commit passes **3 automatic workflows / 8 first-attempt
jobs**, including **five complete 83-suite inventories (415 passes)**, sanitizer
checks, the X11 PiP smoke, source snapshot and ROM-free package. Complete hosted
logs and API receipts are preserved. Documentation-only follow-ups have different
workflow path filters, so their own automatic checks are recorded separately.
A new **16-check local closure** passes, explicitly preserving the desktop wrapper
error and the raw-evidence limitations above.

## Resume from the fresh checkpoint

Current evidence is under `evidence/tst-dispatch-postreset-20261029/`:

- `durable-native-build-test-receipt.json` and `native-input-restoration.json`.
- `prior-checkpoint-restoration.json` and `workspace-loss.json`.
- `game-headless/`, `game-desktop/`, `game-results-verification.json`.
- `desktop-wrapper-cleanup.json`, `local-closure.json`, and `screens/`.
- `next-gap/static-next-gap-00150ce8.json` and `next-gap/fresh-observation-binding.json`.
- Separate capture-save receipts and the full combined checkpoint's continuation receipt.

**Next:** add only the three reviewed blocks at `0x00150CE8..0x00150D00`,
`0x00150D00..0x00150D0C` and `0x00150D0C..0x00150D18`, **12 words** in total.
Fresh static review of authenticated inputs confirms that existing admissions and
evaluators cover them. The stack-controlled test outcome and final POP target
remain unknown; literal table data beginning at `0x00150D18` stays excluded.
The fresh terminal observation binds this plan's entry without claiming an
observed instruction-by-instruction path or future branch outcome.

Public Git remains source and metadata only. The following historical sections
are retained byte-for-byte, with their evidence scope qualified above.

---

# October 29, 2026 — TST dispatch advances startup to the next return continuation

## Current result

The unchanged original game now passes the **`0x00150A54`** gap. Seven
authenticated build-time supplements cover `0x00150A54..0x00150AC8`, adding
**29 instruction words / 116 bytes**. Both headless and SDL desktop advance
**238 issued core-0 instructions** beyond their respective previous runs and
stop at **MissingBlock `0x00150CE8`**, with PC and LR both at that address.
The actual path through the dispatch region has not been captured instruction
by instruction; reaching the new stop does not prove every added block ran.

There are now **18 supplements**, with **111,061 registry blocks / 545,370
verified instruction words**. All **599 original AOT pages / 603 files**, the
previous **11 supplements / 230 instruction records**, and original game
inputs remain unchanged. The generated additions contain 23 existing fast-path
operations, three existing memory operations and three canonical immediate TST
operations. Generated game words and C++ remain private.

| Measurement | Headless | SDL desktop |
| --- | ---: | ---: |
| Terminal stop | MissingBlock `0x00150CE8` | MissingBlock `0x00150CE8` |
| Thread / dispatch round | 7 / 21,843 | 7 / 21,843 |
| Issued core-0 instructions | 61,370,559 | 61,366,676 |
| Increase from previous run of the same mode | 238 | 238 |
| Issued core-1 instructions | 99 | 99 |
| Diagnostic CPU ticks | 2,345,552,506 | 2,345,552,506 |
| Diagnostic guest time, ns | 8,748,410,239 | 8,748,410,239 |
| Display periods | 523 | 523 |
| SDL presentations | N/A | 524 |
| Guest exit / wall timeout | 3 / no | 3 / no |
| Wall duration, seconds | 183.873 | 206.866 |

The finite runs retain the previous profile: **131,072 host events**,
`--block-limit 100000000`, a **600-second wall bound**, and fresh owned SaveData
and SharedExtData. The runner owns and bounds its complete process group.
Diagnostic time is not hardware timing. Wall durations were measured after
restoration into a fresh cloud environment; no performance comparison is made.

The two completed runs have identical terminal registers/TLS, stop context,
guest time, DSP results, complete audio bytes and current filesystem inventories.
Desktop still issues **3,883 fewer core-0 instructions**. It enables guest-visible
HID sampling; the cause of the instruction-count difference remains untraced.
No instruction-for-instruction equivalence is claimed.

Both full **4,571,648-byte / 285,726-record** audio files also match their prior
same-mode files byte-for-byte, retaining SHA-256
`83a7d04b449af60230fd50b5b116075738663f0244b466644a4d0acafa3eb444`.
Restored prior SaveData bodies match current inventories. Prior SharedExtData
`state/` directories were deliberately excluded from the saved archive; their
comparison uses authenticated completion-receipt inventories and hashes only.
Those old state bodies were not recreated or claimed to survive recovery.

## Desktop and validation

The SDL layout remains the whole lower screen in the bottom-right corner,
with Tab enlargement, F11 fullscreen and mouse touch mapping. The actual game
presents **524 frames** and captures a **1280×720** first-nonblack startup image.
The BMP is byte-identical to the preceding desktop capture: the lower loading
indicator appears in the inset while the main screen is black at that moment.
A new lossless PNG export retains every pixel. This is startup evidence;
usable menus, gameplay and speaker playback remain unproven. Linux is the tested
host; Windows execution and its storage backend remain unfinished.

The first desktop invocation failed during SDL initialization because the
restricted cloud environment could not establish X11 display sockets. The
recorded startup order places that failure before guest initialization and
execution. Its completion, output, empty SaveData and audio header are retained
in `game-desktop/`. A separate synthetic X11 smoke passed after the local socket
restriction was resolved. One explicit corrected invocation, `game-desktop-v2/`,
then completed with the **same executable, source, inputs and SDL settings**.
No automatic game retry occurred. The failed initialization is excluded from
the game-result comparison and preserved as separate evidence.

The single fresh Release configure/build/full-test gate passes **84/84 suites**.
The Python supplement-policy suite expands from 17 to **26 test methods**.
Independent C++ fixtures cover **55 fixed TST vectors**, all immediate rotations,
carry preservation and replacement, N/Z/V behavior, passing/failing conditions,
SP/LR operands, unchanged registers and both outcomes of a TST-driven branch.
The existing runtime evaluator is unchanged. Release checks remain enabled.
The separate X11 software-renderer smoke uses colored synthetic panels.

Only canonical A32 immediate TST gains admission: condition 0..14, primary
class 001, opcode 1000, S=1, unused Rd=0 and non-PC Rn. The complete rotated
immediate is retained. Existing shifted MOV/ADD/ORR admission is unchanged;
other forms, malformed encodings, PC operands and unknown disassembly remain
rejected. No address or mnemonic acts as an opcode permission.

Generation verification binds the emitted records to authenticated code bytes,
the exact compile entry, all original AOT files, preserved prior records and the
tested executable. JUnit preserves all 84 results and the 26-method Python output.
A later CTest inventory command reset its temporary `LastTest.log`; that short
file is not used as the full test log. No test rerun was needed. A final
**16-check local closure** verifies the preserved results and frozen candidate.
Only the three status/documentation files change after the closure. Hosted CI
must be read for the exact published commit; it is separate from these local runs.

## Recovery and next task

Workspace cleanup had removed the local checkout and temporary files. Recovery
restored and verified all **9,527 payload bodies** in the saved `2fec57b`
checkpoint, all **521 real Git source files**, the **606 original input snapshot
entries**, and the exact preceding native executable. No older game runs were
repeated and no replacement game input was requested.

The newly tested executable was saved privately **before** game testing:

- Archive: `LEGO-Chase-tst-dispatch-native-20261029.tgz`.
- Executable SHA-256: `e7743a69587f0edc0caf09ba07966870ab7568cecd9d68f7049d81aac6f8589a`.
- Current evidence: `evidence/tst-dispatch-20261029/`.
- Test gate: `gate-v1/`; generation proof: `generated-verification.json`.
- Actual completed runs: `game-headless/`, `game-desktop-v2/`.
- Retained setup failure: `game-desktop/`; correction proof: `display-setup/`.
- Result comparison: `game-results-verification.json`; closure: `local-closure.json`.
- Static next-gap proof: `next-blocker-00150ce8.json` and `next-gap-00150ce8/`.

**Next:** add the following three authenticated continuation blocks using the
existing admission policy and evaluator. This is a static plan; these ranges
have not been added or executed in the current milestone.

| Entry | End exclusive | Instruction words |
| --- | --- | ---: |
| `0x00150CE8` | `0x00150D00` | 6 |
| `0x00150D00` | `0x00150D0C` | 3 |
| `0x00150D0C` | `0x00150D18` | 3 |

The original LR assignment at `0x00150CD4` statically establishes continuation
`0x00150CE8`, matching observed PC/LR. The exact incoming dynamic path remains
untraced. A forthcoming load replaces r4 from **`[0x0E01FDAC]`**, whose value is
absent from the terminal dump; the next bit-5 test outcome is unknown. If that
test falls through with the observed selector 3, the existing immutable table
selects already supplemented `0x00151B48`. The final POP loads its return target
from the stack; that target is unknown and must pass normal registry validation.
Literal table data begins at `0x00150D18` and is excluded from executable ranges.

Public Git contains source and metadata. Original inputs, generated game code,
executables, captures, audio and saves remain private. All preceding historical
sections below remain byte-identical and are not relabeled as fresh evidence.

---

# October 29, 2026 — live desktop PiP and the next dispatch gap

## Current result

The optional SDL3 frontend now presents both live LCD images in one resizable
window: the main image retains its aspect ratio, and the whole lower screen sits
in the bottom-right corner. Tab enlarges or restores the lower panel, F11 toggles
fullscreen, and mouse input maps to the original 320×240 touch coordinates.
The source crop and destination rectangle are separate so a later verified
minimap crop can replace the full lower-screen source. No game-specific crop is
active. See [desktop build, launch flags and controls](../DESKTOP.md).

The original-game X11 run presents **524 frames**. Its actual SDL renderer
readback produces a **1280×720** first-nonblack screenshot showing the lower-screen
loading indicator in the inset, with the main screen black at that early moment.
This is actual host-window output, distinct from the independently authored
colored-panel smoke fixture. It does not establish usable menus or gameplay.
The screenshot is preserved privately as BMP and a pixel-identical PNG export.

The two previously mapped blocks at `0x001535E0..0x0015368C` and
`0x0015368C..0x00153690` are now authenticated build-time supplements. They add
**44 words / 176 bytes**, bringing the supplement count from nine to **11**.
The original **599 AOT pages / 603 files**, earlier nine supplement records and
original game inputs are unchanged. No opcode admission policy expands in this
milestone. The registry now has **111,054 blocks / 545,341 instruction words**.

The headless run advances **282 issued core-0 instructions** beyond the prior
`0x001535E0` stop. Its terminal PC is the real stack-loaded return target
**`0x00150A54`**, with SP **`0x0E01FDAC`** and live LR **`0x144F7F00`**.
No LR substitution or forced return is used. The terminal result and instruction
count agree with the reviewed eight-iteration loop and POP path; no private
per-instruction observational capture was added for this milestone.

| Measurement | Headless | SDL desktop |
| --- | ---: | ---: |
| Terminal stop | MissingBlock `0x00150A54` | MissingBlock `0x00150A54` |
| Thread / dispatch round | 7 / 21,843 | 7 / 21,843 |
| Issued core-0 instructions | 61,370,321 | 61,366,438 |
| Issued core-1 instructions | 99 | 99 |
| Diagnostic CPU ticks | 2,345,552,268 | 2,345,552,268 |
| Diagnostic guest time, ns | 8,748,409,351 | 8,748,409,351 |
| Display periods | 523 | 523 |
| SDL presentations | N/A | 524 |
| Guest exit / wall timeout | 3 / no | 3 / no |
| Wall duration, seconds | 125.068 | 123.676 |

Both fresh runs use **131,072 host events**, `--block-limit 100000000`, a
**600-second wall bound** and fresh owned SaveData/SharedExtData. The private
runner owns a separate process group and terminates that entire group on a wall
timeout. No automatic retry occurred. Diagnostic time is not hardware timing.
The last `fs:RomFS` request is historical, not the current blocker.

The terminal stop, all printed CPU/TLS registers, guest time, DSP measurements,
complete recursive filesystem inventories and complete **4,571,648-byte /
285,726-record** audio streams match between the two runs. Audio and filesystem
state also match the preceding checkpoint. Audio SHA-256 remains
`83a7d04b449af60230fd50b5b116075738663f0244b466644a4d0acafa3eb444`.

**The runs are not instruction-for-instruction equivalent:** desktop issues
**3,883 fewer core-0 instructions**. It enables guest-visible HID samples and
signals, while headless preserves the previous unsampled behavior. Read-only
inspection finds no direct change to the issued-instruction counter, but the
cause of this execution difference has not been traced. Preserve the difference
as an observation; do not infer a hardware-equivalence or causal proof.

## Implementation and validation

Desktop support is opt-in (`LEGO_BUILD_DESKTOP=ON`, `--frontend sdl`). Headless
remains the default and requires no SDL dependency. CMake accepts an installed
SDL3 >= 3.2 or fetches the hash-pinned official SDL 3.4.18 release. The local
validated build uses SDL 3.4.18 and the X11 software renderer.

Scanout decodes the verified rotated RGB8 buffers in device VRAM, using live
GSP-selected addresses, stride and format, plus explicit LCD force-black.
Unsupported formats, fills, geometry or addresses fail explicitly. Keyboard and
touch records publish at existing diagnostic display deadlines, with guest
system-tick timestamps and the real HID events. Host event polling does not
advance guest time. Short presses remain pending until successful HID publication;
focus loss releases input. Minimize/restore handling is implemented, but Xvfb
without a window manager could not exercise that branch in the X11 smoke.

The successful local gate passes **84/84 suites**: 83 default suites plus the
optional SDL smoke. New independent fixtures cover scanout/orientation, screen
layout and inverse touch mapping, HID records/events, runner callbacks, short
input latching, synthetic SDL rendering/readback and the bounded loop/stack-return
behavior. The existing Python admission suite still has **17 cases**. A separate
actual X11 smoke also passes. The first build attempt failed on two missing
fault-pointer arguments in a new test; the corrected second gate passes. Both
attempts and their distinct source snapshots are retained.

The code and original inputs remain unchanged through both game runs. A final
**20-check local closure** verifies the test result, source/input identities,
selected executable, actual run receipts, matching terminal state/audio/storage,
and lossless image conversion. Documentation alone changes after that closure.
Hosted CI adds an independent ROM-free X11 smoke job; full default CI jobs
exercise 83 suites. Hosted results must be read for this exact published commit.

Linux is the validated host. Windows/macOS builds and execution are untested;
Windows SaveData/SharedExtData support is not implemented. Audio remains a DSP
record capture; this frontend does not play it through speakers. Menus, gameplay,
real-time performance and hardware-speed input cadence remain unproven.

## Private checkpoint and next task

The intact `8fd18d4` checkpoint was used. The exact tested executable was saved
privately **before** either game run:

- Executable SHA-256: `a1001c1f477cdc5a455320d98240374bb99e90d357508a778efa77ee16b0a255`.
- Archive: `LEGO-Chase-desktop-pip-native-20261029.tgz`.
- Current evidence: `evidence/desktop-pip-20261029/` under the recovery root.
- Build attempts: `gate-v1/`, `gate-v2/`; dependency provenance: `dependency/`.
- Actual runs: `game-headless/`, `game-desktop/`; separate fixture: `smoke-x11/`.
- Result comparison: `game-results-verification.json`; closure: `local-closure.json`.
- Actual image and lossless provenance: `screens/`; AOT verification: `aot/`.
- Static follow-up map: `next-gap/`. This map is analysis, not executed continuation.

**Next:** verify the narrow TST-immediate admission required at the observed
`0x00150A54` gap, add independent policy and flag-semantic tests, and admit only
the bounded reviewed dispatch intervals. The existing evaluator supports the
operation, but the supplement generator deliberately rejects it under its current
narrow policy. The controlling stack word is absent from the terminal register
dump, so the next conditional path is unknown. Keep all registry/memory checks
and let the next finite run establish the actual path. No opcode guard should be
removed wholesale to make progress.

The mapped follow-up is `0x00150A54..0x00150AC8`, **seven nonoverlapping blocks /
29 words**, containing three TST-immediate instructions. The other four blocks
already pass static rendering under the current policy. Direct conditional exits
reach recorded `0x00150F08`; the final join is recorded `0x00150C90`. If the first
conditional falls through with the observed selector, its immutable-table
selection reaches already supplemented `0x00151B48`. This is a conditional static
deduction, not an observed branch. The private `next-gap/candidate-ranges.json`
and `next-gap/bounded-next-gap.json` preserve exact boundaries and proof.

Public Git contains source and metadata. Original game inputs, generated game
words/C++, executable bodies, captures, audio and saves remain private. The
previous checkpoint and older recovery records below remain byte-identical
historical sections; their claims are not relabeled as fresh evidence.

---

# October 29, 2026 — authenticated AOT dispatch and the next two-block gap

## Current result

The unchanged original game now passes the missing dispatch at **`0x00151B48`**.
Five authenticated build-time supplements add **167 instruction words / 668 bytes**,
bringing the supplement count from four to **nine**. The original **599 AOT pages**,
original four supplement instruction records and runtime semantics are unchanged.

The actual first visit executes the nine-instruction dispatcher, selects
**`0x00151AE4`**, executes its 21 instructions and returns to **`0x00150F40`**.
All **30 instruction records** form a complete, nontruncated chain with the same
TLS value. No unique kernel-thread identity is inferred merely from that value.
The other three leaves are statically reviewed and covered by independent
synthetic dispatch fixtures; they were not observed executing in this run.

| Measurement | Current result |
| --- | --- |
| Terminal stop | **MissingBlock `0x001535E0`**, thread **7**, round **21,843** |
| Core-0 instructions | **61,370,039**, an increase of **96** |
| Core-1 instructions | **99**, unchanged |
| Diagnostic guest time | **8,748,408,299 ns**, an increase of **358 ns** |
| Local gate | **80/80 suites**, one configure/build/full gate |
| Ordinary/capture comparison | **21/21 strict checks** |
| Independent first-visit verification | **30/30 transitions and state boundaries**, three observed admitted operations checked arithmetically |
| Final source/input/evidence closure | **21/21 checks** |

Both runs exit with diagnostic status **3** before the event cap. The fixed
profile is **131,072 host events**, `--block-limit 100000000`, a **600-second guest
wall timeout** and **630-second outer guard**, with fresh owned SaveData and
SharedExtData. In diagnostic mode the block limit bounds instructions between
SVC boundaries. Guest time and CPU ticks are diagnostic measurements, not a
claim of hardware timing accuracy.

The complete **4,571,648-byte / 285,726-record** audio streams match each other
and the preceding checkpoint byte-for-byte. Both runs also agree on normalized
launch arguments/stdout, final CPU/TLS, stop context and complete recursive
filesystem inventories. The last `fs:RomFS` request is historical; it is not the
cause of this missing-code stop.

Both active headless framebuffers are black after the earlier logos cleared.
Actual LCD color-fill words are zero, so force-black is disabled. Three lossless
PNG exports preserve the current active buffers and composite. A menu, gameplay,
host-window presentation and audible playback remain unproven.

## Change and verification scope

`tools/generate_aot_supplement.py` now admits only three reviewed forms from the
otherwise rejected broad ALU-candidate category: **MOV, ADD and ORR with register
operands and immediate shifts**. Conditional encodings, canonical MOV fields and
non-PC operands/destinations are enforced. Register-controlled shifts, exception
returns and unrelated ALU families remain rejected. Existing fast-path computed
branches and returns retain their existing policy. No runtime ALU evaluator or
general frontend implementation changes are included.

The added intervals are `0x001518C0..0x001519A4`, `0x001519A4..0x00151A48`,
`0x00151A48..0x00151AE4`, `0x00151AE4..0x00151B38` and
`0x00151B48..0x00151B6C`, with exclusive ends. They contain **57, 41, 39, 21 and
9 instructions** respectively. The 16-byte literal table at
`0x00151B38..0x00151B48` is excluded from executable intervals. Every interval
fits the existing 256-byte and single-page bounds; no original registry entry
is overwritten.

The new Python suite has **17 cases** and is registered as the 80th CTest suite.
The public ROM-free packager also recognizes its literal CMake registration.
Expanded C++ fixtures use independently authored words and fixed expectations
for shifts, flags, conditions, aliasing, SP/LR operands, four-way dispatch and
strict missing-block behavior. Their CHECK macro remains active in Release.

A private observational executable wraps the original recorded step once and
returns its unchanged result. It records const CPU/op/result values, uses bounded
storage and performs no guest-memory writes or result substitution. Separate
verification binds the trace to the authenticated code, generated metadata,
reviewed source, actual run, selected executable and full test gate. It checks
all actual PC/result transitions and serialized-state continuity. Independent
integer arithmetic verifies the three observed newly admitted operations:
one MOV and two ORRs. Memory register/writeback effects and the authenticated
table-read value are checked; general memory contents are not independently
replayed. No broader memory or hardware-equivalence claim is made.

## Current private evidence

This continuation used the intact `5b4933e` checkpoint; no recovery rebuild was
needed. Both exact new executables were archived and saved privately **before**
their game runs:

- Native SHA-256: `c6f30df0bcfeea7e9125c9558d78ba9474c5d39750e69077a5c7a1d51b838bcd`.
  Archive: `LEGO-Chase-aot-dispatch-native-20261029.tgz`.
- Capture SHA-256: `75902da9c529545b11ff19d25c8094ddbf0792a139ee49fc4f5cf366d3b790bc`.
  Archive: `LEGO-Chase-aot-dispatch-capture-20261029.tgz`.

Current evidence paths relative to the recovery root:

- `evidence/aot-dispatch-20261029/gate/`: configure/build logs, full 80-suite console/JUnit/LastTest results, source/input snapshots and selected-native preservation.
- `evidence/aot-dispatch-20261029/runs/{native,capture}/`: approved controls and closed bounded-run receipts.
- `evidence/aot-dispatch-native-131072-sfff_diz/`: actual ordinary run.
- `evidence/aot-dispatch-capture-131072-9vgn6szy/`: actual capture, complete audio and strict comparison.
- `evidence/aot_dispatch_capture/`: reviewed reversible capture helpers and build provenance.
- `evidence/aot-dispatch-20261029/aot-first-visit-verification.json`: independent first-visit proof.
- `evidence/aot-dispatch-20261029/screens/actual-capture-v1/`: lossless images, active-buffer provenance and image closure.
- `evidence/aot-dispatch-20261029/closed-verification.json`: final 21-check closure before documentation edits.
- `evidence/aot-dispatch-20261029/next-gap/`: authenticated static follow-up proof and proposed metadata.

Only source and metadata are published. Raw game words, generated game C++, game
inputs, executable bodies, captures and audio remain private. The earlier recovery
and priority sections below are retained byte-identically as historical records;
their lost original evidence has not been restored or relabeled as current.

## Exact next task

Add only the two statically reviewed follow-up intervals:

| Entry | End, exclusive | Instructions | Exit |
| --- | --- | ---: | --- |
| `0x001535E0` | `0x0015368C` | 43 | NE loop to aligned suffix `0x00153604`; otherwise fall through |
| `0x0015368C` | `0x00153690` | 1 | POP loads PC from guest stack |

These are **44 words / 176 bytes**: 26 existing CoreMemory operations, 17 existing
fast-path operations and one already-admitted MOV with immediate ASR. No opcode
policy expansion is needed. The loop initializes r12 to eight and decrements it
once per iteration; **282 instructions through POP** is a static inference if
all memory operations complete, not an observed continuation result.

At the current stop, entry SP is `0x0E01FD9C`; after the reviewed push and loop,
the POP would load PC from **`0x0E01FDA8`**. That word's value is unknown.
**Do not substitute live LR `0x0015309C` for the loaded return address.** Keep
existing memory and registry checks; unsupported return states must remain
explicit. Let the actual bounded run identify the return or next blocker. The two
proposed blocks have not been added to the manifest or executed in this checkpoint.

---

# October 29, 2026 — recovery of the published priority checkpoint

The tested runtime remains code commit
[`598fe338f41d5af57a21e622ca1566bfc5909b59`](https://github.com/GTTeancum/Lego-Undercover-3DS-Recomp/commit/598fe338f41d5af57a21e622ca1566bfc5909b59).
A subsequent workspace reset lost this milestone's original build/test logs,
raw ordinary/capture evidence and both compiled executables before their
private checkpoint was saved. **Those original files remain lost.** The saved
`284e3d1` evidence and authenticated game inputs were restored. The results
below are fresh recovery reproductions; this update changes documentation only.

## Verified recovery result

| Check | New result |
| --- | --- |
| Release build and local gate | One build; **79/79 suites passed**. |
| Ordinary/capture equivalence | **21/21 strict checks passed**, including complete audio and filesystem state. |
| Actual priority calls | **5/5 succeed**; the previous blocker changes thread **12**, `WaitArb`, **48 → 47**, Result **0**, without waking it. |
| Retained capture bodies | **14/14** identities verified; their aggregate manifest matches the retained historical digest. |
| Final evidence closure | **24/24 checks passed**. |
| Hosted CI for code commit `598fe33` | **3 workflows / 7 jobs pass**, including **5 × 79** suite passes. |

Both fresh runs stop at **MissingBlock `0x00151B48`**, thread **7**, dispatch
round **21,843**, guest time **8,748,407,941 ns**. They use **131,072 host events**,
`--block-limit 100000000` (the diagnostic bound between SVC boundaries), a
**600-second wall timeout** and **630-second outer guard**, with fresh owned
SaveData/SharedExtData. The last `fs:RomFS` request is historical.

The complete **4,571,648-byte / 285,726-record** audio stream matches its
retained historical SHA-256. All fourteen new payload identities also reproduce
the historical aggregate-manifest digest, covering the three individual hashes
that were unavailable separately. This establishes matching new outputs;
it does not restore the missing original files.

Both active headless framebuffers are black, with LCD fill **0** and force-black
disabled. The new lossless export matches the recorded historical PNG digest.
The export does not establish a usable menu, gameplay or host-window presentation.
The original **599 AOT pages** and **four supplements** are unchanged.

## Current private artifacts

The new executable hashes differ from the lost originals. The changed Teakra
library embeds the new checkout path; this is a plausible contributor to the
identity difference, not a bytewise explanation of unavailable original files.
Each new executable is saved in its own private archive:

- Native SHA-256: `62d69f293cb7737e9ef3ea3634e0809593b80397c46e184019210c098a943479`.
  Archive: `LEGO-Chase-recovery-native-598fe33.tgz`.
- Capture SHA-256: `aee57cef7b9e294138bf5865f7968a83d50a7edb4c82842db18aa5dd08769307`.
  Archive: `LEGO-Chase-private-recovery-capture-598fe33.tgz`.

Current private evidence paths, relative to the recovery root:

- `recovery-records/thread-priority-recovery-restoration.json`: restored prior bytes, inputs and recorded verification corrections.
- `evidence/thread-priority-recovery-build.json`, `evidence/thread-priority-recovery-tests.{json,log,xml}` and `evidence/thread-priority-recovery-LastTest.log`: new build and complete local gate.
- `evidence/thread-priority-recovery-native-131072-hu_25kfg/`: new ordinary run.
- `evidence/thread-priority-recovery-capture-131072-2x3cot2l/`: new capture, strict comparison, five-call proof and historical aggregate comparison.
- `evidence/thread-priority-recovery/capture-run/closed-verification.json` and `evidence/thread-priority-recovery/screens/provenance.json`: current closure and image provenance.
- `evidence/hosted-ci-598fe33/`: exact code-revision hosted CI.
- `evidence/thread-priority-recovery/native-saved.json` and `evidence/thread-priority-recovery/capture-saved.json`: successful separate private executable preservation.

Only source and metadata are public; game-derived executable bodies, raw game
inputs, captures and audio remain private. The earlier priority section below
is retained **byte-identically as a historical account**; its original-run paths
and hashes are not substitutes for the current recovery evidence above.

**Next:** verify the missing A32 fragment at `0x00151B48`, its instruction-admission
policy and four unrecorded branch targets before adding authenticated supplements.
The detailed next-task analysis below remains applicable.

---

# October 29, 2026 — active thread priority changes and the next AOT gap

## Current result and next task

The unchanged original game now passes the **SetThreadPriority** call that
stopped the extended startup run at `0x0010B1B0`. The real target is thread
**12**, which has already executed and is waiting on an address arbiter.
Its priority changes from **48 to 47** with Result **0**, while it remains in
`WaitArb`; pending-wake state, stored CPU context and guest time do not change.
The original call resumes at `0x0010B1B4`.

The current ordinary run reaches **MissingBlock**, thread **7**, dispatch round
**21843**, PC **`0x00151B48`**, guest time **8748407941 ns**. It executes
**61369943 core-0 instructions**, which is **113624 more** than the pre-fix
priority stop. The dispatch-round counter counts SVC boundaries, so it need
not increase while ordinary instructions advance between them.

**Next: verify and translate the missing A32 region beginning at
`0x00151B48` using the authenticated build-time supplement workflow.** The
address is absent from the original registry. Initial static inspection reaches
an instruction classified `core_alu_candidate`, which the existing supplement
generator deliberately rejects. Review that exact instruction's semantics,
control-flow exits and generator admission policy before adding a range; do
not remove the guard or insert a forced return merely to advance execution.

The first fragment is **nine instructions / 36 bytes**, ending at exclusive
`0x00151B6C`. Its shifted flag-setting MOV already has runtime evaluator support,
but that does not establish safe admission of the generator's broad candidate
category. The fragment ends with a four-way PC-relative indirect dispatch to
`0x00151AE4`, `0x00151A48`, `0x001519A4` or `0x001518C0`; all four entries are
unrecorded. The captured register values imply the first target under static
evaluation, but no execution beyond the missing entry has been observed.
Review the resulting block graph and exits and add independently authored
admission/semantic tests before changing the supplement manifest.

The current last router request is **historical** `fs:RomFS`, header
`0x080200C2`; it is not an unsupported IPC. Both active display buffers are
black at this later checkpoint, with LCD force-black disabled. The extended
pre-fix run had already cleared the startup logos and had the same black
active buffers. Earlier actual startup-logo exports remain valid historical
evidence; this later image does not establish a usable menu or gameplay.

## Why the previous startup window was extended

The prior **65536-host-event / 240-second** window ended at its event cap
while startup branding was visible. A distinct run of the exact published
`284e3d1322dfada688a44407cda6c47470bac3ae` native executable used
**131072 host events**, the existing **`--block-limit 100000000`** setting,
and a **600-second wall timeout**, with a **630-second outer guard**.
In diagnostic mode, the block-limit setting bounds instructions between SVC
boundaries.

That run stopped at **UnsupportedSvc 0x0C**, PC `0x0010B1B0`, thread **7**,
round **21843**, guest time **8747984148 ns**. The request used handle
`0x0012803A` and priority **47**. The former implementation only supported a
fresh Ready thread that had never executed. A matching capture passed all
**21 strict comparison checks** and preserved all **232 capture members**.
This identifies a concrete runtime boundary instead of inferring a hang from
the earlier event-limited snapshot.

## Supported priority behavior

`Kernel::SetThreadPriority` now accepts active **Running**, **Ready**,
**WaitSleep**, **WaitArb**, **WaitSynchAny** and **WaitSynchAll** targets when
they neither own a mutex nor have a mutex wait registration. It supports the
current-thread pseudo handle and executed targets, and preserves pending
wake results and indices. The shared SVC bridge handles command **0x0C** in
both strict and diagnostic runners; the previous diagnostic-only interception
has been removed.

Validation preserves the established order: unsigned priority range **0..63**,
typed thread handle, then the caller process's priority resource limit
(**24** in this runtime). Invalid requests return the corresponding guest
Result. An unsupported dependency leaves the original call and its registers
untouched. A successful call changes only the target's priority before the
normal runner SVC boundary applies scheduling.

All live mutex owners remain explicitly unsupported, including uncontended
owners. A small per-thread count tracks distinct owned mutex objects, rather
than recursive lock depth. First acquisition, final release and object
destruction maintain it; thread and mutex objects cannot be copied or moved.
Mutex wait dependencies are found through retained wait-object references,
so closing a handle cannot hide one. WaitAll registrations count even when a
mutex is currently available.

This is a bounded slice. There is no nominal/effective priority split or
priority inheritance implementation. Dormant and Dead targets remain
unsupported. Existing diagnostic scheduler tie rules, strict/waiter thread-ID
ordering and weak mutex lifetime behavior are unchanged; full reference FIFO
or hardware-equivalent scheduling is not claimed.

Primary references: pinned Azahar [SVC validation](https://github.com/azahar-emu/azahar/blob/86a9f9236ae42bb5a2b995dbc933d599d8ea07ac/src/core/hle/kernel/svc.cpp),
[thread priority handling](https://github.com/azahar-emu/azahar/blob/86a9f9236ae42bb5a2b995dbc933d599d8ea07ac/src/core/hle/kernel/thread.cpp),
[mutex priority behavior](https://github.com/azahar-emu/azahar/blob/86a9f9236ae42bb5a2b995dbc933d599d8ea07ac/src/core/hle/kernel/mutex.cpp),
and libctru's [SVC declaration](https://github.com/devkitPro/libctru/blob/1f8d386e375ce734e11a3199eb51b8c98cc7e93a/libctru/include/3ds/svc.h)
and [ABI wrapper](https://github.com/devkitPro/libctru/blob/1f8d386e375ce734e11a3199eb51b8c98cc7e93a/libctru/source/svc.s).

## Verification and actual execution

One complete Release build and all **79 local suites pass**. The new authored
fixtures cover running-thread demotion, Ready-thread promotion, cross-core
context/TLS/reservations, validation precedence and handle lifetime, unchanged
sleep deadlines, event/semaphore/WaitAll behavior, arbiter wake order and
once-only pending-wake delivery. Mutex ownership, recursion, multiple objects,
final release, destruction, expired owners, failed handle allocation and
retained WaitAll registrations are covered too. Guest-written memory verifies
actual preemption and atomic unsupported stops in both runner modes.

One ordinary run and one private observational capture use the same larger
**131072-host-event / 600-second** profile and unchanged
**`--block-limit 100000000`** setting, with fresh SaveData/SharedExtData.
The unchanged strict comparator passes **21 checks**, including normalized
complete launch arguments/stdout, CPU/IPC state, filesystem inventories and
all **4571648 audio bytes / 285726 records**, with complete streams and
successful close status. Every audio byte from the pre-fix stop is an exact
prefix of the new stream.

The capture records **all five actual priority calls**, with no truncation;
all return success. Call **5** uniquely matches the earlier blocked request
by PC, caller, handle and requested priority. Its before/after proof verifies
the successful change on already-executed thread **12** and unchanged
serialized caller/target metadata and live/stored CPU contexts, except for
the permitted priority and result/PC updates. The observation runs inside the unchanged common
SVC boundary, before the runner publishes context or invalidates reservations.
The original setter executes exactly once. No GPU replay, artificial wake,
guest-memory repair, game-specific handler or automatic run retry is used.

All **14 capture payloads** are retained and hashed. Source, four production
libraries, native/capture executables, private supplements, original inputs
and reviewed helpers remain unchanged through the completed runs. The
original **599 AOT pages** and **four supplements** are unchanged. Hosted CI
for the exact published revision is recorded separately.

The initial AOT restoration preparation rejected the archive's SGID directory
mode before extracting any files. A dependent configure then correctly failed
because no pages existed. The corrected restore accepts the authenticated
archive, strips that directory's SGID bit, and preserves every regular-file
byte and mode. Both failed preparation records remain retained; they were
not failed tests or game runs. No project build or test failure occurred in
this priority milestone.

## Private evidence and source identity

- `extended-startup/` and `extended-startup-native-131072-2xi1ppau/`:
  exact published executable's larger startup window and restoration records.
- `extended-startup-capture-131072-oojf7a0v/`: matching pre-fix capture.
- `extended-startup/thread-priority-design/`: primary reference identities,
  bounded design decisions and independent production/capture reviews.
- `thread-priority-build.json`, `thread-priority-tests.{json,log,xml}` and
  `thread-priority-LastTest.log`: complete build and test gate.
- `thread-priority-native-131072-ntjiagtu/`: actual post-fix ordinary run.
- `thread-priority-capture-131072-t6yzkqg1/`: complete capture, strict
  comparison, five-call proof and raw state.
- `thread_priority_capture/` and `thread-priority-capture-run/`: private
  observational builder, exact approvals, execution and closure records.
- `thread-priority-screens/`: actual later framebuffer export and provenance.
- `thread-priority-next-blocker/`: bounded private static AOT-gap diagnosis.

Native SHA-256:
`6212ad2295258bd7a3f703ccf3f0449d3b05795b79c532a466b271202a04e041`
(**11726648 bytes**).
CTR library SHA-256:
`f152e45ddd542dd3987fbd9aa5a813ae50960c4e635ccdce824ddfc24a214d98`
(**1398410 bytes**).

The prior saved `284e3d1` metadata and private evidence are preserved, including
the earlier workspace-loss boundary and explicitly labeled recovery runs.
This milestone's extended and post-fix runs are new actual executions.
Public source remains in Git; game-derived inputs, executable bodies,
captures, audio and screenshots remain private. The complete prior handoff
remains byte-identical below.

---

# October 28, 2026 — RGB8 repeat textures and visible startup branding

## Current result and next task

The unchanged original game now renders **actual startup logos and copyright
text into its headless framebuffers**. The private lossless export shows
WB Games, TT Games, LEGO and Nintendo branding on the top screen and copyright
text on the bottom. The previously rejected texture queue at dispatch round
**10271** now completes successfully.

The current ordinary run reaches **round 18082**, guest time **7100531896 ns**,
thread **5**, PC `0x002594C4`, before **HostEventLimit**. This is the unchanged
**65536 host-event budget**, with no current unsupported IPC. The logged last
`gsp::Gpu` request, header `0x00080082`, is historical; current TLS command-buffer
bytes are retained separately.

**Next: extend bounded validation beyond the startup logos to identify the
next real blocker.** Retain explicit event/block/wall-time limits and account
for slow host execution: this ordinary window took approximately 199 seconds.
The budget-limited snapshot does not establish a hang at the logos or identify
another missing graphics feature.

## Observed state and implementation

The initial diagnostic retained the complete **2160-byte** pending command
list and its pre-batch GPU state. Read-only masked scalar reconstruction
through the rejected draw established ordinary 2D texture0, **RGB8**, repeat
on both axes, equal linear min/mag filters, zero LOD bias and min/max levels,
and the linear mip-filter selector. The rejected transaction preserved the
GPU/upload state, GSP page, VRAM, LCD controls, event state, time and list bytes.
No GPU queue, shader or raster replay was used to derive that configuration.

The implementation adds tiled **three-byte B,G,R storage with opaque alpha**
and independent clamp-to-edge/repeat axes. A signed unit remainder precedes
power-of-two scaling; each integer tap then wraps before storage-Y inversion.
This preserves negative nearest-sample floor semantics, handles large finite
coordinates without integer overflow, and blends across repeat seams.
Encoded RGB8 extents govern source snapshots, dependency and feedback checks.

Either mip-filter selector is accepted only with the existing **zero bias,
min=max=0 and equal min/mag filters**. Under the pinned reference sampler
contract, both choices sample the same base level. Other LOD levels, bias,
unequal filters, texture types and border/mirror modes remain unsupported.
Known-byte provenance, private reads and atomic output publication remain
required. Filtering retains the explicit deterministic binary32 model;
exact PICA hardware filter precision is not claimed.

Primary references: the pinned [register definitions](https://github.com/azahar-emu/azahar/blob/86a9f9236ae42bb5a2b995dbc933d599d8ea07ac/src/video_core/pica/regs_texturing.h), [texture layout decoder](https://github.com/azahar-emu/azahar/blob/86a9f9236ae42bb5a2b995dbc933d599d8ea07ac/src/video_core/texture/texture_decode.cpp), [BGR/alpha conversion](https://github.com/azahar-emu/azahar/blob/86a9f9236ae42bb5a2b995dbc933d599d8ea07ac/src/common/color.h), and [host sampler setup](https://github.com/azahar-emu/azahar/blob/86a9f9236ae42bb5a2b995dbc933d599d8ea07ac/src/video_core/renderer_opengl/gl_texture_runtime.cpp), with the [OpenGL 3.3 sampling rules, sections 3.8.11–3.8.12](https://registry.khronos.org/OpenGL/specs/gl/glspec33.core.pdf).

## Verification and actual rendering

All **79 local suites pass**. New authored fixtures cover RGB8 layout/alpha,
exact encoded extents, independent repeat axes, nearest/linear seams, negative
and very large coordinates, invalid inputs and base-only mip-selector
semantics. Existing IA8 and ETC formats exercise periodic coordinates too.
An independently authored queue verifies **64 actual pixels** with expected
RGB(104,132,100), depth writes and IRQ delivery; a later invalid packet rolls
the entire transaction back. Transfers at offsets 640 versus 768 verify the
exact 768-byte source boundary of a 16x16 RGB8 texture.

The baseline diagnostic `texture-state-capture-uu0k2f__` passes **21 strict
comparison checks** and **21 texture-state checks**. The final capture,
`texture-sampler-capture-m390xz4a`, starts the selected queue with the same
list bytes and identical pre-queue GPU, GSP, VRAM and register-knownness bytes.
It commits the actual success reply **0x000C0040**, advances the ring from
index **8**, count **1**, to index **9**, count **0**, and leaves the list
unchanged. Its result records **2 draws**, **10 vertices**, **4 triangles**,
**384000 raster pixel writes** and **103345 changed VRAM byte positions**.
Raster pixel-write counts can include repeated writes to the same pixel.

One ordinary run and one final capture use the existing **100 million block
limit**, **65536 host-event limit** and **240-second ordinary timeout**.
Original inputs and four authenticated AOT supplements are unchanged.
The final comparison passes **21 checks** across normalized launch arguments,
stdout, CPU/IPC, complete filesystem inventories and all **3708736 audio
bytes / 231794 records**, with complete streams and successful close status.
The queue proof passes **13 checks**, and complete integrity retains all
**231 capture members**. Neither selected capture was retried; no GPU replay
or captured-byte repair was used. Exact-revision hosted CI is recorded separately.

The final capture reaches the same HostEventLimit and explicitly marks its
last router request as historical. It records no terminal unsupported GPU
queue. Prior SaveData semantic proofs remain historical evidence and are not
counted as newly repeated semantic tests in this graphics milestone.

## Image and practical limits

The native-size export contains a **400x240** top screen and **320x240** bottom
screen, decoded losslessly from captured BGR display buffers with integer
rotation. The top contains **26722 nonblack pixels** and the bottom **9287**;
both LCD force-black controls are disabled. Only neutral spacing is added
around the original pixels. Earlier black-screen and save-preparation images
remain historical evidence.

This establishes startup-screen rendering in the headless runtime. Host-window
presentation, usable menus, audible playback and playable gameplay remain
unproven. Game-derived images, command lists and assets stay private.

## Private evidence and source identity

- `texture_state_capture/closed-verification.json` and
  `texture-state-capture-uu0k2f__/texture-state-proof.json`: initial diagnosis.
- `texture-sampler-build.json`, `texture-sampler-tests.{json,log,xml}` and
  `texture-sampler-independent-gate-review.json`: build, complete gate and
  independent source/artifact/ordinary-run verification.
- `texture-sampler-native-21ray1rl/`: selected ordinary run.
- `texture-sampler-capture-m390xz4a/`: actual queue proof, strict comparison,
  complete payload integrity and final CPU/GPU/GSP/VRAM/LCD state.
- `texture_sampler_capture/closed-verification.json`: final capture closure
  and release of the source/build freeze.
- `screenshot-texture-sampler/`: actual startup-screen export and provenance.
- `texture-state-references/`: pinned primary register/layout/sampler references.

Native SHA-256:
`04b88cd0aa777c8d78c684de3616e34661c723a9344db222cd79c3e8d3fc418e`
(**11726648 bytes**).
CTR library SHA-256:
`6d5aec57fbbebebfc598bd33e7909e8ffb337f872e7eee6a082a3d4d99639182`
(**1397640 bytes**).
This follows published SaveData Read milestone
`bb9aa0521009acc8d18c90a75c3cbe5f72d9ddb1`. The complete prior handoff,
including its failures, corrections and evidence limitations, remains
byte-identical below.

---

# October 28, 2026 — SaveData reads and the next graphics stop

## Current result and next task

The unchanged original game now passes the **fs:File Read** command that
previously stopped at dispatch round **8275**. The current ordinary run reaches
round **10271**, guest time **3593445876 ns**, thread **1**, PC `0x0025947C`,
with an explicit `UnsupportedIpc` stop and detail `0x32`.

The new request is **gsp::Gpu command-queue processing**, header
**0x000C0000**. The current PICA guard requires texture0 to use ordinary 2D,
base level zero and clamp-to-edge. It rejects the draw reached at command-list
byte **0x3F0**, command register **0x22E**. That location identifies where
processing stopped; it does not by itself identify the texture-register field
that needs additional support.

**Next task: retain the actual pending PICA command-list bytes and reconstruct
masked register writes up to the rejected draw.** The current capture retains
the last committed GPU state. Its texture0 configuration passes the existing
guard, while the failed queue's effective state has not been established.
The pending **2160-byte** command-list payload is absent from this capture.
The committed snapshot therefore cannot select a specific new texture feature;
that diagnosis remains the next bounded task.

The actual Read now returns success with reply header **0x08020082**, a count
of **42644**, and the original mapped descriptor/address. Its destination
already matched the file before the call, so **zero destination bytes change**.
The execution proof binds the successful reply/count, complete file/output
bytes and frozen real-`pread` implementation; the authored tests below use
distinct sentinels to establish changed-output behavior. No copied-output
claim is inferred solely from preexisting equal bytes.

The captured sequence then successfully closes the file and its second archive
mount. It does not issue an additional ControlArchive operation for that
read-only use. The last selected FS call is historical relative to the later
GPU stop; its timestamp is not substituted for the current final CPU time.

## Supported Read behavior

The new slice accepts the observed SaveData file Read command and correctly
shaped mapped output descriptor within the existing transfer bound. The file
endpoint must remain open. Reads use the actual pinned descriptor with
`pread`, consult the live extent and preserve the existing regular-file,
link-count and **64 MiB host-policy** checks. They do not reopen a diagnostic
pathname, mutate file contents or change a shared seek position.

The handler checks the entire declared destination even when EOF will return
fewer bytes. Destination and reply require private backing without physical
aliasing between them. Reply and destination preparation finish before any
guest-visible data is published; host data is read into temporary staging.
Successful replies report the actual byte count and retain the original mapped
buffer descriptor/address. Only that actual prefix is copied, preserving the
rest of the destination and its neighbors. EOF returns a successful short or
zero-byte read. Unsupported host conditions stop without publishing staged
bytes or a success reply.

An already-open SaveData file can still be read after its archive mount is
closed, and it sees growth performed through another endpoint. The new handler
is SaveData-only: it does not add Read support to SharedExtData or change that
backend's fixed-extent write behavior. These bounded host-file semantics add
no hardware journal, flush, quota or durability claim.

## Verification and retained evidence

The complete **79-suite local gate passes**. Authored read cases start with
output sentinels that differ from the file, proving actual byte transfer.
They cover full and offset reads, partial/zero EOF results, zero-length reads,
unchanged tails and neighbors, growth through another endpoint, archive/file
lifetime, pinned objects after pathname replacement, current extent after host
truncation, invalid transport, address overflow, private reply/destination
preparation, physical alias rejection, and explicit host-policy stops.
Existing SharedExtData behavior remains covered. Byte-identical executable
copies retain original test arguments, properties and working directories;
source and binary identities remain frozen through the selected gate/run.

One ordinary run uses the existing **65536 host-event budget**, **100 million
block limit** and **240-second ordinary timeout**. This milestone does not
raise those limits. Original code/RomFS/ExHeader inputs and four authenticated
build-time AOT supplements are unchanged. Its audio stream has **1872192 bytes
/ 117010 records**, with successful close status.

One selective capture, `save-read-capture-0da68ru1`, uses the same original
inputs and finite budgets as the ordinary run. The strict comparison passes
**21 checks** covering complete normalized launch arguments/stdout, final
CPU/IPC state, full recursive filesystem inventories, and all **1872192 audio
bytes / 117010 records**. Both complete audio streams close successfully.

The actual filesystem sequence passes **693 semantic checks**, including the
successful Read, following File Close and Archive Close, retained file bytes
and exact guest replies. Portable integrity and its readback check cover
**264 retained files and all 216 capture members**. No guest rerun, captured-byte
repair or GPU queue replay is used to produce this final comparison.
The capture retains full final CPU/GPU/GSP state, all **6 MiB of VRAM**,
LCD controls, the final queue request and bounded FS transport. It does not
retain the pending graphics command-list payload described above.

The latest private lossless framebuffer export shows **both actual screen
buffers black**, with zero nonblack pixels on either screen. Both LCD
force-black controls are disabled. The saved pixels are therefore black
independently of those controls. The earlier save-preparation notice belongs
to a previous stop. Host-window presentation, a usable menu, audible playback
and playable gameplay remain unproven.

GPU source is unchanged and no graphics implementation is claimed by this
filesystem milestone. Only the retained graphics state and the explicit guard failure are diagnosed here; failed-list state is not reconstructed. Exact-revision hosted
CI results are recorded separately from local and original-game verification.

## Private evidence and source identity

- `save-read-build.json` and `save-read-tests.{json,log,xml}` retain the
  production build and complete local gate.
- `save-read-native-tlq6i7t7/` is the selected ordinary run.
- `save-read-capture-0da68ru1/` retains the strict comparison,
  `save-read-proof.json`, portable integrity and complete capture payloads.
- `save_read_capture/closed-verification.json` records capture closure and
  release of the source/build freeze.
- `screenshot-save-read/` retains the actual two-black-screen export and its
  provenance. The pending command list remains a diagnosed evidence gap.

Native SHA-256:
`15cb516f6340941d2ca3b11398be9b1f4081d16e08d53f485622370f6df56dfd`
(**11726496 bytes**).
CTR library SHA-256:
`0212d32429a139acb68a3109e5645dca7d9c0410e708d21f5ef384e7e4da2e57`
(**1396272 bytes**).
This follows the published commit-acknowledgement/archive-close milestone
`4daf301b67134310e6197eff702ac3ceab0121c8`. The complete earlier handoff and
its preserved failures, corrections and evidence limits remain unchanged below.

---

# October 28, 2026 — SaveData commit acknowledgement and archive closing

## Current result and next task

The unchanged original game now passes the observed **ControlArchive** commit
request and **CloseArchive**. It preserves the existing **42644-byte**
application file, retires the first title-save mount, opens a fresh mount and
reopens that same file. GetSize reports **42644**.

The current explicit stop is **fs:File Read**, request header **0x080200C2**,
requesting **42644 bytes at offset 0**. It occurs at dispatch round **8275**,
guest time **2724472618 ns**, thread **11**, PC `0x0025947C`, with
`UnsupportedIpc` detail `0x32`.

**Next task: implement the observed bounded file Read operation.** The captured
Read has not executed. Its destination already contained bytes equal to the
file before the unsupported call; that preexisting content is not evidence
of a successful read. The complete request, transport, filename and save bytes
remain private.

## Supported command behavior

ControlArchive accepts the observed SaveData commit-action shape with one-byte
input and output buffers. It validates the initialized title's archive mount,
bounded private transport and complete reply backing. Both output and reply
writes are prepared before either changes. The input remains opaque; successful
execution writes a zero output byte and returns the reference success result.

This follows the pinned local SaveData implementation's success-only control
operation. The acknowledgement adds no file modification, filesystem journal,
flush or power-loss durability guarantee. The preceding file writes retain
their actual host effects. Unsupported action/buffer shapes remain explicit.

CloseArchive validates ownership and prepares the private guest reply before
retiring that mount. The archive manager preserves its next-handle sequence,
so reopening produces a fresh mount identity. Files, metadata and kernel file
session handles are unchanged by archive closing. File sessions own separate
descriptors and remain independent of the mount's lifetime.

The observed capture confirms successful ControlArchive and CloseArchive
replies, unchanged input/file/metadata bytes, zeroed control output and the
mount count changing **1 to 0** at CloseArchive. A subsequent OpenArchive
creates a fresh mount; OpenFile preserves the existing file and GetSize returns
its real extent before the unsupported Read request.

## Verification and retained history

The final combined implementation passes the complete **79-suite local gate**.
Authored cases cover exact commit/close replies, initialized title ownership,
stale handles, mount retirement and fresh reopening, surviving file endpoints,
invalid transport, aliasing and private reply preparation. Byte-identical
executable copies preserve the original test arguments, properties and
working directories. Source, native, CTR and AOT identities remain frozen
through the selected gate and ordinary run.

An earlier Control-only build attempt was interrupted. A subsequent prelaunch
artifact guard found an empty unrelated generated test executable before any
tests or guest run could start. The failure and original artifact were retained;
that target was relinked from unchanged source. The intermediate Control-only
79-suite pass and its ordinary stop at CloseArchive, round **5350**, remain
separate historical evidence. The final ControlArchive-plus-CloseArchive build
and all 79 suites pass without a current gate failure.

For that final combined implementation, **one ordinary run and one selective
capture** use the established **65536 host-event budget**, **100 million block
limit** and **240-second ordinary timeout**. This milestone does not raise the
limits further. Original code/RomFS/ExHeader inputs and four authenticated
build-time AOT supplements are unchanged.

The strict comparison passes **21 checks**. Complete normalized launch
arguments/stdout, final CPU/IPC state, recursive SaveData/SharedExtData
inventories and all **1417152 audio bytes / 88570 records** match. Both audio
streams retain complete headers and reported tails with successful close
status. The actual command sequence passes **596 semantic proof checks**.
Portable integrity and its readback check cover **230 retained files and all
188 capture members**. No guest rerun or captured-byte repair was used to
produce this final comparison.

The capture retains the full next Read request and bounded transport,
GPU/GSP state, all **6 MiB of VRAM** and both LCD control words. A private
lossless export shows **both actual screen buffers black**. Both LCD
force-black controls are disabled; the black pixels are present in the
retained buffers. The earlier save-preparation notice is historical evidence
from the preceding stop, not the current frame. Host-window presentation,
a usable menu, audible playback and playable gameplay remain unproven.

GPU code is unchanged and no GPU queue is replayed for this filesystem
milestone. Exact-revision hosted CI results are recorded separately from
local and original-game verification.

## Private evidence and source identity

- `save-complete-build.json` and `save-complete-tests.{json,log,xml}` retain
  the final combined production build and complete local gate.
- `save-complete-native-btkpe5se/` is the selected ordinary run.
- `save-commit-capture-wnrzzlh4/` retains the strict comparison, successful
  `save-commit-proof.json`, portable integrity and complete capture payloads.
- `save_commit_capture/closed-verification.json` records the closed capture
  verification and release of the source/build freeze.
- `screenshot-save-complete/` retains the actual framebuffer export and
  its provenance; both screen buffers are black at the current stop.
- `save-commit-execution-interruption.json`, `save-commit-pass-failure.json`,
  `save-commit-relink.json`, `save-commit-tests.{json,log,xml}` and
  `save-commit-native-a00cvy6q/` preserve the earlier Control-only sequence.

Native SHA-256:
`93e79869cdc41b07973fb6d861702c34d5854c52a6431487aba34eb3f0e0875b`.
CTR library SHA-256:
`c1675dd0ebcd0f7b62d571a6a5a23346ae8cfa4e4f0522debe6b166432591b6e`.
This follows the published file-opening/growth milestone
`110d7e5330ba9a1b23a70b2932c9dfc1aa776604`. The complete earlier handoff,
including the OpenFile ordering correction, Format failures and other
historical evidence limits, is preserved unchanged below.

---

# October 28, 2026 — SaveData files and growing writes

## Current result and next task

The unchanged original game now completes the previously unsupported SaveData
**OpenFile** request. It creates a real zero-length application file, reads its
size, writes **42644 bytes**, and closes the file. The file contents come from
the running guest and remain private.

The selected ordinary run reaches **fs:USER ControlArchive**, request header
**0x080D0144**, at dispatch round **5349**, guest time **1872359327 ns**. The
explicit stop is `UnsupportedIpc`, PC `0x0025947C`, detail `0x32`, thread 10.

**Next task: implement the observed ControlArchive commit acknowledgement
against the verified reference behavior.** The complete command and transport
remain private. This milestone establishes file creation and writes; it does
not claim that the subsequent archive commit or save initialization completed.

## Supported file behavior

The new SaveData slice accepts the observed UTF-16 OpenFile request with
**read/write/create mode 7 and attributes 0**. The mount must belong to the
initialized client program. Descriptor-relative traversal stays beneath the
pinned title directory, checks the bounded path and rejects unsupported link
or file types. Replacing a diagnostic host pathname cannot redirect the mount.

An existing regular file opens without truncation. A missing leaf is staged
under an owned temporary name and published with Linux
`renameat2(RENAME_NOREPLACE)`. The file endpoint, client session, kernel handle
capacity and private guest reply are prepared before publication. Failed
preparation or publication cleans up the owned temporary leaf without removing
an existing file; rollback failures remain explicit host stops.

SaveData file size is read from the actual descriptor, so separate opens see
one another's growth. Writes may extend the file, including an intervening
zero-filled gap, up to an explicit **64 MiB host policy bound**. This bound is
not a discovered console capacity or a claim that format-metadata quotas are
enforced. SharedExtData retains its prior fixed-size clipping semantics.

The file service handles GetSize, Write and Close for this observed slice.
The Read command remains unsupported. SaveData reply backing is prepared
before operations that change the file or release its descriptor. Write input
must have private backing separate from the reply. A host write failure stops
explicitly and retains its real partial byte count; it does not pretend that
a full write succeeded. There is no new hardware journal or power-loss
durability claim.

## Verification and finite-run limits

The complete **79-suite local gate passes**. Authored SaveData cases cover
creation, growth, gaps, preserved existing bytes, size visibility across
separate opens, title ownership, invalid paths and links, publication conflict
and rollback, exhausted handle capacity, private reply preparation, closed
endpoints and an actual partial host write. The gate uses byte-identical
executable copies with the original test arguments, properties and effective
working directories. Source and binary bytes remain frozen through the gate.

Two ordinary original-game runs are retained. The first uses the existing
**16384 host-event budget**, creates and writes the 42644-byte file, then reaches
`HostEventLimit` at dispatch round **5162**. That limit is not an unsupported
game operation. A separately recorded second run raises only the finite
host-event budget to **65536** and reaches ControlArchive at round **5349**.
The native binary, original inputs, **100 million block limit** and **240-second
ordinary timeout** are unchanged. No test rerun accompanies this budget change.

One selective capture uses the second run's **65536-event** setting and the
same other limits. Complete normalized launch arguments/stdout, final CPU/IPC
state, recursive SaveData/SharedExtData inventories and all **970944 audio
bytes / 60682 records** match that ordinary run. Both audio streams include
complete headers and reported tails with successful close status.

The actual file sequence is **OpenFile → GetSize(0) → Write(42644) → Close**.
OpenFile adds one kernel handle, from 38 to 39. File Close releases its file
descriptor without removing that kernel session handle. The final capture
retains the full subsequent ControlArchive request, bounded input transport,
GPU/GSP state, all **6 MiB of VRAM** and both LCD force-black words.

The first semantic proof compared identical filesystem entries in different
orders: C++ path ordering and Python string ordering. It retained one failed
check. A separate, reviewed read-only verifier rejects duplicate paths and
compares all complete entries after sorting by the same key. The corrected
`save-file-proof-checked.json` passes **276 checks**. The original failed proof
and original helper remain unchanged; no guest rerun or captured-byte repair
was performed. Portable integrity and its readback check cover **145 retained
files and all 102 capture members**.

The latest private lossless framebuffer export still shows a save-preparation
notice on the lower screen and a black top screen. Both captured LCD
force-black controls are disabled. This is evidence of the actual headless
framebuffer at the stop, without demonstrating a host window, usable menu,
audible playback or playable gameplay.

GPU code is unchanged and no GPU queue is replayed for this filesystem
milestone. The original code/RomFS/ExHeader hashes and four authenticated
build-time AOT supplements are unchanged. Exact-revision hosted CI results
are recorded separately from the local and original-game evidence.

## Private evidence and source identity

- `save-file-build.json` identifies the frozen production build.
- `save-file-tests.{json,log,xml}` retains the complete 79-suite local gate.
- `save-file-native-_wwyms8d/` retains the first run at the existing event limit.
- `save-file-extended-preparation.json` and `save-file-extended-run.json`
  record the explicit budget change and unchanged inputs/source/binary.
- `save-file-extended-native-e2rk_ue6/` is the selected ordinary comparator.
- `save-file-capture-46pp9n5n/` and `save_file_capture/` retain the selective
  capture, strict comparison, original failed proof, corrected proof and helpers.
- `save_file_capture/closed-verification.json` records the complete capture
  verification and release of the source/build freeze.
- `screenshot-save-file/` retains the actual framebuffer export and provenance.

Native SHA-256:
`00a6bf7547b7884a8711ca88c09365770fbf24a92c9fd417af5d3ab9a442f98e`.
CTR library SHA-256:
`075e92f96b64d1b794085387ec9851783b348eacd0d249fecfb60c1404a5d6e3`.
This follows the published first-time format milestone
`bbb453626d266e75782d168cc0ddbe3a1123dd48`. All earlier chronological sections,
including their historical failed gates and evidence limits, remain unchanged
below.

---

# October 28, 2026 — first-time SaveData formatting

## Current result and next task

The unchanged original game now completes its explicit **FormatSaveData**
request against a fresh configured title container. The operation creates a
real per-title data directory and deterministic **16-byte format metadata**.
The game then reopens that archive successfully and advances from dispatch
round **1626 to 5118**, reaching guest time **1772111212 ns**.

The next stop is **fs:USER OpenFile**, request header **0x080201C2**, against the
opened SaveData archive with **read/write/create** flags. The stop remains
`UnsupportedIpc`, PC `0x0025947C`, detail `0x32`, now on thread 10.

**Next task: implement the observed SaveData OpenFile operation with its
requested read/write/create behavior.** The complete request, bounded UTF-16
path transport and current archive binding are retained privately. Keep the
captured filename, payload bytes and game content out of the public repository.
File opening and application save contents are not yet implemented for this
archive; the real format metadata is not a fabricated application save.

A lossless private export of the latest captured framebuffer shows a
save-preparation notice on the lower screen, while the top screen remains
black. Both captured LCD force-black controls are disabled. This establishes
the actual headless framebuffer contents at the stop. It does not establish
host-window presentation, a game scene, a usable menu, audible playback or
playable gameplay.

The finite launch settings, original code/RomFS/ExHeader inputs and four
authenticated build-time AOT supplements are unchanged. GPU code is unchanged
and no GPU replay was performed for this filesystem milestone. Hosted CI
results are recorded separately for the exact published revision; the local
results below do not claim a new hosted result.

## Supported format behavior

The bounded implementation accepts the observed SaveData/Empty request shape
only after validating the initialized client program binding, configured root,
transport backing, format size/count bounds and complete private reply backing.
Empty-path transport is opaque; its incidental contents do not select a title
or become a host filesystem path.

The backend uses descriptor-relative traversal beneath the pinned save root.
It prepares the complete first missing title subtree, writes the deterministic
metadata, and publishes the subtree with Linux `renameat2(RENAME_NOREPLACE)`.
All fallible preparation finishes before publication. Existing title data,
including an empty existing data directory, remains an explicit unsupported
replacement case. Hosts without the required atomic operation do not receive
a weaker fallback.

The metadata records requested capacity, directory/file counts and the
duplicate-data flag with explicitly zeroed padding. These fields describe the
bounded backend's format model; they do not claim hardware allocation,
hash-bucket behavior, duplicate save copies or power-loss durability. Formatting
creates no application file contents and allocates no archive mount. A later
successful OpenArchive creates one title-bound SaveData mount, isolated from
SharedExtData and without allocating a kernel handle.

## Verification and retained evidence

The complete **79-suite local gate passes**, including the expanded SaveData
suite. Authored cases cover exact replies and metadata, all three supported
missing-subtree publication boundaries, subsequent archive opening, title
isolation, existing-data preservation, symlink/root replacement guards, reply
preflight and a real partial metadata-write failure with staging cleanup.

An earlier full gate could not launch four unrelated executables after their
execute-mode bits changed. That failed gate is preserved. The final gate uses
fresh byte-identical executable copies with the original test arguments,
properties and working directories; all 79 names pass in both console and
JUnit records. The preparation working-directory correction is retained as a
separate artifact. Source and binary bytes remained frozen; no test was
removed or weakened.

Exactly **one ordinary game run and one selective FS capture** were performed.
The capture brackets the actual missing OpenArchive response, FormatSaveData
success and subsequent successful OpenArchive. It verifies real guest reply
writes, initialized title binding, mount/kernel-handle counts and complete
owned filesystem inventories before and after each selected call. The missing
archive initially has no storage; format publishes only the title directories
and metadata; reopening adds exactly one SaveData mount. All three calls leave
the kernel handle count unchanged. **All 82 FS proof checks pass.**

Complete normalized launch arguments/stdout, final CPU/IPC state, SaveData and
SharedExtData inventories, and all **918464 audio bytes / 57402 records** match
ordinary execution. Both audio streams have their complete headers and reported
tails, with successful close status. The final capture retains the complete
unknown IPC request and bounded transport, GPU registers, GSP state, all
**6 MiB of VRAM**, and both LCD force-black words. No graphics queue is replayed.

The continuous launcher's final summary print raised a Python `TypeError`
after the build, single capture and strict comparison had completed and been
saved successfully. The original helper and its top-level failed receipt remain
unchanged. A separate read-only verification passes **105 checks**, including
the completed stages, original failure, frozen sources and executable copies.
A separately named portable verifier retains that correction evidence and
checks **74 files and all 40 capture members**. Its subsequent read-only
verification also passes. No guest rerun, captured-byte repair or replacement
of the original failure record was used.

Private receipts currently available:

- `format-save-build.json` for the frozen production build.
- `format-save-tests.{json,log,xml}` for the preserved initial gate.
- `format-save-launch-preparation-failure.json` for the preparation correction.
- `format-save-final-tests.{json,log,xml}` for the complete passing gate.
- `format-save-native-g1npvcda/` for the one ordinary run.
- `format-save-capture-pgg23eef/` for the strict comparison, FS proof, separate
  post-launch verification and portable integrity record.
- `format_save_capture/` for the frozen private helpers, original failed
  launch receipt and separately authored read-only verification helpers.
- `screenshot-format-save/` for the final framebuffer export and its provenance.

Native SHA-256:
`e9cf04813867cf4ed9c9be6d93d2a68b016e38f991115fc4cabf51baa1705cd2`.
CTR library SHA-256:
`79350f1bad5bb56b8746d422e7a119d64066c3e4fb22eb871545f4e2071fad3b`.
This follows the published compressed-texture/clipping milestone
`31819b131a677b0407774770fdc040b7d59cb038`. All earlier chronological sections,
including incomplete historical Protect audio evidence, remain unchanged below.

---

# October 28, 2026 — compressed textures and homogeneous clipping

## Current result and next task

The unchanged original game now passes the GPU request that blocked dispatch
round **1529**. ETC1/ETC1A4 texture sampling and positive-W homogeneous clipping
allow the observed draw at **0x12C0** to execute. The ordinary native run reaches
dispatch round **1626**, guest time **652361036 ns**, and then stops on
**fs:USER FormatSaveData**, request header **0x084C0242**. The stop is
`UnsupportedIpc`, PC `0x0025947C`, detail `0x32`, thread 9.

**Next task: implement the observed FormatSaveData operation after verifying its
complete captured request and pinned filesystem semantics.** It requests the
per-title SaveData archive. The configured fresh save container remains empty
at this stop; no format operation or automatic save creation has been added.
Preserve the distinction between unconfigured storage, a configured missing
save, and an explicitly requested format. The final capture retains all twelve
request words and the readable path transport. Use that complete private
evidence and the next-blocker brief rather than the earlier partial trace.

This milestone advances beyond the previously rejected GPU queue. It does not
establish a visible scene, menu, audible playback or playable gameplay. The
finite launch settings, original code/RomFS/ExHeader inputs and four authenticated
build-time AOT supplements are unchanged. The final selective capture verifies
accepted queue completion and complete equivalence with ordinary execution.
Hosted CI results are recorded separately for the exact published revision.

## Compressed texture behavior

Texture0 now supports ETC1 (format 12) and ETC1A4 (format 13) alongside RGBA8
and IA8. The independently authored decoder follows the pinned PICA little-endian
4-by-4 block layout within 8-by-8 tiles, including individual and differential
color modes, both partition orientations, modifier tables and selector order.
ETC1 is opaque; ETC1A4 decodes independent four-bit alpha before filtering.
Undefined differential endpoints stop without replacing output colors or
wrapping into another compression mode.

The staged source retains its encoded byte extent: ETC1 uses one half byte per
texel and ETC1A4 one byte per texel. The complete encoded span drives source
preflight, queue read dependencies and framebuffer feedback checks. Existing
ordinary-2D, base-level-zero, power-of-two size, clamp-to-edge, matching filter,
known-state and numeric guards remain in force. Sampling reads only the staged
private-memory source or ordered disposable VRAM snapshot.

## Canonical clipping behavior

Position, preclamped primary color and enabled ordinary texture0 UV are
validated before assembly. A primitive entirely outside one canonical plane
still takes the arithmetic-free rejection path. Remaining positive-W crossings
are clipped against the six strict planes before perspective division. Each
generated intersection interpolates the consumed attributes and snaps exactly
to its active plane. Equality stays inside; no epsilon is added.

Each input primitive can produce at most nine polygon vertices and seven fan
triangles. Boundary endpoints are reused, duplicate adjacent/closing vertices
are removed, and transformed storage and generated triangles have explicit
growth limits. The reported input-triangle count retains its earlier meaning.
Fan triangles preserve culling and shared-edge behavior, and every bounding box
counts against the existing draw and queue pixel-work budgets before fragments
execute. All surviving screen coordinates must fit the configured framebuffer.
Nonpositive W, unknown components, unsupported intermediate arithmetic, user
clipping, scissor tests and framebuffer clipping remain explicit stops.

No input register, shader output or source buffer is modified. A failure leaves
the caller's previous output plan unchanged, and queue ownership still governs
the eventual color/depth commit. The strict clipping and binary32 interpolation
are declared emulator models, not measurements of PICA float24 or filter
precision.

## Verification and current evidence

The complete **79-suite local gate passes**, with all configured names present
in both the console log and JUnit output. Logs and XML were flushed and checked
after writing, and source/native/library identities remained unchanged during
the build and test gate. Authored cases cover compressed color/alpha decoding,
tile/subblock addressing, exact encoded read spans, source dependencies,
feedback rejection and queue rollback. Clipping cases check visible coverage,
shared-edge blending, strip winding, exact boundary handling, fan work budgets,
variable-W color/UV/depth goldens and late-failure output preservation.

An initial ETC1A4-only replay verifies the complete **4096-byte** encoded source,
all **4096 texel centers** and **409 fractional/clamp probes**: all **18020 RGBA
output bytes** match an independent oracle. Its six original vertices and all
**120 selected shader output components** are verified. That earlier replay
correctly stops at the then-existing clipping guard and preserves VRAM. It is
retained as an intermediate result with that exact scope.

The combined clipping replay then executes the same original draw successfully.
Its **two input triangles become three fan triangles**, with **8841 candidate
pixel tests** and **3852 pixel writes**. The independent scalar oracle matches
all **6291456 VRAM bytes** and the complete **409600-byte color patch**. Exactly
**2110 RGB bytes** change; depth, stencil, destination alpha and uncovered bytes
remain unchanged. All 120 shader components are again checked. The resulting
VRAM SHA-256 is
`75ad28f89a2ee1bad8ebb3d8638adc66c685407a055187ac6406066bac7f161c`.
This is an offline staged-draw proof. The final selective capture separately
ties that draw and the complete live queue to their committed results.

The selective capture observes **264 GPU requests** while logging only queue
**250** and final state. Queue 250 is accepted with **eight draws, 406 vertices,
7308 shader instructions and 136 input triangles**. Its **316041 candidate
pixel tests** produce **157452 covered/written pixels** across the complete
list. The disposable execution matches the actual committed GPU registers,
uploaded state, VRAM and queue result; the seventh draw matches the independent
clipped oracle above. Complete normalized launch arguments/stdout, final CPU/IPC
state and all **332096 audio bytes / 20754 records** match the ordinary run.
No diagnostic image or buffer change is presented as a displayed game frame.

Private receipts currently available:

- `etc1-clip-build.json` and `etc1-clip-tests.{json,log,xml}`.
- `etc1-replay-9k2158r8/{proof,receipt}.json` for the earlier sampler/guard proof.
- `clip-intersections-replay-za2edw5z/{proof,receipt}.json` for the complete-byte clipped draw proof.
- `etc1-clip-native-qm7bewl9/{receipt.json,stdout.txt}` for the ordinary run to FormatSaveData.
- `etc1-clip-capture-z_od9jgr/` for accepted-queue, complete-run comparison and portable integrity proofs.
- `next-format-save-data-blocker.json` for the observed next filesystem request and pinned semantics.

Native SHA-256:
`a1d7ad1f33b50eceba636dd3a97857be201e5df388e8df7a25041c6b140c0631`.
CTR library SHA-256:
`3ae63778db70cd4f76df932dabaa508d41a190a7445e14d1fe5cb5766568e187`.
This work follows the separately published IA8 milestone
`81b1414463b281dbbf9dcd633f95219b659d63a8`. Its captures, sampler proof and all
earlier evidence limitations remain preserved below. The earlier incomplete
Protect audio artifacts are unchanged.

---

# October 28, 2026 — IA8 texture sampling

## Current result and next task

Texture0 now accepts IA8 (format 5) alongside RGBA8. The original draw at
**0xB58** and the next two draws stage successfully. The same 5952-byte GPU
command list reaches its seventh draw, at **0x12C0**, which selects ETC1A4:

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=3 dispatch_rounds=1529
host_ipc_error=PICA texture0 format is outside the supported RGBA8/IA8 slice at list byte 0x12c0 register 0x22e
guest_now_ns=635131145
```

**Next task: implement the observed ETC1A4 compressed texture format.** The
captured texture is 64 by 64 with a complete 4096-byte encoded source. Preserve
the pinned PICA block layout and explicit alpha; this requires an actual block
decoder. IA8 support alone does not complete this queue, present a frame, or
advance execution beyond round 1529. The later failure rolls back every staged
draw. No visible game scene, menu, audible playback or gameplay is demonstrated.

## IA8 behavior and verification

The staged texture plan now has an explicit format. IA8 stores two source bytes
per texel in alpha/intensity order and expands each lookup to `(I,I,I,A)` before
the existing nearest or linear filtering. Tile addressing, Y inversion, numeric
policy, provenance checks and sampler bounds are retained. The encoded two-byte
extent drives source reads, FCRAM dependencies and color/depth feedback checks;
it is never replaced by an expanded four-byte extent. Unknown formats and
malformed plan sizes still fail without changing caller output.

The sampler, rasterizer and queue suites pass. Added authored cases exercise
independent intensity/alpha, transparent nonzero intensity, tile boundaries,
nearest/linear goldens, exact source ends, malformed plans, transfer adjacency
versus overlap, framebuffer feedback and whole-queue rollback. No new suite was
added; the existing full configuration still contains 79 suites. Hosted checks
are attached to the exact published revision separately from original-game work.

One offline native replay verifies all **65536** original texel centers and
**2876** meaningful noncenter filter/clamp probes. All **273648** output color
bytes match an independent address/decode/binary32 oracle. The exact **131072**
encoded source bytes are retained. All **2640** selected shader components
match the independent arithmetic oracle; all **44** triangles have zero area.
The draw produces no patches or pixel work and preserves the complete 6 MiB
VRAM snapshot. This degenerate geometry is not a visible texture-rendering
claim; authored nondegenerate draws test the integration separately.

The selective original capture records only queue 250 and final state. Its
complete normalized arguments/stdout, CPU/IPC state and all **323072** audio
bytes match the ordinary run. Six draws succeed only in disposable storage;
the seventh stops on ETC1A4. The live queue retains its rollback behavior.
Filtering and depth remain declared emulator approximations, and earlier
historical evidence limitations are unchanged.

Private receipts: `ia8-build.json`, `ia8-focused-tests.{json,log,xml}`,
`ia8-replay-s_0e7j5q/{proof,receipt}.json`, `ia8-native-hqs7m6jo/`, and
`ia8-capture-nychrpno/`. Original inputs and the four authenticated AOT
supplements are unchanged. Native SHA-256:
`863baf3b47b3059d950391133cf0ee823208fd0ecb77aef7d93c3b4946af7030`.
CTR library SHA-256:
`2b30e9d4d674fb2ee8d04d04b607c5264975817a8951be284ff36d5d12895245`.

The following sections preserve previous milestones and their original scope.

---

# October 28, 2026 — proven whole-triangle clip rejection

## Current result and next task

The later draw at list byte **0x668** contains two triangles entirely left of
the view: all three original shader positions in each triangle satisfy
`x < -w`. The rasterizer now recognizes this case as a successful draw with
zero pixel work and no color/depth patches. The first textured/depth draw at
**0x3F0** remains byte-for-byte identical to its independent reference result.

The unchanged original game now proceeds to a later texture configuration in
the same command list:

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=3 dispatch_rounds=1529
host_ipc_error=PICA texture0 requires ordinary RGBA8, base level0 and clamp-to-edge at list byte 0xb58 register 0x22e
guest_now_ns=635131145
```

**Next task: support the actual texture configuration captured at 0xB58.**
Start with the private `evidence/next-texture-blocker.json` and its exact source
spans, then extend the bounded sampler only for behavior supported by that
evidence and pinned references. The whole pending queue still rolls back at
this later guard, including all earlier staged draws. This is progress within
one GPU request, not an increase beyond round 1529 or a presented game frame.
No visible scene, menu, audible playback or gameplay is demonstrated.

## Geometry behavior and verification

The rasterizer classifies known finite position components against the six
canonical homogeneous clip planes, with positive W required. It rejects a
triangle only when all three endpoints are strictly outside the same plane.
Plane distance is affine across the triangle, so that condition proves there
is no intersection to render. Equality stays inside. Outside endpoints skip
perspective division and screen conversion, avoiding unsafe conversion of
large finite values. Position, color and enabled UV provenance are still
validated before this decision.

Triangles crossing a plane, endpoints outside different planes, nonpositive W,
unknown components and unsupported numeric inputs remain explicit stops. A
later failure leaves the caller's output plan unchanged. These are strict
canonical comparisons; they do not reproduce the pinned software clipper's
small approximate epsilon or claim measured hardware clipping precision.

All **79 local suites pass**. Authored cases cover all six planes, boundary
equality, large finite outside positions, unknown enabled UV, nonpositive W,
crossing triangles and late failure. Two private replays use the frozen final
CTR library and exact original captured sources. For both draws, all 120
selected shader output components match the independent arithmetic oracle.
The first draw's complete 6 MiB output and patch bytes remain identical to the
previous depth proof. The second draw reports two outside triangles, zero
tested/covered/written pixels, zero patches and all 6 MiB unchanged.

The initial test receipt and JUnit recorded all 79 passing cases, but a later
read found that its console log was truncated and no longer matched the saved
hash. That artifact is preserved. A separately named complete gate checks the
same frozen source and binaries, all configured names, and durable log/JUnit
readback; no test is removed or weakened. Earlier executable-mode repairs and
the historical incomplete Protect audio captures remain recorded below.

Current private receipts:

- `clip-reject-build.json` and `clip-reject-final-tests.{json,log,xml}`.
- `clip-reject-replay-enzranjj/{proof,receipt}.json`.
- `clip-reject-native-ed0d4w_y/{receipt.json,stdout.txt}`.
- `next-texture-blocker.json`.

Native SHA-256: `adcd70223add98be5e46c225156796fd1a838486d914f43731734437ba61bba2`.
CTR library SHA-256: `cdf0b1abf36c08257a9cce3545e58fd2c19561a1c827a8af71cf870e7734da63`.

Use the unchanged finite original launch and private-input recipe below.
Exact-commit hosted results and the final targeted capture are recorded in the
private continuation receipt. The following sections preserve preceding
milestones and their original evidence scope.

---

# October 28, 2026 — authenticated AOT entry, texture0 and depth

## Current result and next task

The missing ARM entry at **0x00133788** is now a reviewed, six-instruction
build-time supplement. All six words are checked against the authenticated
private executable; the original 599 AOT pages remain unchanged. The merged
registry contains four supplements and verifies **545130** instruction words.
No original instruction arrays are committed and no runtime decoder is added.

The unchanged original game advances from dispatch round **1469 to 1529**.
Its next GPU request originally stopped at the first textured draw, list byte
**0x3F0**. Texture0 sampling and depth testing now stage that draw successfully.
The same command list proceeds to a later draw at **0x668**, where it needs
homogeneous triangle clipping:

```text
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=3 dispatch_rounds=1529
host_ipc_error=PICA raster clipping is required outside the supported homogeneous volume at list byte 0x668 register 0x22e
guest_now_ns=635131145
```

**Next task: implement the observed clipping behavior after reviewing the
later draw's actual shader outputs and the pinned clipper semantics.** Do not
clamp positions, discard geometry or acknowledge a failed draw to force progress.
The later failure discards the complete pending queue, including the earlier
staged color/depth writes. The new depth result is proven by private offline
replay; it is not a claim that this rejected queue committed or presented a frame.
No host event cap was hit or increased. There is no visible game scene, menu,
audible playback or gameplay demonstration.

## Implemented texture and depth slice

`pica_texture` stages an immutable, bounded texture0 source. It accepts ordinary
2D tiled RGBA8, power-of-two dimensions from 8 through 1024, base level zero,
zero LOD bias, clamp-to-edge in both axes, and matching nearest or linear min/mag
filters. Other units, procedural textures, fog, mipmaps, wrap modes, formats and
projective/cube/shadow modes remain guarded. Consumed fields must have known-byte
provenance; missing data never becomes a fallback color.

Source bytes come from the ordered disposable VRAM snapshot or a completely
readable cacheless-private FCRAM span under the existing physical-to-linear-memory
policy. Device callbacks and shared-memory sources are excluded. The complete
physical read span is retained so the queue rejects a texture dependent on an
earlier uncommitted FCRAM transfer. Source overlap with either active framebuffer
and color/depth framebuffer overlap are rejected pending feedback semantics.

Ordinary texture0 UV uses output semantics 12/13, preserving component provenance
and perspective interpolation. RGBA8 decoding uses the primary reference's tiled
layout and storage orientation. Bilinear filtering uses explicit binary32
rounding and a documented byte-color rounding rule. The pinned accelerated
renderer selects host linear filtering; its software sampler leaves filtering
unimplemented. This is a deterministic emulator approximation, not measured
PICA filtering precision.

The fragment stage now supports D24S8 Z buffering and the eight standard depth
comparisons. It interpolates Z/W with biased coverage weights, applies the known
viewport scale/offset, clamps to [0,1], and truncates the binary32-scaled 24-bit
value. Depth writes preserve each stencil byte. The observed depth-read mask 2
and depth-write masks 0/2 are supported; other active permission masks remain
explicit stops. W buffering, other depth formats, stencil and alpha tests remain
unsupported. Depth precision follows the declared software model rather than a
hardware-equivalence claim.

All color and depth patches remain private until the whole GSP queue, its reply
and interrupt effects pass existing preflight. Later failures preserve complete
live register/upload/VRAM/event/time state. A subsequent draw in a valid queue
sees earlier staged VRAM fills, texture data and depth/color writes.

## Verification

All **79 unique local CMake suites pass**, with exact configured names checked
against JUnit and no skips. The new sampler suite and extended raster/queue suites
use authored data to verify nonuniform Morton-addressed colors, bilinear goldens,
perspective UV, TEV/blending/alpha masks, all depth comparisons, independent Z/W
interpolation, stencil preservation, memory policy, feedback rejection, ordered
VRAM writes, FCRAM dependency rejection and whole-queue rollback.

Five unrelated generated test executables initially lacked execute bits and
could not start. Only their modes were restored from 0644 to 0755; all five byte
hashes were unchanged. The complete 79-suite rerun passed with unchanged source,
native executable and CTR library. No test was removed or disabled.

Private original evidence establishes:

- A complete ordinary/capture match at the pre-fix texture stop: normalized
  arguments/stdout, CPU state, IPC, fresh save state and all **323072** audio bytes
  for **20190** reported records. All 250 GSP request/response records are complete.
- The rejected queue is one command-list packet. Its exact first-draw prefix,
  shader state, 144 vertex-source bytes and all 1024 texture bytes are retained.
  GPU/GSP state, full VRAM, uploads, events and time are unchanged on rejection.
- Six original vertices execute 108 shader instructions. All 120 selected output
  components agree with an independent exact-rational shader oracle.
- First-draw replay evaluates **76800** covered pixels using an independent
  binary32 depth calculation. The full original 6 MiB VRAM output matches, with
  76800 passing depth writes and 230400 changed depth bytes. A separate seeded
  case has 67857 passes and 8943 failures; its complete 6 MiB output also matches.
  Color, stencil, masked components and every outside byte are preserved.

The captured texture is uniform and this draw's final alpha is zero. Consequently,
its color output cannot establish nontrivial filtering or visible rendering.
The authored nonuniform sampler and colored-alpha integration cases provide
those separate checks. An initial private oracle had a stale disabled-depth
precondition; the failed check and receipt were retained, the condition corrected
to the captured enabled mask, and the same native outputs were verified without
rerunning either case.

The previous milestone's two truncated Protect diagnostic-audio artifacts remain
incomplete historical evidence. A successful new capture does not repair them.
Hosted GCC, Clang and ASan/UBSan results are collected for the exact published
revision separately from private original-game execution.

Key private receipts beneath `evidence/`:

- `aot-00133788-review-private.json`, `aot-00133788-build.json`.
- `texture-depth-build0.json`, `texture-depth-tests1.{log,xml,json}`.
- `texture-capture-v_mntn5l/ordinary-capture-comparison.json` and `rejected-queue-proof.json`.
- `texture-shader-replay-k7t1kw7c/shader-proof.json`.
- `texture-draw-replay-hjuoftux/draw-proof.json`.
- `texture-depth-native-wrfwvroq/receipt.json` and `stdout.txt`.
- `texture-references/{primary-manifest,design-brief,depth-brief}.json`.

Native SHA-256: `4f7722a4fffc7001fafadc5c28a5b2401f6ab33edeb652006d482390abd55b9e`.
CTR library SHA-256: `625ce22111f84fd48712d7760d594341e84dee77ec3842db2665aadb03e53d6c`.

## Reproduction

Use the existing private input restoration and native build recipe, including
`LEGO_AOT_DIR` and `LEGO_SUPPLEMENT_CODE_BIN`. The latter regenerates the reviewed
supplement privately; neither original game inputs nor generated original-word
source belongs in Git. The finite launch remains:

```bash
python3 evidence/run_original.py continuation build-current/LEGOChaseNative \
  --fresh-save-data --host-event-limit 16384
```

Preserve the original-game and precision limitations above when resuming. The
preceding SaveData/private-memory and rendering records below are historical.

---

# October 28, 2026 — SaveData and private-memory cleanup

## Current result

The unchanged original game advances from dispatch round **1203** to **1469**.
The explicit per-title SaveData backend is published in `31dc20990bf78a125954dac54a02bfb24bb1527f`.
This follow-on completes the actual memory cleanup sequence reached afterward:
Unmap at round 1277 and the subsequent Protect at round 1278.

```text
stop=MissingBlock pc=0x00133788 detail=0x00000000 thread=1 dispatch_rounds=1469
guest_now_ns=618440772
```

The next task is authenticated build-time AOT coverage for the newly reached
ARM block. The PC is absent from both the saved source table and its linked
executable table, including recorded suffix coverage. The next recorded block
starts at 0x001337A0; the 24-byte gap is a candidate translation interval, not an
already validated or generated supplement. The private next-blocker brief
records original bytes, hashes, table evidence and the existing generation
route. Do not patch game bytes, fabricate runtime decode, or publish original
instruction arrays. No new AOT translation was generated in this milestone.

No visible game scene, menu, audible playback or gameplay is demonstrated.
The earlier completed GSP IPC is diagnostic context, not the MissingBlock cause.

## Implemented behavior and limits

SaveData opening uses a distinct configured container and typed handle table,
as documented in the preceding section below. The original missing-save reply
is real NotFormatted; configuration/opening create no title-save files. No
format request was observed. Existing-save success is checked with authored
filesystem fixtures, not a recovered personal game save.

The observed Unmap removes target 0x0E021000, which shares 8192 bytes with
source 0x083B5000. The source had been protected to None; Unmap requests restoration
to ReadWrite. `GuestMemory::UnmapUserAlias` verifies one complete recorded pair,
backing identity and offsets, then removes only target/alias metadata. The
source retains its bytes, physical identity, exclusive epochs and allocation;
commit-resource accounting does not change. All checks precede the
allocation-free metadata erasure. Partial pairs, unmatched pairs, flagged
operations and other source-restoration permissions remain explicit stops.

The next original Protect requests ReadWrite on the now-private source span,
which is already ReadWrite. The new fallback proves that the entire bounded,
aligned span lies in one private backing with exactly the requested permissions
and no overlap with any alias-source overlay. It then succeeds without changing
permissions, splitting regions, writing bytes or allocating. General private
permission changes, partial alias protection and multi-region cases remain
unsupported. Existing exact-alias Protect behavior is preserved.

Both SVC paths retain the existing deterministic unused-output policy (`r1=0`)
and advance PC by 4 on a handled result. This avoids reproducing the pinned HLE
wrapper's uninitialized output; it is host policy, not a hardware-value claim.
Pinned reference gaps—missing Map-pair checking and mutation before later
failure—are not reproduced. The native implementation preflights the entire
supported operation.

## Verification and recorded limitations

All **78 unique local CMake suites pass**, verified against the complete console
log and JUnit XML. The new SaveData suite and expanded alias suite cover exact
replies, real filesystem state, isolated handles, mapping identity, permissions,
nonzero bytes, epochs, unaffected neighbors/aliases, allocation-failure behavior,
request/output preflight and guarded unsupported shapes.

The first full local run could not execute three malformed generated outputs:
all lacked execute bits and one was empty. Those artifacts and the 75-test
initial result were preserved. Only the three disposable executables were
regenerated; the full 78-test run then passed. Native/CTR hashes and production
source remained unchanged. Normal process permissions were used for the
existing isolated privilege-drop filesystem tests. No test was disabled.

Separate original-game captures establish:

- The exact missing-save reply and all 67 preceding FS request/response pairs;
  SaveData stays empty, and the separate shared-extdata tree contains only the
  previously recorded 20-byte Play Coin file.
- The actual Map/Protect/Unmap relation before implementation, including source
  permissions None and a readable target sharing the same physical bytes.
- Successful original Unmap with all 8192 bytes preserved, source RW restored,
  target absent and every other alias unchanged.
- Successful following Protect with all 8192 bytes and all 1024 epoch tokens
  (27 nonzero) unchanged, along with permissions, alias metadata, CPU reservation
  fields and guest time. This is evidence about the host memory model, not
  measured hardware exclusive-monitor behavior.
- Identical normalized launch arguments, complete stdout, CPU/stop fields and
  final MissingBlock in ordinary and instrumented execution.

**Full final audio-artifact equivalence is incomplete.** Ordinary execution
retains 314,304 bytes for 19,642 reported records. The first final capture retained
only 312,672 bytes; the one authorized retry retained 314,256 bytes, missing 48 bytes
(three records). Each retained prefix exactly matches ordinary output, and both
processes reported the same record count and successful close. The cause is
unresolved. Failed files/receipts remain intact. No tail was reconstructed, no
success was asserted for the strict audio gate, and the original strict verifier
still rejects both files. A separately named memory/CPU proof reports that
scope explicitly. Earlier intermediate captures have full audio equivalence;
that does not make the final incomplete captures complete.

Authoritative private evidence includes:

- `save-memory-final-build.{log,json}` and `save-memory-final-tests.{log,xml,json}`.
- `savedata-open-capture-dwyailt_/savedata-proof.json`.
- `unmap-baseline-capture-e9xuodl2/unmap-baseline-proof.json`.
- `save-unmap-capture-1dhv31j1/unmap-proof.json`.
- `save-protect-capture-retry-b1_9_0sq/memory-state-proof-with-audio-limitation.json`.
- `save-protect-native-g22huee4/receipt.json` and `next-aot-blocker.json`.

These paths are beneath the private checkpoint's `evidence/` directory. Hosted
GCC/Clang/sanitizer results are collected for the exact published revision and
recorded separately from private original-game execution. Source is preserved
in GitHub; private evidence contains source identity metadata, not another copy
of the repository. Original code.bin, ExHeader, RomFS and saved AOT are unchanged.

## Reproduction

Use the existing native build instructions and separately preserved original
inputs. The finite original run requires the explicit fresh save selection:

```bash
python3 evidence/run_original.py continuation build-current/LEGOChaseNative \
  --fresh-save-data --host-event-limit 16384
```

Private capture rebuilding and scope are documented in
`evidence/protect_final_capture/README.md`. Preserve the recorded audio
limitation when reusing the state proof. The current stop was reached without
hitting or increasing the 16,384-event cap.

The preceding SaveData-only and drawing/transfer milestones remain below as
historical records.

---

# October 28, 2026 — explicit per-title SaveData opening

## Current result and next task

The original game's `OpenArchive(4, Empty)` request now executes through a
separate title-save backend. On an explicitly selected fresh save container,
the real missing-save response is `0x080C00C0, 0xC8A04554, 0, 0`, followed by
zeroed unused reply words. It allocates no archive or kernel handle, changes no
save files, and ignores the incidental Empty-path byte `0x3c` after validating
its readable span.

The unchanged original game advances from round **1203** to **1277**:

```text
stop=UnsupportedSvc pc=0x0025c9f4 detail=0x00000001 thread=1 dispatch_rounds=1277
r0=0x00000005 r1=0x0e021000 r2=0x083b5000 r3=0x00002000 r4=0x00000003
guest_now_ns=534871200
```

This is `ControlMemory` operation5 (Unmap): target0x0e021000,
source0x083b5000, size0x2000, requested source permissions3. The last IPC name
is `gsp::Gpu`, but that earlier completed IPC is not the stop cause. No FS
format request follows the missing-save reply in this run. Next work is to
implement the actual inverse alias operation after checking the recorded
mapping and pinned source, preserving bytes and restoring source permissions.

No visible game scene, menu, audio playback or gameplay is demonstrated.

## SaveData behavior

`--save-data-root DIR` is opt-in and requires an existing host container. The
initialized FS session supplies the program ID; lowercase numeric components
select `<root>/<program-high:08x>/<program-low:08x>/data/00000001`.
Configuring or opening does not create directories, metadata or game files.
An omitted host selection remains an explicit unsupported capability; a
configured but absent title save returns NotFormatted. Existing real save
directories need no metadata file for OpenArchive and receive opaque u64
handles from a separate typed table. File operations and formatting are still
unsupported; shared-extdata file semantics are never substituted.

The POSIX backend pins the selected root and descends with descriptor-relative,
no-follow directory opens. Shared-extdata allocation reserves the SaveData
handle namespace. Unsupported hosts receive no pathname-only fallback.
Unexpected host I/O errors stop before committing a guest reply or allocating
a save mount. A real existing save is left unchanged by OpenArchive.

This follows the local backend semantics in
[Azahar 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac](https://github.com/azahar-emu/azahar/blob/86a9f9236ae42bb5a2b995dbc933d599d8ea07ac/src/core/file_sys/archive_source_sd_savedata.cpp),
with explicit containment and handle typing as host policy. The private
reference manifest records twelve primary-source files and their Git/SHA-256
identities. Those sources do not establish measured console filesystem timing.

## Verification

The new SaveData suite and seven existing FS/RomFS suites pass: **eight focused
suites**, with **78** suites now registered overall. Tests cover the exact
failure reply, missing versus unconfigured storage, ignored non-NUL Empty
payload, real existing directories, session/program binding, handle separation,
response preflight, symlink rejection, pinned-root replacement and real host
permission failure. The sandbox's uid mapping blocks the existing isolated
privilege-drop test; the authorized run with normal process permissions passes.
No existing test was disabled or changed.

The ordinary run and separate FS-only logging capture match normalized launch
arguments and complete stdout, CPU/stop fields, and all **270,528 DSP audio
bytes**. The capture contains **67** complete FS request/response pairs. Its
last FS operation is the real missing-save reply at501570401ns, with38 kernel
handles before and after. The fresh SaveData container stays empty. The
separate shared-extdata tree contains the same20-byte `gamecoin.dat` generated
by the already-supported earlier PTM path; it is not a new title save.

Private evidence: `savedata-open-native-blxjkvde`,
`savedata-open-capture-dwyailt_/savedata-proof.json`, `save-data-local-tests.json`,
`save-data-open-build.json` and `save-data-references/manifest.json`, all under
`evidence/`. These inputs/captures are not part of public Git. Public source is
preserved in the real repository history; the private checkpoint carries its
source identity and complete proof/log data. Original code, ExHeader and RomFS
hashes remain unchanged.

After the documented native build, the private finite-run helper reproduces
this state with an independently created empty save container:

```bash
python3 evidence/run_original.py continuation build-current/LEGOChaseNative \
  --fresh-save-data --host-event-limit 16384
```

The prior drawing/transfer milestone and its evidence remain below as history.

---

# October 28, 2026 — native color draws and framebuffer crop

## Current outcome and next stop

The native runtime now completes the original draw that stopped checkpoint
`5563791c095e1ff8c76f8d79cdd3cf76764322c5` at dispatch round 843. It also completes
the next unsupported framebuffer transfer, reached at round 1109. No original
game instructions, uploaded shader program, AOT pages, assets, or input files
were patched to obtain this progress.

The latest ordinary original-game run, with the existing launch profile and
`--host-event-limit 16384`, reports:

```
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=8 dispatch_rounds=1203
last_ipc_session=fs:USER request_header=0x080c00c2
guest_now_ns=501570401
request_words=080c00c2 00000004 00000001 00000001 00004002 0e022d3c
```

This is `OpenArchive` for archive ID 4 (SaveData), an Empty path of declared size
1 in static buffer 0. The captured payload byte is `0x3c`, not NUL; the Empty
path type governs its semantics, so it must not be interpreted as a filename.
The current filesystem implementation supports a specific
shared-extdata OpenArchive path, not a title save-data archive. The next task is
to implement a distinct, contained title-save backend and its real missing-save
and handle semantics. Do not widen the shared-extdata ID check or invent a
successful save mount. The private checkpoint includes a pinned-reference brief
and the observed request/path capture.

The default 4,096-event diagnostic budget first stopped at round 1087 with
`HostEventLimit`. Raising only that finite host budget to 16,384 exposed the real
subsequent transfer and filesystem stops. This is a diagnostic budget change,
not a CPU timing fix, skipped operation, or hardware speed measurement.

No visible game scene, menu, music playback, or gameplay is demonstrated. The
verified startup color buffers are black; native drawing and device transfers
are now real operations, but these results do not establish useful presentation.

## Implemented drawing path

`src/services/pica_rasterizer.{h,cpp}` adds an independently authored, bounded
software color stage. It consumes the existing fetched/shaded vertex outputs and
known register state, maps output semantics, assembles fresh triangle lists or
strips, performs viewport conversion and pixel coverage, interpolates colors with
perspective correction, applies the supported TEV operations and blending, and
stages channel-masked tiled RGBA8 writes.

Supported TEV operands are primary color, stage constants, and previous stage;
operations are unscaled Replace and Modulate with the implemented direct
operands. Additive blending supports Zero, One, SourceAlpha, and
OneMinusSourceAlpha factors. LogicCopy is supported when blending is disabled.
The framebuffer uses 8-by-8 Morton tiles, ABGR bytes, and the supported vertical
orientation. Unknown consumed fields and unsupported states stop explicitly.

Coverage and arithmetic follow the pinned Azahar software model. Arithmetic
uses separate binary32 rounding boundaries, with explicit finite normal/zero
checks and nearest host rounding. This is not measured hardware float24 or
subpixel parity. User clipping, clip-volume crossings, scissor tests, indexed
fetch, geometry shaders, textures, fog, other fragment tests/formats, unsupported
blend/TEV operations, and the other framebuffer orientation remain guarded.
Unused register storage is not evidence that a consumed field is known.

The decoder supplies an optional draw staging callback only when the GSP queue
owns a device transaction. Calling the decoder alone still rejects draw
execution. Primitive state begins empty according to the reference and resets
on topology/restart writes. A nonempty draw requires another explicit reset
before a later draw; retained strip/fan or partial-list state is not fabricated.

The queue keeps a lazy independent copy of established VRAM for preflight.
Earlier device writes are visible to later draws, and later transfers can read
staged draw output. FCRAM dependencies that are not forwarded remain explicit
stops. A later bad list, packet, or IRQ preflight discards all staged draw bytes
and state. On success, actual device patches commit before the associated P3D
interrupt. A draw does not invent an IRQ when its list did not request one.
Existing synchronous timing and event semantics remain in force.

Host bounds are 4,096 fetched vertices and 1 MiB of source reads per draw; the
existing shader bounds remain. A raster framebuffer is at most 1 MiB, each draw
has at most 4 Mi candidate-pixel tests, and a complete eligible queue accepts at
most 16 draws and 16 Mi candidate-pixel tests. Remaining queue raster work is
preflighted before fragment loops. These are containment limits, not hardware
capacity claims.

## Unscaled framebuffer transfer

`DisplayTransfer` flag `0x1004` now supports a left/top crop from tiled RGBA8
physical VRAM to linear RGB8 physical VRAM. It preserves the input tile-row
stride, drops alpha, and copies BGR channels without filtering or horizontal
halving. Odd or partial-tile output dimensions are valid. Source and output
spans are bounded independently; alignment, overlap, capacity, and atomic
staging checks remain mandatory.

The observed packet is source `0x1f084800`, input 256 by 400, destination
`0x1f038400`, output 240 by 320, flags `0x1004`. It produces 230,400 bytes and
requests a real PPF completion. Existing scaled `0x01001004` and RGBA4 `0x4400`
paths remain supported. No screen-clear substitution is used in either stage.

## Verification and evidence

All 77 runtime suites pass locally, including the two new raster/draw suites and
expanded transfer coverage. Authored nonzero fixtures check triangle coverage,
shared-edge ownership, perspective interpolation, TEV and blending, masks,
framebuffer tile boundaries, work limits, late-failure atomicity, real queue
commit and waiter/IRQ behavior, draw/transfer ordering, and complete rollback.
Transfer fixtures include odd crops and the original 256-by-400 to 240-by-320
shape, with independent bytes that distinguish copying from averaging.

Independent private raster checks preserve all four original shader colors,
including alpha `0x3f7fffff`, and calculate every covered sample. The original
first draw covers 192,000 pixels with two triangles and 384,000 candidate-pixel
tests. Nearest-byte color conversion produces alpha 255, followed by TEV alpha
238. Every byte of the 6,291,456-byte VRAM bank is compared for both the captured
input and an arbitrary seeded background, including all masked/uncovered bytes.
The seeded first-draw case changes 573,732 RGB bytes. The captured original case
changes zero because its target was already black; this is not evidence of a
substitute no-op.

The initial integrated capture verifies all 121 queue traces, the actual first
draw's queue retirement and P3D event, and atomic rejection at the subsequently
unsupported transfer. It records seven committed draws totaling 28 vertices,
504 shader instructions, 14 triangles, and 1,228,800 covered pixels. Final
proofs rerun the raster and transfer stages against the final production build,
and the final integrated capture demonstrates the transfer commit before the
new filesystem stop. That final capture verifies all 147 queue traces and
records 11 draws, 44 vertices, 792 shader instructions, 22 triangles, and
1,766,400 covered pixels. Both observed `0x1004` transfers (queues 121 and 147)
match the full-VRAM oracle and exact dequeue/PPF bytes. Exact proof paths,
hashes, counters, source identity, and
hosted GCC/Clang/sanitizer outcomes are in the private continuation receipt.

Several generated executables in the recovered cloud build cache had invalid
headers or modes; they were regenerated and checked before the final local
suite. The existing filesystem permission test ran with normal process
permissions so its isolated child could drop privileges. No test was disabled
or weakened. Hosted workflows rebuild public source in clean environments.

Original code.bin, ExHeader, and RomFS still match their saved SHA-256 values.
The original 599-page AOT archive and authenticated three-block supplement are
unchanged. Public Git contains only source, authored tests, and documentation;
game inputs, raw GPU captures, audio, and private proofs stay in the recovery
checkpoint. Source is preserved by the real GitHub revision and hosted source
snapshot; the private archive carries source identity metadata and evidence.

## Reproduction and continuation

Use the saved private input manifest and the real repository history. From the
recovery directory with `repo/`, the separately saved game inputs, prepared
RomFS, and restored AOT pages in their documented locations:

```bash
cmake -S repo -B build-current -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DLEGO_AOT_DIR="$PWD/restored-aot/generated2" \
  -DLEGO_SUPPLEMENT_CODE_BIN="$PWD/recovered-inputs/LEGO-Chase-Recovery/code.bin"
cmake --build build-current --parallel 4
ctest --test-dir build-current --output-on-failure
python3 evidence/run_original.py continuation build-current/LEGOChaseNative \
  --host-event-limit 16384
```

Rebuild native diagnostic helpers using the private checkpoint instructions;
compiled artifacts are deliberately excluded. Private replay and capture tools
verify immutable original inputs and distinguish offline staging from live
queue commit. New runs use fresh owned test-state directories. Do not reuse
unknown saves or substitute a cleared image for actual draw execution.

Primary reference: [Azahar at 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac](https://github.com/azahar-emu/azahar/tree/86a9f9236ae42bb5a2b995dbc933d599d8ea07ac),
`pica/primitive_assembly.cpp`, `pica/output_vertex.cpp`, PICA register definitions,
`renderer_software/sw_rasterizer.cpp`, `sw_clipper.cpp`, `sw_framebuffer.cpp`,
`sw_texturing.cpp`, `sw_blitter.cpp`, and common color/vector helpers. The private
reference manifests record exact URLs and content hashes.

Continue in the assistant cloud workspace with ordinary build tooling. Keep
chat replies to one screen. Do not launch Codex tasks, operate the user's PC,
or change Dot's project. Preserve checkpoints and advance the next verified
blocker without repeated approval prompts.

---

## Prior checkpoint record — attributes and staged vertex execution

The following section is historical evidence for the predecessor checkpoint;
its draw stop and missing-raster descriptions have been superseded above.

# October 28, 2026 continuation update

## Verified restoration and current stop

The saved checkpoint `d83daf2a1a39ac6478dd8b7993af6827102de59a` was restored and
matched against the current Git history: all 490 source blobs and modes match.
The checkpoint verifier passed all 836 manifest files. The executable, ExHeader,
599-page AOT archive (603 regular files), both prepared RomFS parts, and combined
769,179,648-byte RomFS matched their recorded hashes. No new game upload or CCI
extraction was needed. Original game bytes and generated AOT pages are unchanged.

A clean GCC native build with the authenticated three-block supplement reproduced
the previous `round843 / pc0x0025947c / attributes0x232 at byte0x380` stop exactly.
After adding default-attribute support, the unchanged original run now reports:

```
stop=UnsupportedIpc pc=0x0025947c detail=0x00000032 thread=1 dispatch_rounds=843
last_ipc_session=gsp::Gpu request_header=0x000c0000
host_ipc_error=PICA draw execution is unimplemented at list byte 0x4e8 register 0x22e
```

This advances processing within the same 1,376-byte GPU list. It does not advance
the guest past the queue request: the existing complete-queue rejection policy
keeps every staged attribute/register/IRQ change uncommitted. No rendered frame,
logo, title screen, menu, music playback, or gameplay is demonstrated.

## New default-attribute implementation

`src/services/pica_startup.{h,cpp}` now models the three-word packed float24 FIFO,
raw component values, corresponding IEEE32 container bits, known/written state,
full-width cursor, and stored default attributes 0 through 14. All three data ports
feed the same FIFO; partial vectors persist across successful list boundaries.
Index writes reset the FIFO; masked and zero-mask index writes are supported after
a complete index has established its value. Completing attribute14 advances to15
without wrapping. Index15 vertex submission remains an explicit rendering stop.

Containment policy: new data ports require full byte mask0xF; an unknown selected
index, unsupported index, or corrupt FIFO count stops before data-port effects.
The pinned emulator's upload-mask TODO does not establish hardware partial-mask
semantics. The supported 0..14 range follows that emulator; Citro3D's public API
exposes twelve default attributes. None of this claims complete hardware timing
or float24 arithmetic fidelity. Draw, chain, and fog guards remain in force.

Focused GCC checks passed: `ctr_pica_attributes_test`, `ctr_pica_startup_test`,
`ctr_pica_proctex_test`, `ctr_pica_swizzle_test`, `ctr_pica_vertex_fetch_test`,
`ctr_pica_vertex_shader_test`, and `ctr_gsp_submit_test`. They cover independently specified
packed values, cursor/FIFO behavior, known-state/bounds policies, and a later
GSP-packet failure preserving already committed state and suppressing staged IRQs.
The native original-game run is separate evidence from these ROM-free tests.

## Native vertex loading

`src/services/pica_vertex_fetch.{h,cpp}` adds a staged, read-only non-indexed
vertex loader and vertex-shader input mapping. It decodes signed bytes, unsigned
bytes, signed shorts and float32 arrays, including component alignment, padding
descriptors, separate loader offsets/strides, uploaded default attributes and
input-register permutations. Per-byte register knowledge prevents allocated zero
storage or mask-zero writes from establishing unknown configuration fields.

Reads are restricted to readable private linear FCRAM or an explicitly supplied
VRAM bank. Shared/device callbacks, unknown configuration, duplicate loaders,
retained attributes, indexed draws and geometry shaders remain unsupported.
The explicit host bounds are 4,096 vertices and 1 MiB of source reads per stage.
Failure preserves the caller's previous output plan and all source state.

The original first draw uses four vertices with a 32-byte stride: float3 position,
two padding descriptors, unsigned-byte4 color, and four uploaded defaults. The
new loader performs eight attribute reads totaling 64 bytes. These are shader
inputs from a discarded command-list prefix; they do not constitute draw execution.

## Native vertex-shader execution and original-data proof

`src/services/pica_vertex_shader.{h,cpp}` adds the bounded MOV, MUL, DPHI, MADI,
CALLU and END instruction families. The stage tracks known components through
input, temporary and output registers, applies operand swizzles/signs and write
masks, follows conditional calls, and compacts enabled outputs in register order.
Unknown program words, descriptors, uniforms or consumed components are explicit
failures; no missing instruction is replaced with an implicit END. A selected
boolean's byte must be established, even if the upload-observed flag is set.

The numerical model follows the pinned reference's float32 containers, with a
separate rounding boundary for each multiply and addition. It does not claim
hardware float24 precision. Arithmetic requires nearest host rounding and finite
normal/zero operands/results; subnormal, underflow and nonfinite arithmetic stop.
MOV preserves raw bits. Relative addressing, geometry shaders, unsupported
opcodes, ambiguous call boundaries and calls deeper than three remain unsupported.
Host bounds are 1,024 executed instructions per vertex and 1,048,576 per plan.
Source state is immutable and a failed stage preserves the prior output plan.

The complete private original capture verified all 78 queue traces. Its first-draw
prefix contains the four default uploads and the four vertices described above;
the rejected queue and diagnostic replay leave registers, uploads, shared GSP
memory, VRAM, requester event state and guest time unchanged. The final VRAM image
matches the previous depth-fill-only capture. An earlier diagnostic attempt had
one empty trace file and is explicitly marked failed/superseded; the complete
capture uses checked atomic JSON writes and sequence-completeness verification.

An offline helper then calls the actual production shader stage using those
captured vertices and the unchanged uploaded shader program/descriptors/uniforms.
Each vertex executes 18 instructions; all four produce five output vectors.
All 80 output component bit patterns match an independent exact-rational oracle
that rounds after every operation to binary32, ties to even. It independently
checks 72 executed instructions, 144 multiplies and 96 additions. This is genuine
shader execution in a read-only replay, not a committed GPU draw or rendered frame.

The private checkpoint contains `evidence/captured-final-1z19nwkg/capture`, its
`attribute-proof.json`, `evidence/shader-replay-x5bf1hxx/shader-proof.json`, the
raw captures, authored replay/verification tools, and focused build/test receipts.
Hosted checks and exact publication identity are recorded in the downloadable
continuation receipt after publication. No raw captures are in public Git.

## Immediate next work

Implement primitive assembly and rasterization, starting from the already verified
first-draw shader outputs. The captured configuration calls for a triangle strip,
output-semantic mapping, clipping/viewport conversion, texture-disabled TEV
combining, alpha blending and masked RGB writes into the tiled RGBA8 framebuffer.
The viewport is 480 by 400; the framebuffer is 512 by 400. Depth/stencil writes
and tests are disabled for this draw. The private
`evidence/first-draw-raster-requirements.json` records decoded fields and their
known-byte provenance; treat it as configuration evidence, not pixel results.

The current recovered source has register/upload storage, vertex loading, bounded
shader execution, fills and display transfers, but no complete draw pipeline.
Do not acknowledge a draw or retire the queue until the required work is implemented.
Private capture evidence must distinguish staged prefix attributes from live state.

## Fresh recovery and reproduction

Extract the new source/evidence archive into a new owned directory after its
`verify_checkpoint.py` check passes. The archive contains `repo/` as an exact
source snapshot, without Git metadata. For continued development, clone this
GitHub repository with its real history, check out the receipt's commit, and
compare its tree with `SOURCE-INDEX.json`; do not manufacture snapshot ancestry.
Use the saved snapshot if GitHub is unavailable, while preserving the recorded
commit/tree identity for later reconciliation.

Materialize the separately saved inputs named in `evidence/recovery-inputs.json`
under `recovered-inputs/LEGO-Chase-Recovery/`, retaining their subfolders. Verify
code, ExHeader and AOT hashes from `evidence/restored-inputs.json`; the RomFS
restoration tool verifies each part and the combined hash. No original CCI
extraction or user re-upload is needed. Current helper layout:

```
repo/
evidence/
recovered-inputs/LEGO-Chase-Recovery/code.bin
recovered-inputs/LEGO-Chase-Recovery/Prepared-Launch/exheader.bin
recovered-inputs/LEGO-Chase-Recovery/Prepared-RomFS/romfs-parts.json
recovered-inputs/LEGO-Chase-Recovery/Prepared-RomFS/LEGO-Chase-USA-romfs.bin.part001
recovered-inputs/LEGO-Chase-Recovery/Prepared-RomFS/LEGO-Chase-USA-romfs.bin.part002
restored-aot/generated2/
romfs.bin
build-current/
```

After checking the recorded AOT archive hash, extract its regular `generated2/`
members into `restored-aot/`, rejecting absolute paths, parent traversal and links.
Run these commands from the containing recovery directory with Python 3.12,
CMake, Ninja, a C++20 compiler, Clang and `llvm-objdump` installed. Clang/LLVM are
used only to regenerate the authenticated three-block AOT supplement at build time:

```bash
python3 repo/tools/restore_romfs_parts.py \
  recovered-inputs/LEGO-Chase-Recovery/Prepared-RomFS/romfs-parts.json romfs.bin
cmake -S repo -B build-current -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DLEGO_AOT_DIR="$PWD/restored-aot/generated2" \
  -DLEGO_SUPPLEMENT_CODE_BIN="$PWD/recovered-inputs/LEGO-Chase-Recovery/code.bin"
cmake --build build-current --parallel 4
ctest --test-dir build-current --output-on-failure
python3 evidence/run_original.py restored-native build-current/LEGOChaseNative
python3 evidence/startup_capture/build_capture.py --vertex-fetch
python3 evidence/run_original.py restored-capture evidence/startup_capture/LEGOChaseCapture --capture
```

Verify the fresh printed capture directory with `verify_capture.py --require-vertex`
and pass its first-draw candidate JSON to `run_shader_replay.py`; exact commands
and scope are in `evidence/startup_capture/README.md`. The archived complete capture
can also be replayed offline without rerunning the game. The native stop remains
expected exit 3 at round 843. Each run wrapper creates fresh owned state; it does
not reuse unknown saves. Old absolute paths in receipts are provenance, not
mandatory locations. Compiled executables/libraries/objects and generated AOT are
rebuilt; capture bytes, startup audio records, tools and verification are retained.

## Reference pins

- Azahar `86a9f9236ae42bb5a2b995dbc933d599d8ea07ac`,
  `src/video_core/pica/pica_core.cpp`: default-index reset, FIFO ports, full-width
  cursor increment, and default/immediate split.
- Same pin, `src/video_core/pica/packed_attribute.h`: four float24 values in three
  words; `src/video_core/pica_types.h`: lossless container conversion.
- Same pin, `src/video_core/pica/vertex_loader.cpp`, `regs_pipeline.h`,
  `regs_shader.h` and `shader_unit.cpp`: array layout, padding, component formats
  and shader input-register mapping. The new loader is an independent bounded
  implementation, with unsupported reference cases rejected explicitly.
- [Azahar pinned source](https://github.com/azahar-emu/azahar/tree/86a9f9236ae42bb5a2b995dbc933d599d8ea07ac),
  `src/video_core/shader/shader_interpreter.cpp`, `pica_types.h` and
  `pica/shader_unit.cpp`: arithmetic ordering, conditional calls and output mapping.
- [nihstro pinned instruction format](https://github.com/neobrain/nihstro/blob/f4d8659decbfe5d234f04134b5002b82dc515a44/include/nihstro/shader_bytecode.h):
  opcode families, operand widths, swizzle fields and flow-control encoding.
- Citro3D `9f21cf7b380ce6f9e01a0420f19f0763e5443ca7`, `source/base.c` and
  `source/immediate.c`: independent fixed-attribute upload and packing behavior.

## Operating and recovery rules

Continue in assistant cloud scratch with ordinary compiler/build tooling. Do not
invoke Codex or operate the user's PC or Dot. Keep user-facing replies brief.
Preserve private game data and raw captures outside public Git. Use the real Git
history and non-forced publication; never replace it with snapshot ancestry.
Save an updated handoff and a recoverable private source/evidence checkpoint after
meaningful work. The prepared inputs below remain the recovery source of truth.

The following October7 handoff is preserved as the verified predecessor, including
input hashes and launch flags. Its old attribute stop and scratch paths are historical;
the current stop is the draw request above. Use the new checkpoint receipt for
current paths, source revision, checks and saved evidence.

---

# LEGO Chase Begins — canonical continuation handoff

October 7, 2026. Continue in assistant scratch, NOT the user's PC or Work.
LEGO City Undercover: The Chase Begins, Nintendo 3DS USA. Native/headless startup
reconstruction, NOT playable. POST UPDATED DOWNLOADABLE MARKDOWN AND A COMPLETE
SOURCE/EVIDENCE CHECKPOINT EVERY WORK TURN. Only show meaningful genuine output;
no rendered logo, title screen, main menu or worthwhile screenshot exists.

## Source and recovery

The commit containing this source extends published main
`b6ea4709af883666f9b0eec9cb0af74b5926f7e2`, exact baseline tree
`3167ec7ebe8f69f5abfae950cfb757743ddac98f`. The appended downloadable receipt names
actual publication, tested tree, hosted checks and durable backup. Local Git is a
reconstructed snapshot/index, NOT remote history. Re-read main and reconcile any
intervening work before non-forced publication. Never force-push snapshot ancestry.

The provided b6ea470 checkpoint passed 764 manifest and 484 source-blob checks and
reconstructed the complete baseline tree before edits. Its archive SHA256 is
`d25508e307537b23abb176e9f236ede8702b0a0debb6c1e42c40b3ec8ae11e8d`.
Baseline full GCC build/all69 suites and the original DSP cache stop at round592
were reproduced. Original code/AOT and prepared RomFS parts were already present;
prepared ExHeader was restored from the checkpoint. RomFS was reassembled and its
whole hash checked. No CCI extraction, user reupload or user-PC operation was needed.
The predecessor remains separately preserved; do not recursively duplicate history.

## Actual new original-game progress

Eight DSP FlushDataCache calls and one GSP FlushDataCache call now succeed, covering
86,808 bytes. Target bytes and 64 captured neighbours per call remain identical;
exact reply shape, guest time, handle/thread counts remain verified. This memory
model is cacheless and already coherent. No copying or simulated cache work is
required for the supported private-memory spans. External DSP AHB/FCRAM remains
unsupported, and success does not assert an external bus transaction or device work.

The original game then reaches previously unsupported VFP short-vector settings.
The first operation at 0x00258AF0 has a three-lane FPSCR setting but a scalar-bank
destination. Bounded VFPv2 single-precision support now executes that original routine
and later vectors. Final original trace contains58 calls:4 scalar-bank calls,
6 three-lane calls and48 four-lane calls,214 lane operations. No game instruction,
AOT word, supplemental range or result was patched to force progression.

Independent Python rational binary32 replay checks ALL32 before/after register
words for every call. It covers the actual finite round-to-nearest operands and
separate multiply/add rounding. The original FPSCR/CPSR values remain unchanged
on this observed path. This is not a general independent ARM emulator or a claim
of full VFP exception/overlap/hardware-timing coverage.

## Exact current next stop

```
stop=UnsupportedIpc pc=0x0025947c detail=0x32 thread=1 dispatch_rounds=843
last_ipc_session=gsp::Gpu request_header=0x000c0000
host_ipc_error=PICA default/immediate attributes are unimplemented at list byte 0x380 register 0x232
cpu_ticks=85142780 core0_instructions=14847201 core1_instructions=99
quota_transitions=299 display_periods=19 guest_now_ns=317564398
DSP scheduled_slices=2447 notification_wait_slices=1 completed/attempted=40140800
```

The original pending command list is1,376 bytes. Its first default/immediate
attribute selection is index2, register0x232 at byte0x380. It also contains12 packed
attribute data-port words. The ENTIRE queue request remains uncommitted: live GPU
registers, upload state, GSP shared page and VRAM are unchanged by rejection. No
attribute, draw, queue advance, P3D notification or completed frame is fabricated.

Seven threads/37 handles remain. English and Spanish stop at round843; French845.
Without language the old round569 stop remains; without sound295; without camera235.
The original core1 worker has still issued only99 instructions, not gameplay.

The audio file holds9,794 complete records, all actual FIFO source-mask3 and zero
startup samples, with no fallback frames. It is not music, effects or playback.
VRAM contains only the inherited819,200-byte depth clear at offset0x419400, repeating
little-endian0x00FFFFFF; every other byte is zero. This is not a rendered picture.
No HID sample producer, executed game shader, logo/title/menu or gameplay exists.
Do not present depth-clear patterns or silent captures as worthwhile media.

## Cache contract and safeguards

DSP0x00130082 and GSP0x00080082 decode two u32 values and exactly one copied process
object (descriptor0). Current-process pseudo-handle0xFFFF8001 and actual duplicate
handles resolve to the SAME ProcessObject. Wrong/stale types return transport
InvalidHandle; another process object stops unsupported. Nonzero ranges must lie
within one readable private backing, respecting protected alias sources. Cross-region,
overflow, unmapped, shared and device spans reject before target callbacks or writes.
Zero size performs no range work after process checks. DSP requires its healthy
attached live device; GSP maintenance does not require/acquire GPU ownership.

Full normal reply preflight and private readable/writable reply validation run before
reply creation. Success modifies only the IPC response: no target read/copy/write,
reservation epoch invalidation, cache allocation, device callback, DSP instruction,
guest-time charge, interrupt, mapping or ownership change. Existing coherent alias
backing is retained. InvalidateDataCache remains unsupported. These bounded range
and failure policies are host containment, not measured firmware error precedence.
A successful flush never means external AHB/FCRAM or renderer cache behavior exists.

## VFP short-vector contract and limitations

New helper in the existing scalar backend expands supported single-precision vector
forms into the SAME bit-exact scalar arithmetic. Eight-register banks wrap circularly;
FPSCR length/stride apply; S0..S7 destinations force one scalar operation. Fm in
S0..S7 broadcasts, while Fn advances. Supported arithmetic is MLA/MLS/NMLA/NMLS,
MUL/NMUL,ADD/SUB/DIV and ABS/NEG/SQRT. Existing comparisons/conversions stay scalar.
Supported strides are1/2; reserved stride encodings and repeated-bank vectors reject.

Shifted cross-lane source/destination overlap is conservatively rejected before
mutation; identical per-iteration overlap is allowed. Enabled exception traps,
double vectors and unimplemented opcode forms retain their stop. Candidate state
holds all lane effects until success; sticky flags accumulate through scalar calls,
LEN/STRIDE controls are restored, and PC advances once. Numeric scalar algorithms
are unchanged. This is deliberately not full VFPv2 or hardware vector timing.
Original game and saved AOT bytes are untouched. Vendor LOCAL-PATCHES.md/provenance.json
record the new wrapper separately from the upstream blob and historical local edits.

## Tests, captures and scope

Full GCC/Clang builds link599 original AOT page units plus the existing three
build-generated authenticated supplemental blocks. All72 configured CTest suites
pass with both compilers and ROM-free Clang ASan/UBSan, with leak checks/halt-on-error.
JUnit names exactly match configured names, without skips or duplicates. Earlier69
suites remain unchanged. Three new suites cover DSP/GSP cache contracts and VFP
banks/stride/scalar rules, supported arithmetic, overlap/fault containment and flags.

Six ordinary compiler pairs match stdout/stderr/exit/owned files:en,fr,es,unset
language,unset sound,unset camera. All69 final capture files match byte-for-byte.
Independent checks verify nine exact cache replies/unchanged target spans, all58
VFP calls, failed queue atomicity, parsed original list, complete audio and depth-only
VRAM. Logging-only IPC/VFP/main objects link the same production libraries/AOT;
normal CLI reproduces the same stop after normalizing only the owned-root path.
This is not a full ARM/DSP address-space dump or independent CPU emulator.

Original code.bin, whole raw RomFS, ExHeader and all603 saved AOT members match their
recorded hashes. GCC/Clang supplement C++ is identical. Full IVFC checking was not
repeated; no Windows/macOS, hardware-timing, renderer or playback test ran.
A foreground build was interrupted by its tool timeout, then completed using a
tracked finite driver. Streaming sessions were unavailable. Every initiated job
was collected. Initial/intermediate stops and logs are not final test evidence.

Source changes: cache predicate in ctr_memory; DSP/GSP handler declarations/dispatch;
new dsp_cache_service.cpp; existing vendor a32_vfp_scalar.cpp vector wrapper;
CMake/test lists; three tests; vendor patch metadata and reports. No DSP peripheral,
ARM scheduler, original opcode bytes/AOT, GPU renderer, FS, CFG or HID sampler changes.

## Exact next work

Inspect the captured pending list in cache-checkpoint/capture-final/gcc/
pending-list-0-0.bin and pinned PICA default-attribute/index/packed-value behavior.
Implement real stored attributes and known-state/cursor semantics with proper bounds
and whole-list/queue failure behavior. Do not remove draw/chain guards, assume these
are rendered vertices, inject pixels or signal completion for an unexecuted list.
Rerun unchanged original code in NEW owned state and follow its actual next request.
No additional original-game upload is needed. Useful visuals remain the goal.

All earlier modes remain explicit. Audio file output avoids the old finite-prefix
capacity stop. ARM one recorded instruction/core/nominal tick, synchronous DSP
boot/waits and scheduled DSP slices are diagnostic conventions, not measured
hardware latency or physical parallelism. ExHeader verifies Multi/max30 only.
Use singular --dsp-boot-mode reference-slice. Do not silently select old fallback
silence or drop reset/special/CFG/VRAM/PTM/CPU policies.

Primary cache references: Azahar86a9f9236ae42bb5a2b995dbc933d599d8ea07ac,
src/core/hle/service/dsp/dsp_dsp.cpp blobf8b23c07925c6b4a9fe36acaa7954f05126441f5;
src/core/hle/service/gsp/gsp_gpu.cpp blob6f915e4d6a5d27853321d0a233103afb9d877bc0.
VFP: ARM VFP11 TRM DDI0274H sections2.7/3.4.2, tables2-7/2-8/3-7, official URL
https://documentation-service.arm.com/static/5e8e227c88295d1e18d377ac . Parsed text
was read; requested screenshots/direct download failed. No page-image inspection
or new hardware measurement is claimed. Exact read scopes are in references.json.

## Scratch and complete reproduction

Root /mnt/data/lego_recovery/. Source repo/. Full builds build-gcc/,build-clang/;
ROM-free build-asan/. AOT generated2/; code restored/code.bin; raw RomFS
 game/prepared-romfs/romfs.bin; ExHeader prepared-launch/exheader.bin;
parts/manifest romfs-library-roundtrip/. Current work cache-checkpoint/.
Historical predecessor restored-b6ea470/ remains separate; never overlay it on repo/.

```sh
cd /mnt/data/lego_recovery
cmake -S repo -B build-gcc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DLEGO_AOT_DIR=/mnt/data/lego_recovery/generated2 -DLEGO_SUPPLEMENT_CODE_BIN=/mnt/data/lego_recovery/restored/code.bin
cmake --build build-gcc --parallel 4
ctest --test-dir build-gcc --output-on-failure
NEW_ROOT="$(mktemp -d /mnt/data/lego_recovery/attributes-next.XXXXXX)"
mkdir -p "$NEW_ROOT/00048000/F000000B/user"
./build-gcc/LEGOChaseNative restored/code.bin --shared-extdata-root "$NEW_ROOT" --ptm-step-mode empty --romfs game/prepared-romfs/romfs.bin --gpu-vram-mode reference-zero --display-clock-mode reference-idle --cfg-profile reference-stereo --cfg-sound-mode stereo --cfg-language en --cpu-mode diagnostic-dual --block-limit 100000000 --exheader prepared-launch/exheader.bin --dsp-special-profile empty-config --dsp-executor live-teakra --dsp-reset-profile reference-zero-data --dsp-transmit-profile reference-stereo --dsp-audio-mode capture --dsp-boot-mode reference-slice --dsp-audio-file "$NEW_ROOT/audio.dspaud"
```

Expected exit3, pending attributes at round843. Clang uses clang++. Sanitizers omit
BOTH AOT/supplement options, Debug-O1 with address/undefined/frame-pointer compiler
flags and matching linker flags, ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 and
UBSAN_OPTIONS=halt_on_error=1. Exact commands: final-*-steps.json. Omitting supplement
input retains earlier MissingBlock; do not call it a new regression. Generated
supplement C++ remains PRIVATE and regenerates from authenticated code.bin at build.

Evidence: cache-checkpoint/tests.json,configured-*.json,junit-final-*.xml,
final-*-test.log; matrix-results.json,pair-*/verified.json; capture-final/{gcc,clang}/;
proof.json/verify_results.py; identity.json/verify_inputs.py; make_capture.py and
trace_ipc.cpp/trace_vfp.cpp/capture_main.cpp; references.json/setup-notes.txt.
run_job.py collects bounded build/test stages; all .done receipts are0. Capture
builders use current production sources. validate_pair.py requires NEW output names;
unknown saves/captures must never be overwritten. Only the driver's own just-created,
byte/hash-checked gamecoin/audio files are removed to pair compilers on the same path.
cache-owned.* and cache-paired.* are TEST state, not recovered console NAND.

## Durable recovery and every-turn mandate

GitHub GTTeancum/Lego-Undercover-3DS-Recomp; Library /LEGO-Chase-Recovery/.
New archive contains complete indexed source/current PRIVATE evidence, prepared
launch, parts manifest, source/member index, verifier, exact parent patch and
actual receipts. Verify CHECKPOINT-MANIFEST.json and SOURCE-INDEX.json paths/modes/
Git blobs, including tracked ignored reports. Independently restore tree/patch;
never invent upstream ancestry. Historical archives remain separate, not recursively
embedded. The downloadable appended receipt gives actual commit and saved locations.

Large originals have existing separate Library backups: code.bin;
LEGO-Chase-current-AOT-599pages-2026-10-03.tgz (unpacks generated2/);
Prepared-RomFS/two parts+romfs-parts.json; Prepared-Launch/exheader.bin.
Parts402653184+366526464 restore via repo/tools/restore_romfs_parts.py into a NEW
file; raw769179648 bytes, native offset4096/size769175552. Preserve integrity tables.
No CCI extraction or new game upload is required. Original SHA256 identities:
code5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f;
AOT2dd483e571bdb8f83a2ec7f60374f7370e9e57e39351c06e77ea8de170f121a9;
RomFS6e767bd3b308a72dae8d45ccd830f21306e79f6b19539b38da500e3779b709cf;
ExHeaderd7641f0a3bb89697ca8f0751e88d31703b54aa185ac78f9e74e41169dfcdc004.

Raw game instruction/VFP/PICA/DSP/SRAM/audio/ExHeader captures NEVER enter public
GitHub. Code.bin, private AOT/supplement C++, rawRomFS/CCI, .git and compiled binaries
are excluded from the source/evidence archive. Scratch can reset; artifacts/Library/
GitHub are recovery routes, not permanent scratch. Append only observed publication,
CI and persistence receipts. POST UPDATED DOWNLOADABLE MARKDOWN AND COMPLETE
RECOVERABLE SOURCE/EVIDENCE AT THE END OF EVERY WORK TURN.
