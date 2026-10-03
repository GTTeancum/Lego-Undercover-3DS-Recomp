import importlib.util
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location(
    "generate_aot_pages_llvm", ROOT / "tools" / "generate_aot_pages_llvm.py"
)
mod = importlib.util.module_from_spec(spec)
assert spec.loader is not None
sys.modules[spec.name] = mod
spec.loader.exec_module(mod)


class LlvmRecoveryTests(unittest.TestCase):
    def test_direct_branch_classification(self):
        opcode, cond, flags, category = mod.classify(0xEA000000, "b")
        self.assertEqual((opcode, cond, flags, category), ("Branch", "Al", 0, "fast_path"))

    def test_link_branch_sets_link_flag(self):
        opcode, cond, flags, category = mod.classify(0xEB000000, "bl")
        self.assertEqual(opcode, "Branch")
        self.assertEqual(cond, "Al")
        self.assertEqual(flags & 4, 4)
        self.assertEqual(category, "fast_path")

    def test_simple_mov_immediate(self):
        opcode, cond, flags, category = mod.classify(0xE3A00001, "mov")
        self.assertEqual(opcode, "MovImm")
        self.assertEqual(cond, "Al")
        self.assertEqual(flags & 2, 2)
        self.assertEqual(category, "fast_path")

    def test_simple_word_load(self):
        opcode, cond, flags, category = mod.classify(0xE59F0000, "ldr")
        self.assertEqual((opcode, cond, category), ("Ldr32", "Al", "fast_path"))

    def test_simd_remains_explicitly_unsupported(self):
        opcode, _, _, category = mod.classify(0xF2200150, "vorr")
        self.assertEqual(opcode, "Unsupported")
        self.assertEqual(category, "unsupported_vfp_simd")

    def test_vmls_is_scalar_not_ls_condition(self):
        opcode, cond, _, category = mod.classify(0xEE419A60, "vmls.f32")
        self.assertEqual((opcode, cond, category), ("VfpScalar", "Al", "vfp_scalar"))

    def test_vldmia_is_supported_transport(self):
        opcode, cond, _, category = mod.classify(0xEC900A08, "vldmia")
        self.assertEqual((opcode, cond, category), ("VfpTransport", "Al", "vfp_transport"))

    def test_conditional_indirect_preserves_fallthrough(self):
        base = mod.BASE
        inst = {
            base: (0x012FFF1E, "bxeq", "lr"),
            base + 4: (0xE1A00000, "mov", "r0, r0"),
            base + 8: (0xE12FFF1E, "bx", "lr"),
        }
        flow = mod.walk(inst, [base])
        self.assertIn(base + 4, flow["seen"])

    def test_explicit_lr_linked_indirect_recovers_return_site(self):
        base = mod.BASE
        inst = {
            base: (0xE28FE000, "add", "lr, pc, #0"),
            base + 4: (0xE12FFF14, "bx", "r4"),
            base + 8: (0xE1A00000, "mov", "r0, r0"),
            base + 12: (0xE12FFF1E, "bx", "lr"),
        }
        flow = mod.walk(inst, [base])
        self.assertIn(base + 8, flow["seen"])
        self.assertIn(base + 8, flow["lr_returns"])

    def test_base_relative_switch_targets(self):
        base = mod.BASE
        site = base + 0x100
        table = base + 0x40
        target1 = base + 0x200
        target2 = base + 0x204
        code = bytearray(mod.TEXT_ALLOCATED_BYTES)
        code[table - base:table - base + 4] = (target1 - table).to_bytes(4, "little")
        code[table - base + 4:table - base + 8] = (target2 - table).to_bytes(4, "little")
        code[table - base + 8:table - base + 12] = (0xFFFFFFFF).to_bytes(4, "little")
        inst = {
            site - 12: (0xE24F60BC, "sub", "r6, pc, #188"),
            site - 8: (0xE7965105, "ldr", "r5, [r6, r5, lsl #2]"),
            site - 4: (0xE1A0E008, "mov", "lr, r8"),
            site: (0xE085F006, "add", "pc, r5, r6"),
            target1: (0xE1A00000, "mov", "r0, r0"),
            target2: (0xE1A00000, "mov", "r0, r0"),
        }
        reachable = {site - 12, site - 8, site - 4, site}
        targets, words, sites = mod.base_relative_switch_targets(inst, bytes(code), reachable)
        self.assertEqual(targets, {target1, target2})
        self.assertEqual(words, {table, table + 4})
        self.assertEqual(sites, [site])

    def test_inline_word_thunk_detection(self):
        base = mod.BASE
        inst = {
            base: (0xE49E4004, "ldr", "r4, [lr], #4"),
            base + 4: (0xE1A00000, "mov", "r0, r0"),
        }
        self.assertTrue(mod.inline_word_thunk(inst, base))
        self.assertFalse(mod.inline_word_thunk(inst, base + 4))

    def test_aligned_ascii_literals(self):
        code = bytearray(mod.TEXT_ALLOCATED_BYTES)
        text = b"Token_Test.tga\0"
        code[:len(text)] = text
        words = mod.aligned_ascii_literal_words(bytes(code))
        self.assertIn(mod.BASE, words)
        self.assertIn(mod.BASE + 4, words)
        self.assertIn(mod.BASE + 8, words)

    def test_absolute_switch_table_words(self):
        base = mod.BASE
        site = base + 0x20
        table = site + 8
        targets_expected = {base + 0x100, base + 0x104, base + 0x108}
        code = bytearray(mod.TEXT_ALLOCATED_BYTES)
        for index, target in enumerate(sorted(targets_expected)):
            code[table - base + index * 4:table - base + index * 4 + 4] = target.to_bytes(4, "little")
        inst = {
            site - 4: (0xE3500003, "cmp", "r0, #3"),
            site: (0x379FF100, "ldrlo", "pc, [pc, r0, lsl #2]"),
        }
        for target in targets_expected:
            inst[target] = (0xE1A00000, "mov", "r0, r0")
        words, targets, sites = mod.absolute_switch_table_words(
            inst, bytes(code), {site - 4, site}
        )
        self.assertEqual(words, {table, table + 4, table + 8})
        self.assertEqual(targets, targets_expected)
        self.assertEqual(sites, [site])

    def test_block_split_at_guest_page_boundary(self):
        base = mod.BASE
        inst = {
            base + 0xFFC: (0xE1A00000, "mov", "r0, r0"),
            base + 0x1000: (0xE1A00000, "mov", "r0, r0"),
        }
        blocks = mod.make_blocks(inst, set(inst), {base + 0xFFC})
        self.assertEqual(blocks, [[base + 0xFFC], [base + 0x1000]])


if __name__ == "__main__":
    unittest.main()
