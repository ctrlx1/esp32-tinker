#!/usr/bin/env python3
"""Tests for Stage 9 staging, publishing, and scoped releases."""

from __future__ import annotations

import json
import subprocess
import unittest
from pathlib import Path

from test_project_registry import RegistryFixture, project_entry

import sys

SCRIPTS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS_DIR))

from package_release import (  # noqa: E402
    publish_project,
    publish_staged_project,
    retain_docs_app_binaries,
    scoped_release_paths,
    stage_project,
    validate_staged_package,
)
from project_registry import RegistryError, load_registry  # noqa: E402
from project_tool import bump_project, deploy_project  # noqa: E402


def write_build_artifacts(project, payload_prefix: str = "") -> None:
    build_directory = project.path / ".pio" / "build" / "esp32dev"
    build_directory.mkdir(parents=True, exist_ok=True)
    for filename in ("bootloader.bin", "partitions.bin", "firmware.bin"):
        (build_directory / filename).write_bytes(
            (payload_prefix + filename).encode("ascii")
        )


def init_git_repo(root: Path) -> None:
    subprocess.run(["git", "init"], cwd=str(root), check=True, capture_output=True)
    subprocess.run(
        ["git", "config", "user.email", "test@example.com"],
        cwd=str(root),
        check=True,
        capture_output=True,
    )
    subprocess.run(
        ["git", "config", "user.name", "Test"],
        cwd=str(root),
        check=True,
        capture_output=True,
    )
    subprocess.run(["git", "add", "-A"], cwd=str(root), check=True, capture_output=True)
    subprocess.run(
        ["git", "commit", "-m", "init"],
        cwd=str(root),
        check=True,
        capture_output=True,
    )


class Stage9PackagingTests(unittest.TestCase):
    def test_real_projects_use_scoped_package_names_and_tags(self) -> None:
        registry = load_registry()
        justin = registry.project("justin")
        moon = registry.project("moon_phase")

        self.assertEqual("firmware_2.0.4.bin", justin.published_app_name)
        self.assertEqual("justin/v2.0.4", justin.release_tag())
        self.assertEqual("moon_phase_1.0.0.bin", moon.published_app_name)
        self.assertEqual("moon_phase/v1.0.0", moon.release_tag())
        self.assertEqual(
            justin.root / "dist" / "justin" / "2.0.4", justin.dist_dir
        )

    def test_stage_writes_project_qualified_package(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        path = fixture.write([project_entry("alpha", "/alpha/")])
        project = load_registry(path=path, root=fixture.root).project("alpha")
        write_build_artifacts(project)

        self.assertEqual(0, stage_project(project))

        metadata = json.loads(project.package_metadata_path.read_text(encoding="utf-8"))
        self.assertEqual("alpha", metadata["projectId"])
        self.assertEqual("1.0.0", metadata["version"])
        self.assertEqual("esp32dev", metadata["environment"])
        self.assertEqual("ESP32", metadata["chipFamily"])
        self.assertEqual("alpha_1.0.0.bin", metadata["artifacts"]["app"]["filename"])
        self.assertTrue((project.dist_dir / "alpha_1.0.0.bin").is_file())
        self.assertFalse((fixture.root / "dist" / "firmware.bin").exists())

    def test_publish_copies_only_the_selected_project_docs(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        path = fixture.write(
            [project_entry("alpha", "/alpha/"), project_entry("beta", "/beta/")]
        )
        registry = load_registry(path=path, root=fixture.root)
        alpha = registry.project("alpha")
        beta = registry.project("beta")
        write_build_artifacts(alpha, "alpha-")
        write_build_artifacts(beta, "beta-")
        self.assertEqual(0, stage_project(beta))

        self.assertEqual(0, publish_project(alpha))

        self.assertEqual(
            b"alpha-firmware.bin",
            (alpha.docs_path / "alpha_1.0.0.bin").read_bytes(),
        )
        self.assertFalse((beta.docs_path / "beta_1.0.0.bin").exists())
        self.assertTrue((beta.dist_dir / "beta_1.0.0.bin").is_file())

    def test_publish_rejects_package_from_another_project(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        path = fixture.write(
            [project_entry("alpha", "/alpha/"), project_entry("beta", "/beta/")]
        )
        registry = load_registry(path=path, root=fixture.root)
        alpha = registry.project("alpha")
        write_build_artifacts(alpha)
        self.assertEqual(0, stage_project(alpha))
        metadata = json.loads(alpha.package_metadata_path.read_text(encoding="utf-8"))
        metadata["projectId"] = "beta"
        alpha.package_metadata_path.write_text(
            json.dumps(metadata), encoding="utf-8"
        )

        with self.assertRaisesRegex(RegistryError, "belongs to 'beta'"):
            validate_staged_package(alpha)
        self.assertEqual(1, publish_staged_project(alpha))
        self.assertFalse(alpha.manifest_path.exists())

    def test_publish_rejects_chip_and_offset_mismatches(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        path = fixture.write([project_entry("alpha", "/alpha/")])
        project = load_registry(path=path, root=fixture.root).project("alpha")
        write_build_artifacts(project)
        self.assertEqual(0, stage_project(project))

        metadata = json.loads(project.package_metadata_path.read_text(encoding="utf-8"))
        metadata["chipFamily"] = "ESP32-S3"
        project.package_metadata_path.write_text(json.dumps(metadata), encoding="utf-8")
        with self.assertRaisesRegex(RegistryError, "staged chip"):
            validate_staged_package(project)

        metadata["chipFamily"] = "ESP32"
        metadata["artifacts"]["app"]["offset"] = 12345
        project.package_metadata_path.write_text(json.dumps(metadata), encoding="utf-8")
        with self.assertRaisesRegex(RegistryError, "offset"):
            validate_staged_package(project)

    def test_publish_rejects_hash_and_filename_tampering(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        path = fixture.write([project_entry("alpha", "/alpha/")])
        project = load_registry(path=path, root=fixture.root).project("alpha")
        write_build_artifacts(project)
        self.assertEqual(0, stage_project(project))
        (project.dist_dir / "alpha_1.0.0.bin").write_bytes(b"tampered")

        with self.assertRaisesRegex(RegistryError, "hash mismatch"):
            validate_staged_package(project)

        metadata = json.loads(project.package_metadata_path.read_text(encoding="utf-8"))
        metadata["artifacts"]["app"]["filename"] = "beta_1.0.0.bin"
        project.package_metadata_path.write_text(json.dumps(metadata), encoding="utf-8")
        with self.assertRaisesRegex(RegistryError, "filename"):
            validate_staged_package(project)

    def test_non_justin_publish_drops_superseded_app_binary(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        path = fixture.write([project_entry("alpha", "/alpha/")])
        project = load_registry(path=path, root=fixture.root).project("alpha")
        write_build_artifacts(project)
        self.assertEqual(0, publish_project(project))
        old_app = project.docs_path / "alpha_1.0.0.bin"
        self.assertTrue(old_app.is_file())

        bump_project(project, "patch")
        write_build_artifacts(project, "next-")
        self.assertEqual(0, publish_project(project))

        self.assertFalse(old_app.exists())
        self.assertTrue((project.docs_path / "alpha_1.0.1.bin").is_file())

    def test_justin_drops_superseded_firmware_bins_only(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        justin = project_entry("justin", "/")
        justin["docs"] = {
            "route": "/",
            "path": "docs",
            "manifest": "docs/manifest.json",
        }
        justin["artifacts"]["app"]["publishedName"] = "firmware_{version}.bin"
        path = fixture.write([justin, project_entry("alpha", "/alpha/")])
        project = load_registry(path=path, root=fixture.root).project("justin")
        sibling = load_registry(path=path, root=fixture.root).project("alpha")
        write_build_artifacts(project)
        self.assertEqual(0, publish_project(project))
        historical = project.docs_path / "firmware_1.9.0.bin"
        historical.write_bytes(b"legacy")
        sibling.docs_path.mkdir(parents=True)
        leftover = sibling.docs_path / "alpha_9.9.9.bin"
        leftover.write_bytes(b"keep")

        retain_docs_app_binaries(project)

        self.assertFalse(historical.exists())
        self.assertTrue(leftover.is_file())
        self.assertTrue((project.docs_path / "firmware_1.0.0.bin").is_file())

    def test_release_paths_stay_inside_one_project(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        justin = project_entry("justin", "/")
        justin["docs"] = {
            "route": "/",
            "path": "docs",
            "manifest": "docs/manifest.json",
        }
        justin["artifacts"]["app"]["publishedName"] = "firmware_{version}.bin"
        path = fixture.write([justin, project_entry("alpha", "/alpha/")])
        project = load_registry(path=path, root=fixture.root).project("justin")

        names = {path.name for path in scoped_release_paths(project)}
        self.assertEqual(
            {"VERSION", "bootloader.bin", "partitions.bin", "firmware_1.0.0.bin",
             "manifest.json"},
            names,
        )
        self.assertTrue(
            all("docs/alpha" not in str(path) for path in scoped_release_paths(project))
        )

    def test_deploy_restores_version_before_commit_on_failure(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        path = fixture.write([project_entry("alpha", "/alpha/")])
        project = load_registry(path=path, root=fixture.root).project("alpha")
        init_git_repo(fixture.root)

        result = deploy_project(project, "patch", build=lambda: 1)

        self.assertEqual(1, result)
        self.assertEqual("1.0.0", project.version)
        log = subprocess.run(
            ["git", "log", "--oneline"],
            cwd=str(fixture.root),
            capture_output=True,
            text=True,
            check=True,
        )
        self.assertNotIn("Release alpha", log.stdout)

    def test_deploy_tags_locally_without_pushing(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        path = fixture.write([project_entry("alpha", "/alpha/")])
        project = load_registry(path=path, root=fixture.root).project("alpha")
        write_build_artifacts(project)
        init_git_repo(fixture.root)

        result = deploy_project(project, "patch", build=lambda: 0)

        self.assertEqual(0, result)
        self.assertEqual("1.0.1", project.version)
        tags = subprocess.run(
            ["git", "tag"],
            cwd=str(fixture.root),
            capture_output=True,
            text=True,
            check=True,
        )
        self.assertEqual("alpha/v1.0.1\n", tags.stdout)
        remotes = subprocess.run(
            ["git", "remote"],
            cwd=str(fixture.root),
            capture_output=True,
            text=True,
            check=True,
        )
        self.assertEqual("", remotes.stdout)

    def test_deploy_rejects_existing_tag_without_bumping(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        path = fixture.write([project_entry("alpha", "/alpha/")])
        project = load_registry(path=path, root=fixture.root).project("alpha")
        init_git_repo(fixture.root)
        subprocess.run(
            ["git", "tag", "alpha/v1.0.1"],
            cwd=str(fixture.root),
            check=True,
            capture_output=True,
        )

        result = deploy_project(project, "patch", build=lambda: 0)

        self.assertEqual(1, result)
        self.assertEqual("1.0.0", project.version)


if __name__ == "__main__":
    unittest.main()
