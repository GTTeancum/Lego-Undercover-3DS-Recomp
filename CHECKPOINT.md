# Latest LEGO Undercover 3DS checkpoint

Updated October 31, 2026 (Eastern). Resume from `main`.

## Plain-English status

Five new startup gaps are filled. A sixteen-row neighboring-byte processing loop and an eight-row copying loop now complete, their missing return steps are restored, and a setup step connects to existing code. All five fresh runs passed their preceding stops. Startup reaches another missing helper; menus and gameplay remain unverified.

The eleven previously pending fixes were published first as `461a306`. This continuation starts from that exact execution source and adds 5 newly observed sections. Menus and gameplay remain unverified.

The final post-stop captures are black and remain private diagnostics. No fresh windowed run was performed. Speaker playback and Windows execution remain unverified.

## Verified state

Base: `461a306`. All 134 prior reviewed sections and all 599 original AOT pages / 603 files remain unchanged. Runtime, generator, tests and build configuration are unchanged in this continuation.

Totals: **139 reviewed sections / 140 page-local blocks / 2795 supplemental words**; **111,183 runtime blocks / 547,906 verified words**.

Final stop: **MissingBlock `0x00153b3c`**. Advance beyond the same-mode baseline: **13,742 executed instructions / 51,255 diagnostic ns**. These are diagnostic counts, not a gameplay percentage or performance measurement.

All 5 new builds passed **89/89 enabled tests once**, with zero failures, errors or skips. Exact tested natives were preserved privately before fresh finite runs. No run timed out or wrote stderr. State and saves match the preceding checkpoint. Its full audio capture remains an exact prefix, with 4 additional diagnostic records. These results do not establish playable gameplay.

## Next exact task

**`[0x153b3c, 0x153bc4)`**. Add the reviewed 34-instruction section. It saves two working registers, loads a mask and prepares the first source row once, then combines neighboring guest rows into pairs of four-word output rows. It advances both pointers and decrements the row count by two. The first control exit conditionally repeats at interior suffix 0x00153B5C, excluding the one-time setup. Existing handlers accept the interval. Future memory values, loop completion and fallthrough at 0x00153BC4 remain unobserved; do not add that successor or invent a return target. The private review in rounds/05/next-review/review.json is bound to the actual final stop and has no overlaps with original or supplemental code.

Preserve all 139 reviewed sections. Authenticate inputs, verify original words and prior sections, build/test, save the exact native, and use fresh owned storage for a finite run. No runtime decoding, forced returns, fabricated GPU completion, guessed targets or unobserved successor bodies.

## Permissions

Ordinary source/metadata publication to `main` and ongoing private builds/backups are authorized. Never force-push. Use ordinary chat/cloud tools, not Codex services, Dot or the user's computer. Keep original inputs, generated instructions, executables, captures, audio, saves and private recovery identifiers out of public Git. Keep summaries short and plain; omit black screenshots.

Details: [current recovery record](docs/recovery/CURRENT-REBUILD.md).
