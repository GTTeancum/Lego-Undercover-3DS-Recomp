import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location(
    "generate_aot_pages", ROOT / "tools" / "generate_aot_pages.py"
)
mod = importlib.util.module_from_spec(spec)
assert spec.loader is not None
import sys
sys.modules[spec.name] = mod
spec.loader.exec_module(mod)


def op(pc, raw=0xE1A00000):
    return mod.RecoveryOp(pc, raw, "MovReg", "Al", 0)


class PageEmitterTests(unittest.TestCase):
    def test_block_is_split_at_page_boundary(self):
        block = mod.RecoveryBlock(
            mod.BASE + mod.PAGE_SIZE - 8,
            (
                op(mod.BASE + mod.PAGE_SIZE - 8),
                op(mod.BASE + mod.PAGE_SIZE - 4),
                op(mod.BASE + mod.PAGE_SIZE),
                op(mod.BASE + mod.PAGE_SIZE + 4),
            ),
        )
        pages = mod.split_blocks_into_pages([block])
        self.assertEqual(len(pages), 599)
        self.assertEqual([len(b.ops) for b in pages[0].blocks], [2])
        self.assertEqual([len(b.ops) for b in pages[1].blocks], [2])
        self.assertEqual(pages[1].blocks[0].pc, mod.BASE + mod.PAGE_SIZE)

    def test_empty_pages_still_render_registry_shards(self):
        pages = mod.split_blocks_into_pages([])
        source = mod.render_page(pages[0])
        self.assertIn("nullptr, 0U", source)
        self.assertIn("0x00100000U", source)
        self.assertIn("0x00100FFCU", source)

    def test_recovery_output_has_documented_604_artifacts(self):
        blocks = [mod.RecoveryBlock(mod.BASE, (op(mod.BASE),))]
        functions = [mod.RecoveryFunction(mod.BASE, mod.BASE + 4, "entry")]
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp)
            manifest = mod.emit_recovery_artifacts(out, blocks, functions)
            files = [path for path in out.iterdir() if path.is_file()]
            self.assertEqual(len(files), 604)
            self.assertEqual(manifest["recovery_layout"]["page_translation_units"], 599)
            self.assertEqual(manifest["recovery_layout"]["artifact_count_including_manifest"], 604)
            self.assertTrue((out / "lego_page_00100.cpp").is_file())
            self.assertTrue((out / "lego_page_00356.cpp").is_file())

    def test_k01_page_files_follow_recovery_naming(self):
        expected = {
            0x001C1D78: "lego_page_001C1.cpp",
            0x00227300: "lego_page_00227.cpp",
            0x0023F168: "lego_page_0023F.cpp",
            0x002CA7BC: "lego_page_002CA.cpp",
        }
        for address, name in expected.items():
            index = mod.page_index(address)
            page = mod.RecoveryPage(index, mod.BASE + index * mod.PAGE_SIZE, ())
            self.assertEqual(page.source_name, name)


if __name__ == "__main__":
    unittest.main()
