import importlib.util
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location(
    "build_aot_page_manifest", ROOT / "tools" / "build_aot_page_manifest.py"
)
pages = importlib.util.module_from_spec(spec)
assert spec.loader is not None
sys.modules[spec.name] = pages
spec.loader.exec_module(pages)


class PageManifestTests(unittest.TestCase):
    def test_geometry(self):
        self.assertEqual(pages.TEXT_PAGES, 599)
        self.assertEqual(pages.TEXT_ALLOCATED_BYTES, 599 * 0x1000)
        self.assertEqual(pages.RECOVERY_J_GENERATED_FILES - pages.TEXT_PAGES, 5)
        self.assertEqual(
            pages.TEXT_BYTES - (pages.TEXT_PAGES - 1) * pages.PAGE_SIZE,
            1324,
        )

    def test_k01_pages(self):
        self.assertEqual(
            [pages.page_start(address) for address in pages.K01_ADDRESSES],
            [0x001C1000, 0x00227000, 0x0023F000, 0x002CA000],
        )
        self.assertEqual(
            len({pages.page_start(address) for address in pages.K01_ADDRESSES}),
            4,
        )

    def test_bounds(self):
        with self.assertRaises(ValueError):
            pages.page_start(pages.BASE - 4)
        with self.assertRaises(ValueError):
            pages.page_start(pages.BASE + pages.TEXT_ALLOCATED_BYTES)


if __name__ == "__main__":
    unittest.main()
