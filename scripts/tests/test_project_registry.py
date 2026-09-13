#!/usr/bin/env python3
"""Tests for the manifest-driven project registry."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from copy import deepcopy
from pathlib import Path


SCRIPTS_DIR = Path(__file__).resolve().parents[1]
REPO_ROOT = SCRIPTS_DIR.parent
sys.path.insert(0, str(SCRIPTS_DIR))

from project_registry import RegistryError, load_registry  # noqa: E402
from project_tool import (  # noqa: E402
    bump_project,
    create_parser,
    deploy_project,
    publish_project,
)


def project_entry(project_id: str, route: str = "/") -> dict:
    return {
        "id": project_id,
        "name": project_id.title(),
        "path": f"firmware/{project_id}",
        "versionFile": f"firmware/{project_id}/VERSION",
        "buildable": True,
        "environments": {"production": "esp32dev", "wokwi": "wokwi"},
        "hardware": {
            "status": "configured",
            "profile": "test-profile",
            "chipFamily": "ESP32",
        },
        "preBuild": [],
        "dependencies": {"commands": [], "pythonModules": []},
        "docs": {
            "route": route,
            "path": f"docs/{project_id}",
            "manifest": f"docs/{project_id}/manifest.json",
        },
        "artifacts": {
            "bootloader": {"source": "bootloader.bin", "offset": 4096},
            "partitions": {"source": "partitions.bin", "offset": 32768},
            "app": {
                "source": "firmware.bin",
                "offset": 65536,
                "publishedName": f"{project_id}_{{version}}.bin",
            },
        },
    }


class RegistryFixture:
    def __init__(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)

    def close(self) -> None:
        self.temporary_directory.cleanup()

    def write(self, projects: list) -> Path:
        for project in projects:
            project_path = self.root / project["path"]
            project_path.mkdir(parents=True, exist_ok=True)
            (project_path / "platformio.ini").write_text(
                "[env:esp32dev]\n[env:wokwi]\n", encoding="utf-8"
            )
            version_path = self.root / project["versionFile"]
            version_path.parent.mkdir(parents=True, exist_ok=True)
            version_path.write_text("1.0.0\n", encoding="utf-8")
        registry_path = self.root / "projects.json"
        registry_path.write_text(
            json.dumps({"schemaVersion": 1, "projects": projects}),
            encoding="utf-8",
        )
        return registry_path


class ProjectRegistryTests(unittest.TestCase):
    def test_repository_registry_is_valid(self) -> None:
        registry = load_registry()
        self.assertEqual(["justin"], [project.id for project in registry.projects])

    def test_loads_valid_registry(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        path = fixture.write([project_entry("alpha")])

        registry = load_registry(path=path, root=fixture.root)

        self.assertEqual("alpha", registry.project("alpha").id)
        self.assertEqual("1.0.0", registry.project("alpha").version)

    def test_validates_conditional_python_dependencies(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        project = project_entry("alpha")
        project["dependencies"]["conditionalPythonModules"] = [
            {"module": "PIL", "whenPath": "firmware/alpha/assets"}
        ]
        path = fixture.write([project])

        registry = load_registry(path=path, root=fixture.root)

        self.assertEqual(
            "PIL",
            registry.project("alpha")
            .dependencies["conditionalPythonModules"][0]["module"],
        )

    def test_rejects_duplicate_project_ids(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        first = project_entry("alpha", "/alpha/")
        second = project_entry("beta", "/beta/")
        second["id"] = "alpha"
        path = fixture.write([first, second])

        with self.assertRaisesRegex(RegistryError, "duplicate project id"):
            load_registry(path=path, root=fixture.root)

    def test_rejects_duplicate_docs_routes(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        path = fixture.write(
            [project_entry("alpha", "/same/"), project_entry("beta", "/same/")]
        )

        with self.assertRaisesRegex(RegistryError, "duplicate docs route"):
            load_registry(path=path, root=fixture.root)

    def test_selects_one_project_or_all_projects(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        path = fixture.write(
            [project_entry("alpha", "/alpha/"), project_entry("beta", "/beta/")]
        )
        registry = load_registry(path=path, root=fixture.root)

        self.assertEqual(["alpha"], [project.id for project in registry.select("alpha")])
        self.assertEqual(
            ["alpha", "beta"], [project.id for project in registry.select("all")]
        )
        self.assertEqual("wokwi", registry.project("alpha").environment("wokwi"))

    def test_rejects_invalid_version(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        project = project_entry("alpha")
        path = fixture.write([project])
        (fixture.root / project["versionFile"]).write_text(
            "version-one\n", encoding="utf-8"
        )

        with self.assertRaisesRegex(RegistryError, "strict semver"):
            load_registry(path=path, root=fixture.root)

    def test_rejects_version_file_outside_project(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        project = project_entry("alpha")
        project["versionFile"] = "shared/VERSION"
        path = fixture.write([project])

        with self.assertRaisesRegex(RegistryError, "inside the project path"):
            load_registry(path=path, root=fixture.root)

    def test_rejects_path_outside_repository(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        project = project_entry("alpha")
        invalid = deepcopy(project)
        invalid["path"] = "../alpha"
        path = fixture.write([project])
        data = json.loads(path.read_text(encoding="utf-8"))
        data["projects"] = [invalid]
        path.write_text(json.dumps(data), encoding="utf-8")

        with self.assertRaisesRegex(RegistryError, "inside the repository"):
            load_registry(path=path, root=fixture.root)

    def test_rejects_published_name_outside_docs(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        project = project_entry("alpha")
        project["artifacts"]["app"]["publishedName"] = "../../alpha_{version}.bin"
        path = fixture.write([project])

        with self.assertRaisesRegex(RegistryError, "must stay inside"):
            load_registry(path=path, root=fixture.root)

    def test_rejects_unknown_published_name_placeholder(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        project = project_entry("alpha")
        project["artifacts"]["app"]["publishedName"] = (
            "alpha_{unknown}_{version}.bin"
        )
        path = fixture.write([project])

        with self.assertRaisesRegex(RegistryError, "supports only"):
            load_registry(path=path, root=fixture.root)

    def test_rejects_positional_published_name_placeholder(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        project = project_entry("alpha")
        project["artifacts"]["app"]["publishedName"] = "alpha_{}_{version}.bin"
        path = fixture.write([project])

        with self.assertRaisesRegex(RegistryError, "supports only"):
            load_registry(path=path, root=fixture.root)

    def test_rejects_manifest_outside_docs_path(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        project = project_entry("alpha")
        project["docs"]["manifest"] = "platformio.ini"
        path = fixture.write([project])

        with self.assertRaisesRegex(RegistryError, "inside docs.path"):
            load_registry(path=path, root=fixture.root)

    def test_rejects_docs_path_as_manifest(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        project = project_entry("alpha")
        project["docs"]["manifest"] = project["docs"]["path"]
        path = fixture.write([project])

        with self.assertRaisesRegex(RegistryError, "name a file"):
            load_registry(path=path, root=fixture.root)

    def test_rejects_manifest_artifact_collision(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        project = project_entry("alpha")
        project["docs"]["manifest"] = "docs/alpha/bootloader.bin"
        path = fixture.write([project])

        with self.assertRaisesRegex(RegistryError, "collide"):
            load_registry(path=path, root=fixture.root)

    def test_rejects_manifest_versioned_app_collision(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        project = project_entry("alpha")
        project["docs"]["manifest"] = "docs/alpha/alpha_1.0.0.bin"
        path = fixture.write([project])

        with self.assertRaisesRegex(RegistryError, "collide"):
            load_registry(path=path, root=fixture.root)

    def test_rejects_malformed_json(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        path = fixture.root / "projects.json"
        path.write_text('{"schemaVersion": 1,', encoding="utf-8")

        with self.assertRaisesRegex(RegistryError, "invalid JSON"):
            load_registry(path=path, root=fixture.root)

    def test_accepts_hardware_unassigned_template(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        template = project_entry("starter-template", "/starter-template/")
        template["buildable"] = False
        template["environments"] = {}
        template["hardware"] = {"status": "unassigned"}
        template["docs"].pop("manifest")
        template.pop("artifacts")
        path = fixture.write([template])
        (fixture.root / template["path"] / "platformio.ini").unlink()

        project = load_registry(path=path, root=fixture.root).project(
            "starter-template"
        )

        self.assertFalse(project.buildable)
        self.assertEqual({}, project.environments)

    def test_bumps_only_selected_project_version(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        path = fixture.write([project_entry("alpha")])
        project = load_registry(path=path, root=fixture.root).project("alpha")

        new_version = bump_project(project, "minor")

        self.assertEqual("1.1.0", new_version)
        self.assertEqual("1.1.0\n", project.version_file.read_text(encoding="utf-8"))

    def test_publishes_artifacts_and_manifest(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        path = fixture.write([project_entry("alpha")])
        project = load_registry(path=path, root=fixture.root).project("alpha")
        build_directory = project.path / ".pio" / "build" / "esp32dev"
        build_directory.mkdir(parents=True)
        for filename in ("bootloader.bin", "partitions.bin", "firmware.bin"):
            (build_directory / filename).write_bytes(filename.encode("ascii"))

        result = publish_project(project)

        self.assertEqual(0, result)
        manifest = json.loads(project.manifest_path.read_text(encoding="utf-8"))
        self.assertEqual("alpha_1.0.0.bin", manifest["builds"][0]["parts"][2]["path"])
        self.assertEqual(
            b"firmware.bin",
            (project.docs_path / "alpha_1.0.0.bin").read_bytes(),
        )

    def test_deploy_rejects_template_without_bumping_version(self) -> None:
        fixture = RegistryFixture()
        self.addCleanup(fixture.close)
        template = project_entry("starter-template", "/starter-template/")
        template["buildable"] = False
        template["environments"] = {}
        template["hardware"] = {"status": "unassigned"}
        template["docs"].pop("manifest")
        template.pop("artifacts")
        path = fixture.write([template])
        (fixture.root / template["path"] / "platformio.ini").unlink()
        project = load_registry(path=path, root=fixture.root).project(
            "starter-template"
        )

        result = deploy_project(project, "patch")

        self.assertEqual(1, result)
        self.assertEqual("1.0.0", project.version)

    def test_build_cli_maps_environment_targets_and_verbose(self) -> None:
        args = create_parser().parse_args(
            [
                "build",
                "all",
                "--env",
                "wokwi",
                "--target",
                "clean",
                "--target",
                "compiledb",
                "--verbose",
            ]
        )

        self.assertEqual("all", args.target)
        self.assertEqual("wokwi", args.env)
        self.assertEqual(["clean", "compiledb"], args.build_targets)
        self.assertTrue(args.verbose)

    def test_legacy_build_wrapper_help(self) -> None:
        result = subprocess.run(
            [str(REPO_ROOT / "scripts" / "build-firmware.sh"), "--help"],
            capture_output=True,
            text=True,
            check=False,
        )

        self.assertEqual(0, result.returncode)
        self.assertIn("./scripts/build.sh justin", result.stdout)


if __name__ == "__main__":
    unittest.main()
