import hashlib
import subprocess
import sys
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
CATALOG_DIR = (
    REPO_ROOT
    / "firmware"
    / "justin"
    / "src"
    / "programs"
    / "pixel_art"
    / "catalog"
)
PORTAL_SHELL = (
    REPO_ROOT / "packages" / "tinker-core" / "assets" / "portal" / "shell.html"
)
PORTAL_CONTENT = (
    REPO_ROOT / "firmware" / "justin" / "portal" / "portal_content.html"
)
PORTAL_SETTINGS = (
    REPO_ROOT / "firmware" / "justin" / "portal" / "program_settings.html"
)


def tree_digest(root: Path) -> tuple[int, int, str]:
    files = sorted(path for path in root.rglob("*") if path.is_file())
    digest = hashlib.sha256()
    for path in files:
        digest.update(path.relative_to(root).as_posix().encode())
        digest.update(b"\0")
        digest.update(path.read_bytes())
    return len(files), sum(path.stat().st_size for path in files), digest.hexdigest()


class JustinAssetTests(unittest.TestCase):
    def test_pixel_art_catalog_matches_migration_baseline(self):
        self.assertEqual(
            tree_digest(CATALOG_DIR),
            (
                444,
                421188,
                "8f1a03bcf9545e712dda827a2d740ac28fa9d9f23b0f4594639c93dae7f0d554",
            ),
        )

    def test_pixel_import_skips_pillow_without_source_assets(self):
        before = tree_digest(CATALOG_DIR)
        result = subprocess.run(
            [sys.executable, "-S", "scripts/import-pixel-art.py"],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("keeping existing generated catalog", result.stdout)
        self.assertEqual(tree_digest(CATALOG_DIR), before)

    def test_composed_portal_matches_presplit_page(self):
        shell = PORTAL_SHELL.read_text(encoding="utf-8")
        self.assertEqual(shell.count("{{PROJECT_PORTAL_CONTENT}}"), 1)
        content = PORTAL_CONTENT.read_text(encoding="utf-8").rstrip("\n")
        settings = PORTAL_SETTINGS.read_text(encoding="utf-8").rstrip("\n")
        self.assertEqual(content.count("{{JUSTIN_PROGRAM_SETTINGS}}"), 1)
        content = content.replace("{{JUSTIN_PROGRAM_SETTINGS}}", settings)
        composed = shell.replace("{{PROJECT_PORTAL_CONTENT}}", content)
        self.assertEqual(len(composed.encode()), 52358)
        self.assertEqual(
            hashlib.sha256(composed.encode()).hexdigest(),
            "837ab484645b28aca486e9ecc1d7abea92805838259f0c94c12e0b2b8f825492",
        )


if __name__ == "__main__":
    unittest.main()
