import importlib.util
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location(
    "build_process_manifest", ROOT / "tools" / "build_process_manifest.py"
)
mod = importlib.util.module_from_spec(spec)
assert spec.loader is not None
sys.modules[spec.name] = mod
spec.loader.exec_module(mod)


class ProcessManifestTests(unittest.TestCase):
    def test_known_layout_arithmetic(self):
        text_pages = 599 * 0x1000
        ro_pages = 20 * 0x1000
        data_pages = 28 * 0x1000
        bss_pages = mod.align_up(2_219_880, 0x1000)
        stack = 0x10000
        self.assertEqual(
            text_pages + ro_pages + data_pages + bss_pages + stack,
            mod.EXPECTED_INITIAL_COMMIT,
        )
        self.assertEqual(mod.EXPECTED_INITIAL_COMMIT, 4_935_680)

    def test_stage2_heap_accounting(self):
        self.assertEqual(
            mod.EXPECTED_INITIAL_COMMIT + 0x0124B000 + 0x02900000,
            67_108_864,
        )


if __name__ == "__main__":
    unittest.main()
