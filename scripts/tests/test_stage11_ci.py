#!/usr/bin/env python3
"""Checks for Stage 11 CI matrix, site validation, and workflows."""

from __future__ import annotations

import json
import subprocess
import sys
import unittest
from pathlib import Path


SCRIPTS_DIR = Path(__file__).resolve().parents[1]
REPO_ROOT = SCRIPTS_DIR.parent
sys.path.insert(0, str(SCRIPTS_DIR))

from project_registry import load_registry  # noqa: E402
from test_project_registry import RegistryFixture, project_entry  # noqa: E402
from validate_site import validate_site  # noqa: E402


CI_WORKFLOW = REPO_ROOT / ".github" / "workflows" / "ci.yml"
PAGES_WORKFLOW = REPO_ROOT / ".github" / "workflows" / "pages.yml"


def _write_package(project_root: Path, name: str, version: str) -> None:
    docs = project_root / "site" / "public" / "firmware" / "alpha"
    docs.mkdir(parents=True, exist_ok=True)
    for filename in ("bootloader.bin", "partitions.bin", "firmware.bin"):
        (docs / filename).write_bytes(b"bin")
    (docs / "manifest.json").write_text(
        json.dumps(
            {
                "name": name,
                "version": version,
                "builds": [
                    {
                        "chipFamily": "ESP32",
                        "parts": [
                            {"path": "bootloader.bin", "offset": 4096},
                            {"path": "partitions.bin", "offset": 32768},
                            {"path": "firmware.bin", "offset": 65536},
                        ],
                    }
                ],
            }
        ),
        encoding="utf-8",
    )


class Stage11CiTests(unittest.TestCase):
    def test_firmware_matrix_covers_buildable_environments(self) -> None:
        matrix = load_registry().firmware_ci_matrix()
        pairs = {(item["project"], item["env"]) for item in matrix["include"]}
        self.assertEqual(
            {
                ("justin", "production"),
                ("justin", "wokwi"),
                ("moon_phase", "production"),
                ("moon_phase", "wokwi"),
                ("weather_watch", "production"),
                ("weather_watch", "wokwi"),
                ("real_weather", "production"),
                ("real_weather", "wokwi"),
                ("flight_watch", "production"),
                ("flight_watch", "wokwi"),
                ("flight_tracker", "production"),
                ("flight_tracker", "wokwi"),
                ("starter-max7219", "production"),
                ("starter-max7219", "wokwi"),
            },
            pairs,
        )
        self.assertFalse(any(item["project"] == "starter-template" for item in matrix["include"]))

    def test_ci_matrix_script_prints_json(self) -> None:
        result = subprocess.run(
            [str(REPO_ROOT / "scripts" / "ci-matrix.sh")],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(0, result.returncode, result.stderr)
        data = json.loads(result.stdout)
        self.assertIn("include", data)
        self.assertEqual(14, len(data["include"]))

    def test_validate_site_rejects_version_mismatch(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        entry = project_entry("alpha", "/alpha/")
        entry["docs"] = {
            "route": "/alpha/",
            "path": "site/public/firmware/alpha",
            "manifest": "site/public/firmware/alpha/manifest.json",
        }
        path = fixture.write([entry])
        _write_package(fixture.root, "Alpha", "9.9.9")
        registry = load_registry(path=path, root=fixture.root)
        errors = validate_site(registry=registry, dist=fixture.root / "missing")
        self.assertTrue(any("manifest version" in error for error in errors))

    def test_validate_site_rejects_missing_firmware(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        entry = project_entry("alpha", "/alpha/")
        entry["docs"] = {
            "route": "/alpha/",
            "path": "site/public/firmware/alpha",
            "manifest": "site/public/firmware/alpha/manifest.json",
        }
        path = fixture.write([entry])
        _write_package(fixture.root, "Alpha", "1.0.0")
        (fixture.root / "site" / "public" / "firmware" / "alpha" / "firmware.bin").unlink()
        registry = load_registry(path=path, root=fixture.root)
        errors = validate_site(registry=registry, dist=fixture.root / "missing")
        self.assertTrue(any("missing published firmware.bin" in error for error in errors))

    def test_workflows_keep_firmware_and_pages_separate(self) -> None:
        self.assertTrue(CI_WORKFLOW.is_file())
        self.assertTrue(PAGES_WORKFLOW.is_file())
        ci = CI_WORKFLOW.read_text(encoding="utf-8")
        pages = PAGES_WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("./scripts/validate.sh", ci)
        self.assertIn("./scripts/build.sh", ci)
        self.assertIn("./scripts/build-site.sh", ci)
        self.assertIn("./scripts/validate-site.sh", ci)
        self.assertIn("actions/cache@", ci)
        self.assertNotIn("deploy-pages", ci)
        self.assertIn("./scripts/build-site.sh", pages)
        self.assertIn("./scripts/validate-site.sh", pages)
        self.assertIn("actions/upload-pages-artifact@", pages)
        self.assertIn("actions/deploy-pages@", pages)
        self.assertIn("actions/setup-node@", pages)
        self.assertIn("cache: npm", pages)


if __name__ == "__main__":
    unittest.main()
