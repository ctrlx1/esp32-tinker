#!/usr/bin/env python3
"""Structural checks for the Stage 8 standalone firmware split."""

from pathlib import Path
import unittest


REPO_ROOT = Path(__file__).resolve().parents[2]
FIRMWARE_ROOT = REPO_ROOT / "firmware"
PROJECT_IDS = ("moon_phase", "weather_watch", "real_weather", "flight_watch")


class Stage8ProjectTests(unittest.TestCase):
    def test_extracted_sources_have_one_project_owner(self) -> None:
        for project_id in PROJECT_IDS:
            with self.subTest(project=project_id):
                source_dir = (
                    FIRMWARE_ROOT / project_id / "src" / "programs" / project_id
                )
                self.assertTrue((source_dir / f"{project_id}.cpp").is_file())
                self.assertTrue((source_dir / f"{project_id}.h").is_file())
                self.assertFalse(
                    (FIRMWARE_ROOT / "justin" / "src" / "programs" /
                     f"{project_id}.cpp").exists()
                )

    def test_standalone_build_metadata_stays_project_qualified(self) -> None:
        for project_id in PROJECT_IDS:
            with self.subTest(project=project_id):
                project_dir = FIRMWARE_ROOT / project_id
                platformio = (project_dir / "platformio.ini").read_text()
                wrapper = (project_dir / "build.sh").read_text()
                portal = (project_dir / "portal" / "portal_content.html").read_text()

                self.assertIn(
                    f"custom_tinker_project_id = {project_id}", platformio
                )
                self.assertIn("scripts/pio/configure_firmware.py", platformio)
                self.assertIn('scripts/build.sh" ' + project_id, wrapper)
                self.assertEqual(1, portal.count("APP_VERSION_PLACEHOLDER"))
                self.assertEqual(
                    1, portal.count("APP_FIRMWARE_FILENAME_PLACEHOLDER")
                )

    def test_justin_masks_removed_program_bits_without_reindexing(self) -> None:
        project = (
            FIRMWARE_ROOT / "justin" / "src" / "justin_project.cpp"
        ).read_text()
        settings = (
            FIRMWARE_ROOT / "justin" / "src" / "justin_settings.h"
        ).read_text()

        self.assertIn("constexpr uint8_t PROGRAM_COUNT = 4;", project)
        self.assertIn(
            'preferences.getUChar("programs", 0) & PROGRAM_ALL_FLAGS', project
        )
        self.assertIn(
            "savedPrograms == 0 ? DEFAULT_SELECTED_PROGRAMS : savedPrograms",
            project,
        )
        for project_id in PROJECT_IDS:
            self.assertNotIn(project_id, project)
            self.assertNotIn(project_id.upper(), settings)


if __name__ == "__main__":
    unittest.main()
