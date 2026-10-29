"""ROM-free supplement policy tests; every instruction is authored here.

The constants below encode synthetic architectural forms. No original game
words, instruction addresses or disassembly are used by these fixtures.
Runtime value/flag semantics are tested by ctr_supplemental_registry_test.
"""
import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
spec = importlib.util.spec_from_file_location(
    "generate_aot_supplement", ROOT / "tools" / "generate_aot_supplement.py"
)
mod = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(mod)


def shifted(opcode, *, condition=14, flags=False, rn=1, rd=2, rm=3,
            shift=0, amount=11):
    """Author one classic A32 data-processing register/immediate-shift word."""
    return ((condition << 28) | (opcode << 21) | (int(flags) << 20) |
            (rn << 16) | (rd << 12) | (amount << 7) | (shift << 5) | rm)


class AluAdmissionTests(unittest.TestCase):
    def test_supported_forms_keep_all_conditions_s_bits_and_shift_encodings(self):
        for opcode, name, rn in ((4, "add", 8), (12, "orr", 8), (13, "mov", 0)):
            for condition in range(15):
                for flags in (False, True):
                    for shift in range(4):
                        for amount in (0, 1, 11, 31):
                            with self.subTest(opcode=opcode, condition=condition,
                                              flags=flags, shift=shift, amount=amount):
                                raw = shifted(opcode, condition=condition, flags=flags,
                                              rn=rn, shift=shift, amount=amount)
                                self.assertEqual(mod.verified_alu_candidate(raw),
                                                 name + "_register_immediate_shift")

    def test_general_registers_including_sp_lr_are_not_game_specific(self):
        for opcode, name in ((4, "add"), (12, "orr"), (13, "mov")):
            for register in range(15):
                with self.subTest(opcode=opcode, register=register):
                    raw = shifted(opcode, rn=0 if opcode == 13 else register,
                                  rd=register, rm=register, amount=9)
                    self.assertEqual(mod.verified_alu_candidate(raw),
                                     name + "_register_immediate_shift")

    def test_every_other_data_processing_opcode_remains_unverified(self):
        for opcode in (0, 1, 2, 3, 5, 6, 7, 8, 9, 10, 11, 14, 15):
            for flags in (False, True):
                with self.subTest(opcode=opcode, flags=flags):
                    self.assertIsNone(mod.verified_alu_candidate(
                        shifted(opcode, flags=flags)))

    def test_pc_inputs_and_destinations_are_not_admitted(self):
        for opcode in (4, 12, 13):
            rn = 0 if opcode == 13 else 1
            for flags in (False, True):
                for kwargs in ({"rd": 15}, {"rm": 15}):
                    with self.subTest(opcode=opcode, flags=flags, registers=kwargs):
                        self.assertIsNone(mod.verified_alu_candidate(
                            shifted(opcode, rn=rn, flags=flags, **kwargs)))
                with self.subTest(opcode=opcode, flags=flags, rn=15):
                    self.assertIsNone(mod.verified_alu_candidate(
                        shifted(opcode, rn=15, flags=flags)))

    def test_mov_requires_canonical_unused_rn(self):
        for rn in range(1, 16):
            with self.subTest(rn=rn):
                self.assertIsNone(mod.verified_alu_candidate(shifted(13, rn=rn)))

    def test_register_specified_shifts_and_extra_encoding_space_rejected(self):
        for opcode in (4, 12, 13):
            raw = shifted(opcode, rn=0 if opcode == 13 else 1, amount=0)
            for rs in (0, 8, 14, 15):
                for bit7 in (0, 0x80):
                    with self.subTest(opcode=opcode, rs=rs, bit7=bit7):
                        self.assertIsNone(mod.verified_alu_candidate(
                            raw | (rs << 8) | bit7 | 0x10))

    def test_immediate_operand_and_other_primary_classes_rejected(self):
        for opcode in (4, 12, 13):
            raw = shifted(opcode, rn=0 if opcode == 13 else 1)
            for primary in range(1, 8):
                with self.subTest(opcode=opcode, primary=primary):
                    self.assertIsNone(mod.verified_alu_candidate(raw | (primary << 25)))

    def test_unconditional_and_non_word_values_rejected(self):
        for opcode in (4, 12, 13):
            raw = shifted(opcode, rn=0 if opcode == 13 else 1)
            for other in (raw | 0xF0000000, raw | (1 << 32), raw - (1 << 32)):
                with self.subTest(opcode=opcode, other=other):
                    self.assertIsNone(mod.verified_alu_candidate(other))


class SupplementRenderTests(unittest.TestCase):
    def render_words(self, descriptions):
        base = mod.frontend.BASE
        code = b"".join(struct.pack("<I", raw) for raw, _, _ in descriptions)
        inst = {base + i * 4: item for i, item in enumerate(descriptions)}
        return mod.render(code, inst, [(base, base + len(code))])

    def test_new_classes_retain_raw_s_and_condition_metadata(self):
        descriptions = [
            (shifted(13, rn=0, condition=0, flags=True, amount=3),
             "lslseq", "r2, r3, #3"),
            (shifted(4, condition=1, flags=True, shift=1, amount=5),
             "addsne", "r2, r1, r3, lsr #5"),
            (shifted(12, condition=14, shift=3, amount=0),
             "orr", "r2, r1, r3, rrx"),
            (0xE12FFF1E, "bx", "lr"),
        ]
        source, receipt = self.render_words(descriptions)
        records = receipt["blocks"][0]["instructions"]
        for record, (raw, _, _), condition, category in zip(
                records, descriptions, ("Eq", "Ne", "Al"),
                ("mov_register_immediate_shift", "add_register_immediate_shift",
                 "orr_register_immediate_shift")):
            self.assertEqual(record["raw"], raw)
            self.assertEqual(record["opcode"], "CoreAlu")
            self.assertEqual(record["condition"], condition)
            self.assertEqual(record["category"], category)
            # CoreAlu reads flags from raw, unlike the simple fast-path metadata.
            self.assertEqual(record["flags"], 0)
            self.assertIn(f"0x{raw:08X}U", source)
            self.assertIn(f"a32::Opcode::CoreAlu, a32::Condition::{condition}, 0x00U", source)

    def test_existing_fast_path_and_terminal_are_unchanged(self):
        descriptions = [(0xE3A02005, "mov", "r2, #5"), (0xE12FFF1E, "bx", "lr")]
        source, receipt = self.render_words(descriptions)
        records = receipt["blocks"][0]["instructions"]
        self.assertEqual((records[0]["opcode"], records[0]["condition"], records[0]["flags"]),
                         ("MovImm", "Al", 2))
        self.assertEqual(records[0]["category"], "fast_path")
        self.assertEqual(records[1]["opcode"], "BranchReg")
        self.assertIn("a32::Opcode::MovImm, a32::Condition::Al, 0x02U", source)

    def test_other_candidate_is_rejected_even_with_allowed_mnemonic(self):
        raw = shifted(1, flags=True)  # Authored EOR encoding, deliberately misleading mnemonic.
        with self.assertRaisesRegex(ValueError, "unverified supplemental opcode class"):
            self.render_words([(raw, "mov", "r2, r3, lsl #11"),
                               (0xE12FFF1E, "bx", "lr")])

    def test_unknown_llvm_instruction_cannot_be_rescued_by_admission(self):
        raw = shifted(13, rn=0, flags=True)
        with self.assertRaisesRegex(ValueError, "unverified supplemental opcode class"):
            self.render_words([(raw, "<unknown>", ""), (0xE12FFF1E, "bx", "lr")])

    def test_pc_and_register_shift_candidates_fail_before_emission(self):
        for raw in (shifted(13, rn=0, rd=15, flags=True),
                    shifted(4, rn=15), shifted(12, rm=15),
                    shifted(13, rn=0, amount=0) | (4 << 8) | 0x10):
            with self.subTest(raw=raw):
                with self.assertRaisesRegex(ValueError, "unverified supplemental opcode class"):
                    self.render_words([(raw, "mov", "r2, r3, lsl #11"),
                                       (0xE12FFF1E, "bx", "lr")])

    def test_disassembly_input_binding_stays_required(self):
        base = mod.frontend.BASE
        first = shifted(13, rn=0)
        other = shifted(13, rn=0, amount=9)
        with self.assertRaisesRegex(ValueError, "disassembly/input disagreement"):
            mod.render(struct.pack("<I", first),
                       {base: (other, "lsl", "r2, r3, #9")}, [(base, base + 4)])

    def test_terminal_and_internal_exit_guards_remain(self):
        with self.assertRaisesRegex(ValueError, "must end at a decoded branch/return"):
            self.render_words([(shifted(13, rn=0), "lsl", "r2, r3, #11")])
        with self.assertRaisesRegex(ValueError, "internal control-flow exit"):
            self.render_words([(0xEA000000, "b", "0x00100008"),
                               (0xE12FFF1E, "bx", "lr")])

    def test_manifest_interval_bounds_unchanged(self):
        base = mod.frontend.BASE

        def manifest(ranges):
            return {"format": "lego_observed_aot_supplements_v1",
                    "blocks": [{"entry": hex(start), "end": hex(end)}
                               for start, end in ranges]}

        self.assertEqual(mod.checked_ranges(manifest([(base, base + 256)])),
                         [(base, base + 256)])
        for ranges in ([], [(base, base + 260)], [(base + 0xFFC, base + 0x1004)],
                       [(base, base + 8), (base + 4, base + 12)],
                       [(base + 1, base + 4)], [(base, base)],
                       [(base + i * 4, base + (i + 1) * 4) for i in range(129)]):
            with self.subTest(ranges=ranges):
                with self.assertRaises(ValueError):
                    mod.checked_ranges(manifest(ranges))

    def test_main_rejects_unauthenticated_input_before_llvm_or_output(self):
        with tempfile.TemporaryDirectory(prefix="lego-synthetic-supplement-test-") as tmp:
            directory = Path(tmp)
            code = directory / "synthetic.bin"
            manifest = directory / "manifest.json"
            output = directory / "generated.cpp"
            code.write_bytes(b"synthetic input, not a game executable")
            manifest.write_text("{}")
            argv = ["generate_aot_supplement.py", str(code), str(manifest), str(output)]
            with mock.patch.object(sys, "argv", argv), mock.patch.object(
                    mod.frontend, "build_disassembly") as llvm:
                with self.assertRaisesRegex(ValueError, "requires exact supported private code.bin"):
                    mod.main()
            llvm.assert_not_called()
            self.assertFalse(output.exists())
            self.assertFalse(output.with_suffix(".json").exists())


if __name__ == "__main__":
    unittest.main()
