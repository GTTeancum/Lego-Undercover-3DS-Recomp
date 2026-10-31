"""Strict reader for a pinned original AOT page used by an offline join review.

This module never emits or executes an operation. It accepts the recovered page
layout only, binds each recorded word back to the authenticated input and returns
original block intervals. Unknown layouts are errors, not partial coverage.
"""
from __future__ import annotations

import re
import struct


def recorded_page_ranges(code: bytes, text: str, page: int, *,
                         text_base: int, text_bytes: int) -> list[tuple[int, int]]:
    """Read one complete original page, rejecting loose/partial declarations."""
    if page & 4095 or page < text_base or page >= text_base + text_bytes:
        raise ValueError('invalid recorded page address')
    symbol = f'kLegoPage{page >> 12:05X}'
    pattern = (
        r'\s*#include\s+"lego_aot_generated\.h"\s+'
        r'namespace\s+oot3d::recomp\s*\{\s*'
        rf'static\s+constexpr\s+a32::PackedOp\s+{symbol}Ops\[\]\s*=\s*\{{(?P<ops>.*?)\}};\s*'
        rf'static\s+const\s+a32::Block\s+{symbol}Blocks\[\]\s*=\s*\{{(?P<blocks>.*?)\}};\s*'
        rf'extern\s+const\s+a32::BlockShard\s+{symbol}Shard\s*=\s*\{{\s*'
        rf'0x(?P<first>[0-9A-Fa-f]+)U,\s*0x(?P<last>[0-9A-Fa-f]+)U,\s*'
        rf'{symbol}Blocks,\s*(?P<count>\d+)U\s*\}};\s*\}}\s*'
    )
    match = re.fullmatch(pattern, text, re.S)
    if not match:
        raise ValueError('unrecognized complete recorded base page layout')
    if int(match['first'], 16) != page or int(match['last'], 16) != page + 4092:
        raise ValueError('recorded base shard geometry mismatch')
    op_pattern = (
        r'\{0x([0-9A-Fa-f]{8})U,\s*a32::EncodeMetadata\('
        r'a32::Opcode::\w+,\s*a32::Condition::\w+,\s*0x[0-9A-Fa-f]{2}U\)\},'
    )
    words = []
    for line in match['ops'].splitlines():
        if not line.strip():
            continue
        row = re.fullmatch(op_pattern, line.strip())
        if not row:
            raise ValueError('unrecognized recorded base operation')
        words.append(int(row[1], 16))
    if not 1 <= len(words) <= 1024:
        raise ValueError('recorded base operation count outside page bound')
    block_pattern = (
        rf'\{{0x([0-9A-Fa-f]{{8}})U,\s*{symbol}Ops\s*\+\s*(\d+)U,\s*'
        r'(\d+)U(?:,\s*false)?\},'
    )
    ranges = []
    cursor = 0
    prior_end = page
    for line in match['blocks'].splitlines():
        if not line.strip():
            continue
        row = re.fullmatch(block_pattern, line.strip())
        if not row:
            raise ValueError('unrecognized recorded base block')
        start, offset, count = int(row[1], 16), int(row[2]), int(row[3])
        end = start + 4 * count
        if (start & 3 or count == 0 or offset != cursor or
                offset + count > len(words) or start < prior_end or
                not page <= start < end <= page + 4096 or
                end > text_base + text_bytes or end - text_base > len(code)):
            raise ValueError('invalid recorded base block extent or operation slice')
        for i, word in enumerate(words[offset:offset+count]):
            if struct.unpack_from('<I', code, start-text_base+4*i)[0] != word:
                raise ValueError('base page/input disagreement')
        ranges.append((start, end))
        cursor += count
        prior_end = end
    if cursor != len(words) or len(ranges) != int(match['count']) or not ranges:
        raise ValueError('recorded base page count mismatch')
    return ranges
