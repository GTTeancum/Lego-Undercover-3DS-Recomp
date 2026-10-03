import importlib.util
import unittest
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("prepare_game", ROOT / "tools" / "prepare_game.py")
prepare_game = importlib.util.module_from_spec(spec)
assert spec.loader is not None
sys.modules[spec.name] = prepare_game
spec.loader.exec_module(prepare_game)


class PrepareGameTests(unittest.TestCase):
    def test_blz_rejects_truncated_input(self):
        with self.assertRaises(prepare_game.PreparationError):
            prepare_game.decompress_exefs_code(b"short")

    def test_service_names_are_read_from_fixed_slots(self):
        exheader = bytearray(0x800)
        exheader[0x250:0x258] = b"APT:U\0\0\0"
        exheader[0x258:0x260] = b"fs:USER\0"
        self.assertEqual(prepare_game.service_access(bytes(exheader)), ["APT:U", "fs:USER"])

    def test_known_constants(self):
        self.assertEqual(prepare_game.EXPECTED_PROGRAM_ID, 0x00040000000AD500)
        self.assertEqual(prepare_game.EXPECTED_CODE_SIZE, 2650112)
        self.assertEqual(len(prepare_game.EXPECTED_CODE_SHA256), 64)


if __name__ == "__main__":
    unittest.main()
