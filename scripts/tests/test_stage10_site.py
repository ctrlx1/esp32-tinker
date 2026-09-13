#!/usr/bin/env python3
"""Structural checks for the Stage 10 Astro installer site."""

from pathlib import Path
import sys
import unittest

SCRIPTS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS_DIR))

from project_registry import load_registry  # noqa: E402


REPO_ROOT = Path(__file__).resolve().parents[2]
SITE = REPO_ROOT / "site"


class Stage10SiteTests(unittest.TestCase):
    def test_buildable_projects_publish_stable_firmware_bin(self) -> None:
        registry = load_registry()
        for project in registry.projects:
            with self.subTest(project=project.id):
                self.assertTrue(project.docs["route"].startswith("/"))
                self.assertTrue(
                    str(project.docs_path).endswith(
                        f"site/public/firmware/{project.id}"
                    )
                    or project.docs["path"]
                    == f"site/public/firmware/{project.id}"
                )
                if not project.buildable:
                    self.assertIsNone(project.docs.get("manifest"))
                    continue
                self.assertEqual("firmware.bin", project.published_app_name)
                self.assertEqual(
                    f"site/public/firmware/{project.id}/manifest.json",
                    project.docs["manifest"],
                )

    def test_justin_route_is_no_longer_the_catalog_root(self) -> None:
        justin = load_registry().project("justin")
        self.assertEqual("/justin/", justin.docs["route"])

    def test_site_sources_cover_registry_routes(self) -> None:
        catalog = (SITE / "src" / "pages" / "index.astro").read_text()
        project_page = (SITE / "src" / "pages" / "[slug].astro").read_text()
        config = (SITE / "astro.config.mjs").read_text()
        preview = (REPO_ROOT / "scripts" / "preview-installer.sh").read_text()

        self.assertIn("loadProjects()", catalog)
        self.assertIn("Justin", catalog)
        self.assertIn("getStaticPaths", project_page)
        self.assertIn("InstallButton", project_page)
        self.assertIn('base: "/esp32-tinker/"', config)
        self.assertIn("preview-site.sh", preview)

        for name in (
            "ProjectCard.astro",
            "InstallButton.astro",
            "Pinout.astro",
            "SetupSteps.astro",
            "FirmwareDownload.astro",
            "VersionBadge.astro",
            "HardwareBadge.astro",
        ):
            self.assertTrue((SITE / "src" / "components" / name).is_file())

    def test_published_manifests_use_firmware_bin(self) -> None:
        registry = load_registry()
        for project in registry.projects:
            if not project.buildable or not project.manifest_path.is_file():
                continue
            text = project.manifest_path.read_text(encoding="utf-8")
            self.assertIn('"firmware.bin"', text)
            self.assertNotIn(f"{project.id}_", text)


if __name__ == "__main__":
    unittest.main()
