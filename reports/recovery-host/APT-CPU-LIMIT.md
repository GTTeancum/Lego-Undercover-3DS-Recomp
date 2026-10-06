# Application CPU resource update and observed ControlMemory boundary

Implementation `3e9ecc1617fe4c472b4aeb306c292283add67a40`, tested/uploaded tree
`2dbb59ed4538fb020c9cbbb82fbecc1ad63e8f97`, base `38f0bbe`.

The original APT SetAppCpuTimeLimit(1,30) now changes the kernel's actual application
CpuTime current value from 0 to 30. Existing SVC resource handles and the paired APT
getter read the same object. Maximum 80 is inherited, not newly verified from this
title's ExHeader. Values above the maximum return success without mutation, matching
pinned PM:APP. The original getter is not observed in-game; it is component-tested.

Core-1 enforcement is NOT implemented. All current game threads use core 0. Accepted
updates require that condition and activate explicit guards against later unsupported
multicore creation/execution. No unrelated priority change, fake CPU charge, preemption
timer or wake substitutes for the missing limiter. Full launch-policy reconstruction,
CPU-cycle timing and multicore scheduling remain open.

The original setter gets 004F0040/0. Original code then closes its session, releases
its mutex and creates two events before ControlMemory(Map):

```text
stop=UnsupportedSvc pc=0x0025c9f4 detail=0x00000001 thread=1 dispatch_rounds=243
r0=4 r1=0x0e000000 r2=0x08045000 r3=0x8000 r4=3
```

This requests a 32768-byte map from the existing RW source to an unmapped destination.
It remains unexecuted with source and CPU registers unchanged. The inherited non-Commit
path had returned InvalidCombination and led into a fatal-report wait; the SVC bridge
now stops missing operations explicitly rather than manufacturing a guest failure.
The earlier error-loop trace is retained separately, not claimed as gameplay progress.

Full GCC and Clang builds link all 599 private AOT pages. All 40 CTest suites pass
with both compilers and with Clang ASan/UBSan (ROM-free, leak checking/halt-on-error).
Twenty startup cases match stdout, exit and test-file bytes. All 86 paired diagnostic
capture files match; the resource change is real, while 1842 GPU words, upload state,
GSP page/epochs and the full 6 MiB bank remain unchanged. Map source SHA is
c35020473aed1b4642cd726cad727b63fff2824ad68cedd7ffb73c7cbd890479.

Code, raw RomFS and all 603 AOT archive members match their backups. Full IVFC block
verification was not repeated. No previous test was removed or changed. Early setup
and new-test compile failures are preserved in setup-notes and incomplete logs; final
results above are from corrected, completed runs. No Windows/macOS build was performed.

GitHub Actions 37451651219 passed both GCC and Clang jobs on implementation 3e9ecc1.
Hosted tests are ROM-free, separate from the full local original-game runs.

Private evidence: apt-limit-checkpoint/. Public summaries: CPU-LIMIT-PROOF.json,
CPU-LIMIT-VALIDATION.json and CPU-LIMIT-*-GCC.txt. See the canonical handoff for exact
rebuild, trace and recovery paths. No useful screenshot or rendered frame exists.
Next inspect actual ControlMemory/process-map backing, permissions and alias semantics;
do not replace the map with an allocation, byte copy, guessed error or success stub.

Reference pin: azahar-emu/azahar @ 86a9f9236ae42bb5a2b995dbc933d599d8ea07ac.
APT apt.cpp calls PM:APP; pm_app.cpp updates Application ResourceLimit; kernel
resource_limit.cpp stores values; thread.cpp applies the limiter only on core 1.
Exact blob identities and inspected scopes are in the private references receipt.
