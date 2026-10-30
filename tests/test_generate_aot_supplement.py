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


def immediate_test(*, opcode=8, condition=14, flags=True, rn=6, rd=0,
                   rotate=0, value=0x53, primary=1):
    """Author a data-processing immediate test and deliberately varied neighbors."""
    return ((condition << 28) | (primary << 25) | (opcode << 21) |
            (int(flags) << 20) | (rn << 16) | (rd << 12) |
            (rotate << 8) | value)


def range_manifest(ranges):
    return {"format": "lego_observed_aot_supplements_v1",
            "blocks": [{"entry": hex(start), "end": hex(end)}
                       for start, end in ranges]}


class AluAdmissionTests(unittest.TestCase):
    def test_supported_forms_keep_all_conditions_s_bits_and_shift_encodings(self):
        for opcode, name, rn in ((0, "and", 8), (2, "sub", 8), (4, "add", 8),
                                 (12, "orr", 8), (13, "mov", 0)):
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
        for opcode, name in ((0, "and"), (2, "sub"), (4, "add"),
                             (12, "orr"), (13, "mov")):
            for register in range(15):
                with self.subTest(opcode=opcode, register=register):
                    raw = shifted(opcode, rn=0 if opcode == 13 else register,
                                  rd=register, rm=register, amount=9)
                    self.assertEqual(mod.verified_alu_candidate(raw),
                                     name + "_register_immediate_shift")

    def test_every_other_data_processing_opcode_remains_unverified(self):
        for opcode in (1, 3, 5, 6, 7, 8, 9, 10, 11, 14, 15):
            for flags in (False, True):
                with self.subTest(opcode=opcode, flags=flags):
                    self.assertIsNone(mod.verified_alu_candidate(
                        shifted(opcode, flags=flags)))

    def test_pc_inputs_and_destinations_are_not_admitted(self):
        for opcode in (0, 2, 4, 12, 13):
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
        for opcode in (0, 2, 4, 12, 13):
            raw = shifted(opcode, rn=0 if opcode == 13 else 1, amount=0)
            for rs in (0, 8, 14, 15):
                for bit7 in (0, 0x80):
                    with self.subTest(opcode=opcode, rs=rs, bit7=bit7):
                        self.assertIsNone(mod.verified_alu_candidate(
                            raw | (rs << 8) | bit7 | 0x10))

    def test_immediate_operand_and_other_primary_classes_rejected(self):
        for opcode in (0, 2, 4, 12, 13):
            raw = shifted(opcode, rn=0 if opcode == 13 else 1)
            for primary in range(1, 8):
                with self.subTest(opcode=opcode, primary=primary):
                    actual = mod.verified_alu_candidate(raw | (primary << 25))
                    if opcode == 0 and primary == 1:
                        self.assertEqual(actual, "and_immediate")
                    else:
                        self.assertIsNone(actual)

    def test_unconditional_and_non_word_values_rejected(self):
        for opcode in (0, 2, 4, 12, 13):
            raw = shifted(opcode, rn=0 if opcode == 13 else 1)
            for other in (raw | 0xF0000000, raw | (1 << 32), raw - (1 << 32)):
                with self.subTest(opcode=opcode, other=other):
                    self.assertIsNone(mod.verified_alu_candidate(other))


class TstImmediateAdmissionTests(unittest.TestCase):
    def test_conditions_and_source_registers_are_generic(self):
        for condition in range(15):
            for rn in range(15):
                with self.subTest(condition=condition, rn=rn):
                    self.assertEqual(mod.verified_alu_candidate(
                        immediate_test(condition=condition, rn=rn, rotate=5)),
                        "tst_immediate")

    def test_all_rotation_encodings_and_immediate_boundaries_are_retained(self):
        for rotate in range(16):
            for value in (0, 1, 0x7F, 0x80, 0xFF):
                with self.subTest(rotate=rotate, value=value):
                    self.assertEqual(mod.verified_alu_candidate(
                        immediate_test(rotate=rotate, value=value)),
                        "tst_immediate")

    def test_s_bit_and_unused_destination_must_be_canonical(self):
        self.assertIsNone(mod.verified_alu_candidate(immediate_test(flags=False)))
        for rd in range(1, 16):
            for flags in (False, True):
                with self.subTest(rd=rd, flags=flags):
                    self.assertIsNone(mod.verified_alu_candidate(
                        immediate_test(rd=rd, flags=flags)))

    def test_pc_source_is_not_admitted(self):
        for condition in range(15):
            with self.subTest(condition=condition):
                self.assertIsNone(mod.verified_alu_candidate(
                    immediate_test(condition=condition, rn=15)))

    def test_unconditional_and_non_word_values_remain_rejected(self):
        raw = immediate_test()
        for other in (immediate_test(condition=15), raw | (1 << 32),
                      raw - (1 << 32)):
            with self.subTest(other=other):
                self.assertIsNone(mod.verified_alu_candidate(other))

    def test_other_immediate_opcodes_gain_no_candidate_admission(self):
        # Existing fast-path opcodes are classified separately by the frontend.
        # This new candidate permission must not admit neighboring ALU forms.
        for opcode in range(16):
            if opcode in (0, 8):  # Independently admitted AND and canonical TST.
                continue
            for flags in (False, True):
                with self.subTest(opcode=opcode, flags=flags):
                    self.assertIsNone(mod.verified_alu_candidate(
                        immediate_test(opcode=opcode, flags=flags)))

    def test_register_tst_and_other_primary_classes_remain_unverified(self):
        for primary in (0, 2, 3, 4, 5, 6, 7):
            with self.subTest(primary=primary):
                self.assertIsNone(mod.verified_alu_candidate(
                    immediate_test(primary=primary)))
        for shift in range(4):
            raw = shifted(8, flags=True, rn=6, rd=0, shift=shift, amount=0)
            for register_shift in (0, (7 << 8) | 0x10):
                with self.subTest(shift=shift, register_shift=register_shift):
                    self.assertIsNone(mod.verified_alu_candidate(raw | register_shift))



class AndImmediateAdmissionTests(unittest.TestCase):
    def test_all_conditions_registers_flags_and_rotations(self):
        for condition in range(15):
            for register in range(15):
                for flags in (False, True):
                    for rotate in range(16):
                        raw = immediate_test(opcode=0, condition=condition, flags=flags,
                                             rn=register, rd=14-register,
                                             rotate=rotate, value=0xA5)
                        self.assertEqual(mod.verified_alu_candidate(raw), "and_immediate")

    def test_all_immediate_bytes_and_rotations(self):
        for value in range(256):
            for rotate in range(16):
                for flags in (False, True):
                    raw = immediate_test(opcode=0, flags=flags, rd=7,
                                         rotate=rotate, value=value)
                    self.assertEqual(mod.verified_alu_candidate(raw), "and_immediate")

    def test_pc_and_reserved_condition_are_rejected(self):
        for flags in (False, True):
            for fields in ({"rn": 15}, {"rd": 15}, {"condition": 15}):
                self.assertIsNone(mod.verified_alu_candidate(
                    immediate_test(opcode=0, flags=flags, **fields)))
        raw = immediate_test(opcode=0)
        self.assertIsNone(mod.verified_alu_candidate(raw | (1 << 32)))
        self.assertIsNone(mod.verified_alu_candidate(raw - (1 << 32)))

    def test_other_primary_classes_gain_no_immediate_admission(self):
        for primary in (0, 2, 3, 4, 5, 6, 7):
            self.assertNotEqual(mod.verified_alu_candidate(
                immediate_test(opcode=0, primary=primary)), "and_immediate")

    def test_neighboring_immediate_opcodes_remain_rejected(self):
        for opcode in range(1, 16):
            if opcode == 8:
                continue
            for flags in (False, True):
                self.assertIsNone(mod.verified_alu_candidate(
                    immediate_test(opcode=opcode, flags=flags)))

    def test_render_preserves_raw_condition_and_flag_behavior(self):
        base = mod.frontend.BASE
        raw = immediate_test(opcode=0, condition=1, flags=True,
                             rn=5, rd=7, rotate=1, value=0x80)
        end = 0xE12FFF1E
        code = struct.pack("<II", raw, end)
        instructions = {base: (raw, "andsne", "r7, r5, #32"),
                        base+4: (end, "bx", "lr")}
        source, receipt = mod.render(code, instructions, [(base, base+8)])
        row = receipt["blocks"][0]["instructions"][0]
        self.assertEqual((row["raw"], row["opcode"], row["condition"],
                          row["flags"], row["category"]),
                         (raw, "CoreAlu", "Ne", 0, "and_immediate"))
        self.assertIn(f"0x{raw:08X}U", source)
        for bad in (immediate_test(opcode=1), immediate_test(opcode=0, rd=15),
                    immediate_test(opcode=0, rn=15), immediate_test(opcode=0, condition=15)):
            with self.assertRaisesRegex(ValueError, "unverified supplemental opcode class"):
                mod.render(struct.pack("<II", bad, end),
                           {base: (bad, "and", "r7, r5, #32"), base+4: (end, "bx", "lr")},
                           [(base, base+8)])
        with self.assertRaisesRegex(ValueError, "unverified supplemental opcode class"):
            mod.render(code, {base: (raw, "<unknown>", ""), base+4: (end, "bx", "lr")},
                       [(base, base+8)])

class SupplementRenderTests(unittest.TestCase):
    def render_words(self, descriptions):
        base = mod.frontend.BASE
        code = b"".join(struct.pack("<I", raw) for raw, _, _ in descriptions)
        inst = {base + i * 4: item for i, item in enumerate(descriptions)}
        ranges = mod.checked_ranges(range_manifest([(base, base + len(code))]))
        return mod.render(code, inst, ranges)

    def test_new_classes_retain_raw_s_and_condition_metadata(self):
        descriptions = [
            (shifted(13, rn=0, condition=0, flags=True, amount=3),
             "lslseq", "r2, r3, #3"),
            (shifted(4, condition=1, flags=True, shift=1, amount=5),
             "addsne", "r2, r1, r3, lsr #5"),
            (shifted(12, condition=14, shift=3, amount=0),
             "orr", "r2, r1, r3, rrx"),
            (shifted(0, condition=2, flags=True, rn=5, rd=6, rm=7,
                     shift=2, amount=31), "andscs", "r6, r5, r7, asr #31"),
            (shifted(2, condition=12, flags=True, rn=8, rd=9, rm=10,
                     shift=3, amount=0), "subsgt", "r9, r8, r10, rrx"),
            (0xE12FFF1E, "bx", "lr"),
        ]
        source, receipt = self.render_words(descriptions)
        records = receipt["blocks"][0]["instructions"]
        for record, (raw, _, _), condition, category in zip(
                records, descriptions, ("Eq", "Ne", "Al", "Cs", "Gt"),
                ("mov_register_immediate_shift", "add_register_immediate_shift",
                 "orr_register_immediate_shift", "and_register_immediate_shift",
                 "sub_register_immediate_shift")):
            self.assertEqual(record["raw"], raw)
            self.assertEqual(record["opcode"], "CoreAlu")
            self.assertEqual(record["condition"], condition)
            self.assertEqual(record["category"], category)
            # CoreAlu reads flags from raw, unlike the simple fast-path metadata.
            self.assertEqual(record["flags"], 0)
            self.assertIn(f"0x{raw:08X}U", source)
            self.assertIn(f"a32::Opcode::CoreAlu, a32::Condition::{condition}, 0x00U", source)

    def test_tst_immediate_retains_condition_rotation_and_raw_flag_bit(self):
        descriptions = [
            (immediate_test(condition=0), "tsteq", "r6, #83"),
            (immediate_test(condition=1, rn=10, rotate=1, value=2),
             "tstne", "r10, #2147483648"),
            (immediate_test(condition=6, rn=14, rotate=4, value=0xF0),
             "tstvs", "lr, #4026531840"),
            (0xE12FFF1E, "bx", "lr"),
        ]
        source, receipt = self.render_words(descriptions)
        records = receipt["blocks"][0]["instructions"]
        for record, (raw, _, _), condition in zip(
                records, descriptions, ("Eq", "Ne", "Vs")):
            self.assertEqual(record["raw"], raw)
            self.assertEqual(record["opcode"], "CoreAlu")
            self.assertEqual(record["condition"], condition)
            self.assertEqual(record["category"], "tst_immediate")
            self.assertEqual(record["flags"], 0)
            self.assertIn(f"0x{raw:08X}U", source)
            self.assertIn(f"a32::Opcode::CoreAlu, a32::Condition::{condition}, 0x00U", source)

    def test_unverified_tst_neighbors_cannot_use_an_allowed_mnemonic(self):
        for raw in (immediate_test(flags=False), immediate_test(rd=9),
                    immediate_test(rn=15), immediate_test(condition=15),
                    immediate_test(opcode=9),
                    shifted(8, flags=True, rn=6, rd=0)):
            with self.subTest(raw=raw):
                with self.assertRaisesRegex(ValueError, "unverified supplemental opcode class"):
                    self.render_words([(raw, "tst", "r6, #83"),
                                       (0xE12FFF1E, "bx", "lr")])

    def test_existing_fast_path_and_terminal_are_unchanged(self):
        descriptions = [(0xE3A02005, "mov", "r2, #5"), (0xE12FFF1E, "bx", "lr")]
        source, receipt = self.render_words(descriptions)
        records = receipt["blocks"][0]["instructions"]
        self.assertEqual((records[0]["opcode"], records[0]["condition"], records[0]["flags"]),
                         ("MovImm", "Al", 2))
        self.assertEqual(records[0]["category"], "fast_path")
        self.assertEqual(records[1]["opcode"], "BranchReg")
        self.assertIn("a32::Opcode::MovImm, a32::Condition::Al, 0x02U", source)

    def test_simple_sub_forms_keep_existing_fast_path_metadata(self):
        descriptions = [
            (shifted(2, condition=1, flags=True, rn=5, rd=6, rm=7, amount=0),
             "subsne", "r6, r5, r7"),
            (immediate_test(opcode=2, flags=False, rn=5, rd=6, value=0x2D),
             "sub", "r6, r5, #45"),
            (0xE12FFF1E, "bx", "lr"),
        ]
        _, receipt = self.render_words(descriptions)
        records = receipt["blocks"][0]["instructions"]
        self.assertEqual([(row["opcode"], row["condition"], row["flags"], row["category"])
                          for row in records[:2]],
                         [("Sub", "Ne", 1, "fast_path"),
                          ("Sub", "Al", 2, "fast_path")])
        self.assertEqual([row["raw"] for row in records],
                         [raw for raw, _, _ in descriptions])

    def test_other_candidate_is_rejected_even_with_allowed_mnemonic(self):
        raw = shifted(1, flags=True)  # Authored EOR encoding, deliberately misleading mnemonic.
        with self.assertRaisesRegex(ValueError, "unverified supplemental opcode class"):
            self.render_words([(raw, "mov", "r2, r3, lsl #11"),
                               (0xE12FFF1E, "bx", "lr")])

    def test_unknown_llvm_instruction_cannot_be_rescued_by_admission(self):
        for raw in (shifted(0, flags=True), shifted(2, flags=True),
                    shifted(13, rn=0, flags=True), immediate_test()):
            with self.subTest(raw=raw):
                with self.assertRaisesRegex(ValueError, "unverified supplemental opcode class"):
                    self.render_words([(raw, "<unknown>", ""), (0xE12FFF1E, "bx", "lr")])

    def test_pc_and_register_shift_candidates_fail_before_emission(self):
        for raw in (shifted(13, rn=0, rd=15, flags=True),
                    shifted(4, rn=15), shifted(12, rm=15),
                    shifted(13, rn=0, amount=0) | (4 << 8) | 0x10,
                    shifted(0, rd=15, flags=True), shifted(2, rn=15),
                    shifted(0, amount=0) | (6 << 8) | 0x10,
                    shifted(2, amount=0) | (6 << 8) | 0x10):
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

    def test_full_512_byte_block_retains_every_word_and_terminal(self):
        forms = [
            (shifted(0, rn=11, rd=5, rm=9, shift=2, amount=11),
             "and", "r5, r11, r9, asr #11"),
            (shifted(2, rn=6, rd=10, rm=3, flags=True, shift=1, amount=9),
             "subs", "r10, r6, r3, lsr #9"),
        ]
        descriptions = [forms[i % 2] for i in range(127)]
        descriptions.append((0x1A000000, "bne", "#0"))
        _, receipt = self.render_words(descriptions)
        block = receipt["blocks"][0]
        self.assertEqual(block["end"] - block["entry"], 512)
        self.assertEqual(len(block["instructions"]), 128)
        self.assertEqual([row["raw"] for row in block["instructions"]],
                         [raw for raw, _, _ in descriptions])
        self.assertEqual((block["instructions"][-1]["opcode"],
                          block["instructions"][-1]["condition"]), ("Branch", "Ne"))

    def test_larger_blocks_still_require_the_first_control_exit_at_the_end(self):
        body = (shifted(0, rn=11, rd=5, rm=9, shift=2, amount=11),
                "and", "r5, r11, r9, asr #11")
        with self.assertRaisesRegex(ValueError, "must end at a decoded branch/return"):
            self.render_words([body] * 128)
        # A conditional branch remains a control exit even when it might fall through.
        descriptions = [body] * 128
        descriptions[96] = (0x1A000000, "bne", "#0")
        descriptions[-1] = (0xE12FFF1E, "bx", "lr")
        with self.assertRaisesRegex(ValueError, "internal control-flow exit"):
            self.render_words(descriptions)

    def test_manifest_size_page_text_overlap_and_count_bounds(self):
        base = mod.frontend.BASE
        for size in (4, 256, 260, 508, 512):
            with self.subTest(size=size):
                ranges = [(base, base + size)]
                self.assertEqual(mod.checked_ranges(range_manifest(ranges)), ranges)
        for ranges in ([(base + 0xE00, base + 0x1000)],
                       [(base + 512, base + 1024), (base, base + 512)],
                       [(base + i * 4, base + (i + 1) * 4) for i in range(128)]):
            with self.subTest(ranges=ranges):
                self.assertEqual(mod.checked_ranges(range_manifest(ranges)), sorted(ranges))
        for ranges in ([], [(base, base + 516)], [(base + 0xFFC, base + 0x1004)],
                       [(base, base + 8), (base + 4, base + 12)],
                       [(base + 1, base + 4)], [(base, base + 6)], [(base, base)],
                       [(base + 4, base)], [(base - 4, base)],
                       [(base + mod.frontend.TEXT_BYTES, base + mod.frontend.TEXT_BYTES + 4)],
                       [(base + i * 4, base + (i + 1) * 4) for i in range(129)]):
            with self.subTest(ranges=ranges):
                with self.assertRaises(ValueError):
                    mod.checked_ranges(range_manifest(ranges))

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
