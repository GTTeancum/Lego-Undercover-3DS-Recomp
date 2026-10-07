# DSP boot polling, original startup-message consumption and reply

October 6, 2026 (user-local date). Based on published `4be383cba5c8b370e5ca45150a098618203ac410`,
exact tree `331f4e9a429b3ec7314a8000d89d94d67869204b`. Publication is recorded in the final receipt.

## Actual result

With explicit `--dsp-boot-mode reference-slice`, the original DSP consumes the
four-byte audio startup message, writes a 32-byte reply into its own output pipe,
and notifies the real registered event. The waiting original main thread wakes and
requests ReadPipeIfPossible (0x001000C0), channel2, peer0, length2. That new request
and its output remain untouched. No pipe-read response has been implemented here.

The first scheduled slice advances slot5's firmware-owned read pointer from0 to4,
matching its existing write pointer4. The second writes32 bytes into slot4, advances
its write pointer0->32, and emits actual peripheral notifications. Captures show
thread1 changing WaitSynchAny->Ready/pending_wake with its exact retained EventObject.
By the next IPC it is Running again. The first reply word is15; the following data
has not yet been interpreted as a completed application audio initialization.

Final ordinary run: ARM round261, PC0x0025947C, time18630676ns,
core0 instructions2287464, core1 instructions0, two completed scheduled DSP slices.
The DSP has completed65536 Run(1) calls (32768 during synchronous boot), not measured
hardware cycles. Ten captured stereo pairs are all genuine FIFO words and all zero;
two occur during boot. There are NO tagged underrun replacements on this path.
No audible music/playback, HID sampling, rendered frame, logo, title screen, menu or
gameplay is demonstrated. Complete device VRAM remains zero.

## Diagnosis and bounded fix

The previous loader polled after every Run(1), consuming the fourth boot word after
3796 calls and exposing the device to the next ARM message immediately. Logging-only
original firmware traces show the mailbox2 command reaching its dispatcher while
the per-pipe callback is still null: PC0x4E1D takes the null branch to0x4E2D. Later
firmware setup installs callbacks through0x4E34. The earlier notification has already
been consumed; the actual payload stays queued. An isolated later-delivery experiment
changed this path, but its artificial delay was NOT installed in production.

Pinned Azahar instead executes a complete16384-call RunTeakraSlice whenever a
mailbox word is unavailable, then polls again. The new explicit mode preserves that
cadence and carries incomplete slice state across bounded Advance calls. It does not
insert a title-specific delay, rewrite a task pointer, inject an interrupt or return
hardcoded replies. Original ready replies0:1,1:1,2:1 are consumed at16384 calls;
the distinct reg2 pipe-base word0x0C9E arrives at the next poll at32768. Immediate mode
remains the default diagnostic comparison. Both modes continue to reject unknown SRAM.

The now-reached firmware path also writes transmit control0x010F. GBATEK identifies
bits8..11 as the interrupt-enable nibble (zero disabled; nonzero enabled with uncertain
alternate encodings). Only the observed0x000F/0x010F pair plus disabled reset5 is
supported. FIFO-empty notification is now gated by0x010F, not unconditional. A control
write or empty fallback does not generate an IRQ. The FIFO-empty threshold, external
4096-call output period, channel ordering and held-line timing remain reference
assumptions, not new hardware measurements or full format support.

## Verification

All61 CTest suites pass in full GCC and Clang native builds (599 unchanged private
AOT pages), and all61 ROM-free Clang ASan/UBSan suites pass with leak checks/halt-on-error.
Configured/JUnit names match without skips or duplicates. The new boot-slice suite
uses synthetic executing DSP programs for exact poll boundaries, partial budgets,
chunk equivalence, separate reg2 words, post-reply faults, actual boot-time output,
missing consumers and mode guards. The existing BTDMP suite retains all65536 sample
values and now covers all control words plus disabled/enabled IRQ transitions.

Six ordinary startup compiler pairs match stdout, stderr, exit and owned gamecoin
bytes: reference-slice, immediate, known-only, bounded100, missing consumer and guarded
probe. Six pre-input CLI rejection pairs match. All333 corresponding final capture
files match byte-for-byte across compilers. Independent Python parses live descriptors,
firmware provenance, event identity/wake, mapping coherence and unchanged next IPC.
All captured GPU registers/uploads, GSP page/epochs, mapped HID, LCD and VRAM remain
unchanged across the new DSP work. This is not an independent CPU oracle or full ARM
address-space dump. Original code, raw RomFS, ExHeader and all603 AOT archive members
retain their identities. Full IVFC rechecking was not repeated. No Windows/macOS build.

The first changed-source test run failed the old test's expectation that0x010F was
unsupported and0x000F always caused IRQ. Those assertions were replaced by explicit
corrected-contract regression tests, not deleted without replacement. Initial build
interruptions and exploratory trace binaries/logs are separately recorded; only final
completed tests and original runs are counted. No previous suite was removed.

## Scope and next work

Only boot polling policy, its explicit CLI/runner wiring, boot-time explicit capture,
bounded transmit IRQ gating, tests and provenance documentation change. The service's
pipe read/write implementation, scheduler, CPU/DSP opcode algorithms, original game
bytes, AOT, renderer and input producer are unchanged. The new mode still uses the
existing live boot/table validation/mapping before acknowledging LoadComponent.

Next implement the observed two-byte ReadPipeIfPossible request against the actual
firmware-created output pipe. Validate the static reply-buffer contract, permissions,
aliases and output preflight before consuming bytes. Do not pre-seed the reply, force
pointer movement, or expand unrelated audio features before observing the next call.

References: Azahar86a9f9236ae42bb5a2b995dbc933d599d8ea07ac,
src/audio_core/lle/lle.cpp blob388fe64ec1a5130a2c93a5dfa04ca84df109b567;
Martin Korth GBATEK/no$gba3.03 BTDMP printed365-367, PDF390-392 zero-based,
https://pcy.be/tmp/img/gbatek.pdf. Parsed text was available; screenshot attempts
failed. No page-image validation or new3DS hardware measurement is claimed.
Private evidence: dsp-dispatch-checkpoint/. All original captures stay out of GitHub.
