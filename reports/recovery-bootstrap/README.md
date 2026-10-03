# Recovery bootstrap checkpoint

Recovered/recreated on 2026-10-03 from the user-supplied six-part CCI archive.

## Verified

- Extracted CCI SHA-256: \`3ae683620ada99a6ec80e90db70dd5a18f7c761e6d40d4a7befb82ec83d90525\`
- Title ID: \`00040000000AD500\`
- Product code: \`CTR-P-AA8E\`
- Process: \`LEGOCITY\`
- Main NCCH starts at \`0x4000\`
- ExeFS \`.code\` compressed size: 1,701,108 bytes
- Decompressed code size: 2,650,112 bytes
- Decompressed code SHA-256: \`5b14d798bd510957b98fae753c128fac25b683f78203170f5297274a1894132f\`

The CCI hash and decompressed-code hash exactly match the surviving historical inspection record.

\`tools/prepare_game.py\` was executed against the uploaded CCI and Python byte-compilation passed. The derived \`code.bin\` is intentionally not committed.
