"""ROM-free, independently authored page-boundary generation fixtures."""
import struct
import unittest
from test_generate_aot_supplement import mod, shifted, range_manifest

def crossing_manifest(ranges):
    result = range_manifest(ranges)
    for block in result['blocks']:
        block['page_fallthrough'] = True
    return result

class PageFallthroughGenerationTests(unittest.TestCase):
    def render_at(self, offset, descriptions):
        base = mod.frontend.BASE
        code = bytes(offset) + b"".join(struct.pack("<I", raw) for raw, _, _ in descriptions)
        inst = {base + offset + i * 4: item for i, item in enumerate(descriptions)}
        ranges = mod.checked_ranges(crossing_manifest([(base + offset, base + len(code))]))
        return mod.render(code, inst, ranges)

    def test_cross_page_manifest_requires_explicit_boolean_opt_in(self):
        base = mod.frontend.BASE
        ranges = [(base + 0xFFC, base + 0x1004)]
        with self.assertRaises(ValueError):
            mod.checked_ranges(range_manifest(ranges))
        for value in (False, 1, 0, "true", None):
            document = range_manifest(ranges)
            document['blocks'][0]['page_fallthrough'] = value
            with self.subTest(value=value):
                with self.assertRaises(ValueError):
                    mod.checked_ranges(document)
        self.assertEqual(mod.checked_ranges(crossing_manifest(ranges)), ranges)

    def test_crossings_keep_the_original_size_and_text_bounds(self):
        base = mod.frontend.BASE
        for offset, size in ((0xFFC, 8), (0xFFC, 1024), (0xC04, 1024), (0xF80, 600)):
            with self.subTest(offset=offset, size=size):
                span = [(base + offset, base + offset + size)]
                self.assertEqual(mod.checked_ranges(crossing_manifest(span)), span)
        for span in ([(base + 0xFFC, base + 0xFFC + 1028)],
                     [(base + 0xFFC, base + 0x2004)],
                     [(base + mod.frontend.TEXT_BYTES - 4, base + mod.frontend.TEXT_BYTES + 4)]):
            with self.subTest(span=span):
                with self.assertRaises(ValueError):
                    mod.checked_ranges(crossing_manifest(span))

    def test_each_original_word_appears_once_in_page_local_segments(self):
        body = (shifted(0, rn=8, rd=7, rm=6, shift=1, amount=5),
                "and", "r7, r8, r6, lsr #5")
        for offset, count in ((0xFFC, 2), (0xFFC, 256), (0xF80, 150), (0xC04, 256)):
            with self.subTest(offset=offset, count=count):
                descriptions = [body] * (count - 1) + [(0xE12FFF1E, "bx", "lr")]
                source, receipt = self.render_at(offset, descriptions)
                blocks = receipt["blocks"]
                self.assertEqual(len(blocks), 2)
                self.assertEqual(blocks[0]["entry"], mod.frontend.BASE + offset)
                self.assertEqual(blocks[0]["end"], mod.frontend.BASE + 0x1000)
                self.assertEqual(blocks[1]["entry"], blocks[0]["end"])
                self.assertEqual(blocks[-1]["end"], mod.frontend.BASE + offset + count * 4)
                rows = [row for b in blocks for row in b["instructions"]]
                self.assertEqual([row["raw"] for row in rows], [raw for raw, _, _ in descriptions])
                self.assertEqual([row["pc"] for row in rows],
                                 list(range(mod.frontend.BASE + offset, blocks[-1]["end"], 4)))
                for b in blocks:
                    self.assertEqual(b["entry"] // 4096, (b["end"] - 1) // 4096)
                    self.assertEqual(len(b["instructions"]) * 4, b["end"] - b["entry"])
                self.assertEqual(sum(row["opcode"] == "BranchReg" for row in rows), 1)
                self.assertEqual(source.count("a32::EncodeMetadata("), count)
                # A page split records provenance, not a fabricated branch.
                self.assertEqual(receipt["page_splits"], [{"entry": mod.frontend.BASE + offset,
                                 "end": mod.frontend.BASE + offset + count * 4,
                                 "fallthrough_at": mod.frontend.BASE + 0x1000}])

    def test_page_aligned_end_and_start_do_not_create_empty_segments(self):
        body = (0xE3A02005, "mov", "r2, #5")
        for offset, descriptions in ((0xFF8, [body, (0xE12FFF1E, "bx", "lr")]),
                                     (0x1000, [(0xE12FFF1E, "bx", "lr")])):
            with self.subTest(offset=offset):
                _, receipt = self.render_at(offset, descriptions)
                self.assertEqual(len(receipt["blocks"]), 1)
                self.assertNotIn("page_splits", receipt)

    def test_boundary_is_not_a_substitute_for_a_real_terminal(self):
        body = (0xE3A02005, "mov", "r2, #5")
        with self.assertRaisesRegex(ValueError, "must end at a decoded branch/return"):
            self.render_at(0xFF8, [body] * 4)
        for index in (0, 1, 2):
            descriptions = [body] * 4 + [(0xE12FFF1E, "bx", "lr")]
            descriptions[index] = (0x1A000000, "bne", "#0")
            with self.subTest(index=index):
                with self.assertRaisesRegex(ValueError, "internal control-flow exit"):
                    self.render_at(0xFF8, descriptions)

    def test_tail_remains_bound_to_decoding_and_original_input(self):
        base = mod.frontend.BASE
        first = (0xE3A02005, "mov", "r2, #5")
        last = (0xE12FFF1E, "bx", "lr")
        for tail in ((shifted(1, flags=True), "eor", "r2, r1, r3, lsl #11"),
                     (shifted(0), "<unknown>", "")):
            with self.subTest(tail=tail):
                with self.assertRaisesRegex(ValueError, "unverified supplemental opcode class"):
                    self.render_at(0xFFC, [first, tail, last])
        code = bytes(0xFFC) + struct.pack("<II", first[0], last[0])
        for absent in (True, False):
            inst = {base + 0xFFC: first}
            if not absent:
                inst[base + 0x1000] = (0xE12FFF1D, "bx", "sp")
            with self.subTest(absent=absent):
                with self.assertRaisesRegex(ValueError, "missing LLVM instruction|disassembly/input disagreement"):
                    mod.render(code, inst, [(base + 0xFFC, base + 0x1004)])



class RecordedJoinGenerationTests(unittest.TestCase):
    """Synthetic recorded pages and prefixes; no original-game fixture bytes."""
    def setUp(self):
        import tempfile
        from pathlib import Path
        self.tmp = tempfile.TemporaryDirectory(prefix='lego-authored-base-join-')
        self.addCleanup(self.tmp.cleanup)
        self.directory = Path(self.tmp.name)
        self.page = mod.frontend.BASE + 0x2000
        self.entry = self.page + 0x100
        self.end = self.entry + 8
        self.code = bytearray(0x3000)
        self.prefix = [(0xE3A06009, 'mov', 'r6, #9'),
                       (0xE2866001, 'add', 'r6, r6, #1')]
        self.tail = [0xE3A02007, 0xE12FFF1E]
        self.inst = {self.entry + i*4: row for i, row in enumerate(self.prefix)}
        for pc, row in self.inst.items():
            struct.pack_into('<I', self.code, pc-mod.frontend.BASE, row[0])
        for i, raw in enumerate(self.tail):
            struct.pack_into('<I', self.code, self.end-mod.frontend.BASE+i*4, raw)
        self.write_page([(self.end, self.tail)])

    def write_page(self, blocks, *, suffix=''):
        import hashlib
        symbol = f'kLegoPage{self.page >> 12:05X}'
        lines = ['#include "lego_aot_generated.h"', '', 'namespace oot3d::recomp {', '',
                 f'static constexpr a32::PackedOp {symbol}Ops[] = {{']
        for _, words in blocks:
            for raw in words:
                # These synthetic base opcodes do not execute in generator tests.
                lines.append(f'    {{0x{raw:08X}U, a32::EncodeMetadata(a32::Opcode::MovImm, a32::Condition::Al, 0x02U)}},')
        lines += ['};', '', f'static const a32::Block {symbol}Blocks[] = {{']
        offset = 0
        for start, words in blocks:
            lines.append(f'    {{0x{start:08X}U, {symbol}Ops + {offset}U, {len(words)}U}},')
            offset += len(words)
        lines += ['};', '', f'extern const a32::BlockShard {symbol}Shard = {{0x{self.page:08X}U, 0x{self.page+4092:08X}U, {symbol}Blocks, {len(blocks)}U}};', '', '}']
        self.page_text = '\n'.join(lines) + '\n' + suffix
        self.path = self.directory / mod.frontend.page_name(self.page)
        self.path.write_text(self.page_text)
        self.digest = hashlib.sha256(self.path.read_bytes()).hexdigest()

    def document(self):
        result = range_manifest([(self.entry, self.end)])
        result['blocks'][0]['recorded_fallthrough'] = {'page_sha256': self.digest}
        return result

    def authenticate(self, document=None, directory=None):
        return mod.authenticate_recorded_fallthroughs(
            bytes(self.code), self.document() if document is None else document,
            self.directory if directory is None else directory)

    def render(self, inst=None):
        joins = self.authenticate()
        return mod.render(bytes(self.code), self.inst if inst is None else inst,
                          [(self.entry, self.end)], recorded_fallthroughs=joins)

    def test_explicit_pinned_exact_original_entry_emits_only_missing_prefix(self):
        joins = self.authenticate()
        self.assertEqual(joins, {self.entry: self.end})
        source, receipt = self.render()
        self.assertEqual(source.count('a32::EncodeMetadata('), 2)
        self.assertEqual([r['raw'] for r in receipt['blocks'][0]['instructions']],
                         [r[0] for r in self.prefix])
        self.assertNotIn('0xE3A02007U', source)
        self.assertNotIn('a32::Opcode::Branch', source)
        self.assertEqual(receipt['recorded_fallthroughs'],
                         [{'entry': self.entry, 'end': self.end, 'target': self.end}])

    def test_unmarked_prefix_keeps_the_real_terminal_requirement(self):
        with self.assertRaisesRegex(ValueError, 'must end at a decoded branch/return'):
            mod.render(bytes(self.code), self.inst, [(self.entry, self.end)])
        plain = range_manifest([(self.entry, self.end)])
        self.assertEqual(mod.authenticate_recorded_fallthroughs(bytes(self.code), plain, None), {})

    def test_join_requires_private_base_directory(self):
        with self.assertRaisesRegex(ValueError, 'base AOT directory'):
            mod.authenticate_recorded_fallthroughs(bytes(self.code), self.document(), None)

    def test_annotation_has_an_exact_shape_and_digest(self):
        for value in (True, False, None, {}, {'page_sha256': '0'*63},
                      {'page_sha256': 'g'*64}, {'page_sha256': self.digest, 'path': '../other'}):
            doc = self.document();doc['blocks'][0]['recorded_fallthrough'] = value
            with self.subTest(value=value):
                with self.assertRaisesRegex(ValueError, 'recorded_fallthrough'):
                    mod.checked_ranges(doc)

    def test_only_same_page_joins_are_admitted(self):
        for entry, end in ((self.page+4092, self.page+4096),
                           (self.page+4092, self.page+4100)):
            doc = self.document();doc['blocks'][0].update(entry=hex(entry), end=hex(end), page_fallthrough=True)
            with self.subTest(entry=entry, end=end):
                with self.assertRaisesRegex(ValueError, 'recorded fallthrough must stay within one page'):
                    mod.checked_ranges(doc)

    def test_pinned_file_hash_is_required(self):
        doc = self.document();self.path.write_text(self.page_text+'\n')
        with self.assertRaisesRegex(ValueError, 'base page hash'):
            self.authenticate(doc)

    def test_recorded_words_must_match_authenticated_input(self):
        self.code[self.end-mod.frontend.BASE] ^= 1
        with self.assertRaisesRegex(ValueError, 'base page/input disagreement'):
            self.authenticate()

    def test_end_must_be_exact_entry_not_interior_or_hole(self):
        for target in (self.end+4, self.end-4):
            self.code = bytearray(0x3000)
            for i, raw in enumerate(self.tail):
                struct.pack_into('<I', self.code, target-mod.frontend.BASE+i*4, raw)
            self.write_page([(target, self.tail)])
            with self.subTest(target=target):
                with self.assertRaisesRegex(ValueError, 'exact recorded block entry'):
                    self.authenticate()

    def test_prefix_must_not_overlap_any_original_block(self):
        self.write_page([(self.entry, [r[0] for r in self.prefix]), (self.end, self.tail)])
        with self.assertRaisesRegex(ValueError, 'overlaps recorded code'):
            self.authenticate()

    def test_base_declarations_are_not_loose_regex_hints(self):
        for altered in (self.page_text+'unexpected_token\n',
                        self.page_text.replace('Blocks, 1U', 'Blocks, 2U'),
                        self.page_text.replace('Ops + 0U', 'Ops + 99U'),
                        self.page_text.replace('2U},', '0U},')):
            import hashlib
            self.path.write_text(altered);self.digest=hashlib.sha256(self.path.read_bytes()).hexdigest()
            with self.subTest(altered=altered):
                with self.assertRaises(ValueError):
                    self.authenticate()

    def test_annotation_does_not_hide_control_exits_or_unsupported_ops(self):
        for index, description, message in (
                (0, (0x1A000000, 'bne', '#0'), 'internal control-flow exit'),
                (1, (0xE12FFF1E, 'bx', 'lr'), 'recorded fallthrough cannot end in a control exit'),
                (1, (shifted(1), 'eor', 'r2, r1, r3, lsl #11'), 'unverified supplemental opcode class')):
            inst=dict(self.inst);inst[self.entry+4*index]=description
            struct.pack_into('<I',self.code,self.entry-mod.frontend.BASE+4*index,description[0])
            with self.subTest(index=index, description=description):
                with self.assertRaisesRegex(ValueError,message):
                    self.render(inst)
            for i,row in enumerate(self.prefix):
                struct.pack_into('<I',self.code,self.entry-mod.frontend.BASE+i*4,row[0])

    def test_unknown_join_key_or_wrong_end_is_rejected(self):
        for joins in ({self.entry+4:self.end}, {self.entry:self.end+4}):
            with self.subTest(joins=joins):
                with self.assertRaisesRegex(ValueError, 'recorded fallthrough range'):
                    mod.render(bytes(self.code), self.inst, [(self.entry,self.end)],
                               recorded_fallthroughs=joins)

    def test_symlinked_page_is_rejected(self):
        original=self.directory/'other.cpp';self.path.rename(original)
        self.path.symlink_to(original)
        with self.assertRaisesRegex(ValueError, 'regular base page'):
            self.authenticate()

    def test_legacy_generation_is_unchanged_when_no_joins_are_declared(self):
        code=struct.pack('<II',0xE3A02005,0xE12FFF1E)
        base=mod.frontend.BASE
        inst={base:(0xE3A02005,'mov','r2, #5'),base+4:(0xE12FFF1E,'bx','lr')}
        before=mod.render(code,inst,[(base,base+8)])
        after=mod.render(code,inst,[(base,base+8)],recorded_fallthroughs={})
        self.assertEqual(before,after)
        self.assertNotIn('recorded_fallthroughs',before[1])


if __name__ == "__main__":
    unittest.main()
