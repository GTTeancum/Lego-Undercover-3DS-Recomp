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
