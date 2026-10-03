import importlib.util
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("generate_aot", ROOT / "tools" / "generate_aot.py")
generate_aot = importlib.util.module_from_spec(spec)
assert spec.loader is not None
sys.modules[spec.name] = generate_aot
spec.loader.exec_module(generate_aot)


class InventoryTests(unittest.TestCase):
    def test_documented_initializer_table_geometry(self):
        self.assertEqual(
            (generate_aot.INITIALIZER_TABLE_END - generate_aot.INITIALIZER_TABLE_BEGIN) // 4,
            296,
        )

    def test_relative_initializer_roots(self):
        size = generate_aot.INITIALIZER_TABLE_END - generate_aot.BASE
        code = bytearray(size)
        targets = []
        for index, slot in enumerate(
            range(generate_aot.INITIALIZER_TABLE_BEGIN, generate_aot.INITIALIZER_TABLE_END, 4)
        ):
            target = generate_aot.BASE + (index * 4)
            targets.append(target)
            displacement = (target - slot) & 0xFFFFFFFF
            code[slot - generate_aot.BASE: slot - generate_aot.BASE + 4] = displacement.to_bytes(4, "little")
        self.assertEqual(generate_aot.initializer_roots(bytes(code)), targets)


if __name__ == "__main__":
    unittest.main()
