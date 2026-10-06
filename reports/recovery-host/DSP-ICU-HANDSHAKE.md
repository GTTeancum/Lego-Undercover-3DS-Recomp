# DSP interrupt controller and original-firmware startup handshake

October 6, 2026. Tested local checkpoint over main
`50b2942120f47d221cf66e662961165cf031bd20`. **Not published in this turn.**

## Actual result

With the explicit `guarded-teakra`, `empty-config`, and `reference-zero-data`
options, the original 49,716-byte firmware now completes the four-word startup
protocol in the isolated host probe. All words were produced by executed firmware:

| Receive register | Value | Completed Run(1) calls |
|---|---:|---:|
| 0 | 1 | 3,212 |
| 1 | 1 | 3,261 |
| 2 | 1 | 3,314 |
| 2 | 3,230 (`0x0C9E`) | 3,796 |

The fourth word is a distinct mailbox receive, not reuse of the ready word. The
firmware performs 1,374 word writes, touching 1,250 distinct SRAM bytes; 421 byte
values differ from the staged/reset image. Its final PC is `0x5AF8`. These are
completed interpreter calls and observed writes, not measured DSP cycle timings.

An independent parser locates the descriptor table from the actual received word.
All 160 descriptor bytes were written by firmware. It contains 16 slots, each
with capacity 128 bytes and zero read/write pointers; slot identities and ranges
validate against the captured image. This establishes an initialized descriptor
snapshot, **not functioning live pipe I/O or valid audio payloads**.

Known-only execution still stops after 962 completed calls on unknown SRAM byte
`0x40AC0`, with no replies. No unknown memory was silently made readable to obtain
the new result. The reference-data reset and 532-byte empty-configuration fallback
remain separate explicit policies, not recovered hardware state.

## What changed

The ICU now models logical input polarity, edge/level acknowledgement, manual
trigger propagation, pending bits, routing, master-disable and vector state.
Pending notifications route through the interpreter's existing core entry path.
Reset clears both ICU state and queued interpreter notifications. Vector readback
masks reserved bits; unimplemented registers still throw. Simultaneous or separately
pending vectors that would overwrite the pinned core's single vector slot stop
explicitly instead of silently losing a notification.

The original firmware performs 153 successful MMIO accesses in the reference run.
It changes mode/polarity through `0x6000`, `0x6400`, and `0x6600`. It does not write
manual-trigger or master-disable in this captured startup path; those behaviors
are component-tested, not represented as newly observed game operations.

Logical register behavior is based on Martin Korth's **GBATEK, no$gba 3.03**,
DSi Teak ICU section, printed pages 363–364, plus the register summary on page345:
https://pcy.be/tmp/img/gbatek.pdf . This is hardware-research documentation, not
our own 3DS measurement. The two-stage manual propagation is an interpreter-tick
approximation. Existing peripheral callbacks remain explicit high/low pulses;
held physical lines are not generally wired. Unconnected SIO is held inactive;
the other initial raw inputs are low. Pending flags start quiet, not from a claimed
console power-up dump. Full level-line/core arbitration and hardware timing remain
unproven. The changes are confined to the guarded diagnostic backend.

## The game still does not complete LoadComponent

Normal original startup still stops at ARM `0x0025947C`, round252, with the
untouched request `0x001100C2`. The diagnostic now says:

```
DSP probe handshake complete; live execution and pipes still unimplemented
```

No successful guest reply, loaded-device flag, live DSP schedule, ARM-to-DSP memory
mapping, pipe transaction, audio sample, input sample or GPU completion is supplied.
The service remains unchanged and deliberately refuses to equate a completed probe
with a working device. The original second-core worker still has zero executed
instructions at this stop. VRAM remains blank; **no logo, title screen, main menu,
rendered frame or gameplay is demonstrated**.

## Verification

Full GCC and Clang builds link all 599 unchanged private AOT units. All **54**
CTest suites pass under each compiler, and all54 ROM-free suites pass under Clang
ASan/UBSan with leak checks and halt-on-error. The added ICU suite covers both modes
for every manual bit, polarity ordering, masking, routing, vector/reset behavior,
core entry through real synthetic DSP instructions and pending-notification reset.
An inherited unsupported-MMIO regression was moved from newly implemented `0x20E`
to still-unimplemented `0x254`; its fail-closed assertion remains.

Four ordinary original-startup cases match stdout, exit and newly created test
file bytes across compilers. Both final probe modes have 84 matching capture files
per compiler. IPC/input and captured ARM/thread/time, GPU/upload, GSP/HID page/epoch,
LCD and full6MiB VRAM fields stay unchanged across the call. Standalone runs,
logging-only MMIO runs and one-step versus large bounded slices produce identical
DSP SRAM, provenance and reply summaries. This is not an independent CPU oracle
or a complete dump of the ARM address space.

The code, entire raw RomFS, original ExHeader and all603 AOT archive members match
verified inputs. No full IVFC recheck, Windows/macOS build or new hosted CI ran.
Initial Clang test-namespace compile failures and an interrupted combined capture
batch are retained separately; final corrected tests/captures are used above.

## Next work

Create a live, bounded DSP device from this verified execution state, preserving
actual mailbox/descriptor contents rather than replacing them with constants.
Connect memory provenance, ARM-visible mapping, mailbox/pipe operations, scheduled
execution and lifecycle/fault handling before acknowledging the guest load.
The existing probe is terminal at ProtocolComplete and is not that device.

Private evidence and reproducible drivers: `dsp-icu-checkpoint/`. Pipe layout
reference: Azahar `86a9f9236ae42bb5a2b995dbc933d599d8ea07ac`,
`src/audio_core/lle/lle.cpp`, blob `388fe64ec1a5130a2c93a5dfa04ca84df109b567`.
MIT interpreter pin and local changes remain recorded in vendor/teakra-3d697a1/UPSTREAM.json.
