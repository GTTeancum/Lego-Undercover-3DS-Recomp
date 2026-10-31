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


if __name__ == "__main__":
    unittest.main()
