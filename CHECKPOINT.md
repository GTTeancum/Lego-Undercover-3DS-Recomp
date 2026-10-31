# Latest LEGO Undercover 3DS checkpoint

Updated October 31, 2026 (Eastern). Read this first and resume from `main`.

## Plain-English status

Five more startup gaps are filled. The four-row neighboring-data helper and the sixteen-row processing loop now finish, and both missing return steps are restored. All five fresh startup runs passed their previous stopping points. Startup reaches another missing helper; menus and gameplay remain unverified.

This batch preserves all six earlier unpublished fixes and adds 5 newly tested sections. The final post-stop screen captures are black and remain private diagnostics. Menus, gameplay, speaker playback and Windows execution remain unverified. No fresh windowed run was performed.

## Verified state

Publication base: `e44b7c4`. This checkpoint includes all eleven previously unpublished sections from two tested batches. All 129 recovered sections and all 599 original AOT pages / 603 files are preserved. Runtime, generator, tests and build settings are unchanged in this batch; the previously tested 256-section limit is retained.

Totals: **134 reviewed sections / 135 page-local blocks / 2741 supplemental words**; **111,178 runtime blocks / 547,852 verified words**.

Final stop: **MissingBlock `0x00153e98`**. New measured advance: **5,925 executed instructions** beyond the recovered pending build; **11,615** beyond the last published checkpoint. These are not gameplay percentages.

All 5 fresh builds passed **89/89 enabled tests once**. Exact tested natives were saved privately before their fresh finite runs. No run timed out or wrote stderr. Complete captured audio, state and save contents match the recovered candidate byte-for-byte. These checks do not establish playable gameplay.

## Next exact task

Next reviewed interval: **`[0x153e98, 0x153f10)`**. Add the reviewed 30-instruction section. It loads a mask, adjusts the source pointer once, reads four guest words and a following halfword, combines shifted and masked neighboring values into four output words, advances both row pointers and decrements the count. Its first control exit conditionally repeats at interior suffix 0x00153EA0, excluding the mask load and one-time adjustment. Existing handlers accept the interval. Future memory values, loop completion and fallthrough at 0x00153F10 remain unobserved; do not add that successor or invent a return target. The private review in rounds/05/next-review/review.json is bound to the actual final stop and has no overlaps with original or supplemental code.

Preserve all 134 reviewed sections. Authenticate the private inputs, verify exact original words and existing sections, build/test, preserve the exact native, then use fresh owned storage for a finite run. No runtime decoding, forced returns, fabricated GPU completion, invented targets or unobserved successor bodies.

## Publication and permissions

The user requested publication before further development. The previously rejected synthetic test-file upload was retried through the same GitHub action and accepted unchanged. Its blob and the complete manifest match the verified pending archive exactly. This publication recovers the saved work; it does not claim new game runs. Fresh hosted results must be checked separately.

Ordinary source/metadata publication and ongoing private builds/backups are authorized. Never force-push. Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Keep original inputs, generated game code, binaries, captures, audio, saves and private recovery identifiers out of public Git. Keep summaries short and plain; do not post completely black captures.

Details: [current recovery record](docs/recovery/CURRENT-REBUILD.md).
