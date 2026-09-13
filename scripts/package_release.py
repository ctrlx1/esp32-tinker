#!/usr/bin/env python3
"""Stage and publish project-scoped firmware packages."""

from __future__ import annotations

import hashlib
import json
import shutil
import sys
from pathlib import Path
from typing import Dict, List, Mapping

from project_registry import Project, RegistryError, SEMVER_RE


PACKAGE_SCHEMA_VERSION = 1
METADATA_NAME = "metadata.json"
INSTALLER_MANIFEST_NAME = "manifest.json"
def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def next_semver(version: str, increment: str) -> str:
    match = SEMVER_RE.fullmatch(version)
    if not match:
        raise RegistryError(f"version {version!r} is not strict semver")
    major, minor, patch = (int(part) for part in match.groups())
    if increment == "major":
        major, minor, patch = major + 1, 0, 0
    elif increment == "minor":
        minor, patch = minor + 1, 0
    else:
        patch += 1
    return f"{major}.{minor}.{patch}"


def restore_version(project: Project, version: str) -> None:
    project.version_file.write_text(version + "\n", encoding="utf-8")


def production_build_dir(project: Project) -> Path:
    environment = project.environment("production")
    if not environment:
        raise RegistryError(f"{project.id}: no production environment")
    return project.path / ".pio" / "build" / environment


def installer_manifest(project: Project, app_name: str) -> Dict[str, object]:
    return {
        "name": project.name,
        "version": project.version,
        "builds": [
            {
                "chipFamily": project.hardware["chipFamily"],
                "parts": [
                    {
                        "path": (
                            str(project.artifacts[name]["source"])
                            if name != "app"
                            else app_name
                        ),
                        "offset": project.artifacts[name]["offset"],
                    }
                    for name in ("bootloader", "partitions", "app")
                ],
            }
        ],
    }


def _artifact_record(
    filename: str, offset: int, source: Path
) -> Dict[str, object]:
    return {
        "filename": filename,
        "offset": offset,
        "size": source.stat().st_size,
        "sha256": sha256_file(source),
    }


def package_metadata(project: Project, sources: Mapping[str, Path]) -> Dict[str, object]:
    environment = project.environment("production")
    if not environment:
        raise RegistryError(f"{project.id}: no production environment")
    app_name = project.published_app_name
    return {
        "schemaVersion": PACKAGE_SCHEMA_VERSION,
        "projectId": project.id,
        "name": project.name,
        "version": project.version,
        "environment": environment,
        "chipFamily": project.hardware["chipFamily"],
        "hardwareProfile": project.hardware["profile"],
        "artifacts": {
            "bootloader": _artifact_record(
                str(project.artifacts["bootloader"]["source"]),
                int(project.artifacts["bootloader"]["offset"]),
                sources["bootloader"],
            ),
            "partitions": _artifact_record(
                str(project.artifacts["partitions"]["source"]),
                int(project.artifacts["partitions"]["offset"]),
                sources["partitions"],
            ),
            "app": _artifact_record(
                app_name,
                int(project.artifacts["app"]["offset"]),
                sources["app"],
            ),
        },
        "manifest": INSTALLER_MANIFEST_NAME,
    }


def stage_project(project: Project) -> int:
    if not project.buildable:
        print(f"{project.id}: project is not stageable", file=sys.stderr)
        return 1
    try:
        build_directory = production_build_dir(project)
    except RegistryError as exc:
        print(exc, file=sys.stderr)
        return 1

    sources = {}
    for name in ("bootloader", "partitions", "app"):
        source = build_directory / str(project.artifacts[name]["source"])
        if not source.is_file():
            print(
                f"{project.id}: missing {source}; build production first",
                file=sys.stderr,
            )
            return 1
        sources[name] = source

    destination = project.dist_dir
    if destination.exists():
        shutil.rmtree(destination)
    destination.mkdir(parents=True)

    shutil.copy2(sources["bootloader"], destination / sources["bootloader"].name)
    shutil.copy2(sources["partitions"], destination / sources["partitions"].name)
    shutil.copy2(sources["app"], destination / project.published_app_name)

    metadata = package_metadata(project, sources)
    (destination / METADATA_NAME).write_text(
        json.dumps(metadata, indent=2) + "\n", encoding="utf-8"
    )
    (destination / INSTALLER_MANIFEST_NAME).write_text(
        json.dumps(installer_manifest(project, project.published_app_name), indent=2)
        + "\n",
        encoding="utf-8",
    )
    print(f"{project.id}: staged {project.version} at dist/{project.id}/{project.version}")
    return 0


def _require_mapping(value: object, field: str) -> Mapping[str, object]:
    if not isinstance(value, dict):
        raise RegistryError(f"{field} must be an object")
    return value


def _require_string(value: object, field: str) -> str:
    if not isinstance(value, str) or not value:
        raise RegistryError(f"{field} must be a non-empty string")
    return value


def _require_int(value: object, field: str) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or value < 0:
        raise RegistryError(f"{field} must be a non-negative integer")
    return value


def load_package_metadata(project: Project) -> Mapping[str, object]:
    path = project.package_metadata_path
    if not path.is_file():
        raise RegistryError(
            f"{project.id}: missing staged package {path}; stage first"
        )
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise RegistryError(f"{project.id}: invalid package metadata: {exc.msg}") from exc
    return _require_mapping(data, f"{project.id} package metadata")


def validate_staged_package(project: Project) -> Mapping[str, object]:
    if not project.buildable:
        raise RegistryError(f"{project.id}: project is not publishable")
    environment = project.environment("production")
    if not environment:
        raise RegistryError(f"{project.id}: no production environment")

    metadata = load_package_metadata(project)
    if metadata.get("schemaVersion") != PACKAGE_SCHEMA_VERSION:
        raise RegistryError(f"{project.id}: unsupported package schema")

    staged_id = _require_string(metadata.get("projectId"), "projectId")
    if staged_id != project.id:
        raise RegistryError(
            f"{project.id}: staged package belongs to {staged_id!r}"
        )

    staged_version = _require_string(metadata.get("version"), "version")
    if staged_version != project.version:
        raise RegistryError(
            f"{project.id}: staged version {staged_version} does not match "
            f"{project.version}"
        )
    if metadata.get("environment") != environment:
        raise RegistryError(
            f"{project.id}: staged environment {metadata.get('environment')!r} "
            f"does not match {environment!r}"
        )
    if metadata.get("chipFamily") != project.hardware["chipFamily"]:
        raise RegistryError(
            f"{project.id}: staged chip {metadata.get('chipFamily')!r} does not "
            f"match {project.hardware['chipFamily']!r}"
        )

    artifacts = _require_mapping(metadata.get("artifacts"), "artifacts")
    expected_names = {
        "bootloader": str(project.artifacts["bootloader"]["source"]),
        "partitions": str(project.artifacts["partitions"]["source"]),
        "app": project.published_app_name,
    }
    for name in ("bootloader", "partitions", "app"):
        artifact = _require_mapping(artifacts.get(name), name)
        filename = _require_string(artifact.get("filename"), f"{name}.filename")
        if filename != expected_names[name]:
            raise RegistryError(
                f"{project.id}: staged {name} filename {filename!r} does not "
                f"match {expected_names[name]!r}"
            )
        if ".." in Path(filename).parts or Path(filename).is_absolute():
            raise RegistryError(f"{project.id}: staged {name} filename escapes package")
        offset = _require_int(artifact.get("offset"), f"{name}.offset")
        if offset != int(project.artifacts[name]["offset"]):
            raise RegistryError(
                f"{project.id}: staged {name} offset {offset} does not match "
                f"{project.artifacts[name]['offset']}"
            )
        package_file = project.dist_dir / filename
        if not package_file.is_file():
            raise RegistryError(f"{project.id}: missing staged file {filename}")
        digest = _require_string(artifact.get("sha256"), f"{name}.sha256")
        if sha256_file(package_file) != digest:
            raise RegistryError(f"{project.id}: staged {name} hash mismatch")
        size = _require_int(artifact.get("size"), f"{name}.size")
        if package_file.stat().st_size != size:
            raise RegistryError(f"{project.id}: staged {name} size mismatch")
    return metadata


def scoped_release_paths(project: Project) -> List[Path]:
    return [
        project.version_file,
        project.docs_path / str(project.artifacts["bootloader"]["source"]),
        project.docs_path / str(project.artifacts["partitions"]["source"]),
        project.docs_path / project.published_app_name,
        project.manifest_path,
    ]


def superseded_app_glob(project: Project) -> str:
    return (
        str(project.artifacts["app"]["publishedName"])
        .replace("{project}", project.id)
        .replace("{version}", "*")
    )


def retain_docs_app_binaries(project: Project) -> None:
    current = project.published_app_name
    for path in project.docs_path.glob(superseded_app_glob(project)):
        if not path.is_file() or path.name == current:
            continue
        if path.parent != project.docs_path:
            continue
        path.unlink()
        print(f"{project.id}: removed superseded {path.name}")


def publish_staged_project(project: Project) -> int:
    try:
        metadata = validate_staged_package(project)
    except RegistryError as exc:
        print(exc, file=sys.stderr)
        return 1

    artifacts = metadata["artifacts"]
    assert isinstance(artifacts, dict)
    project.docs_path.mkdir(parents=True, exist_ok=True)
    for name in ("bootloader", "partitions", "app"):
        filename = str(artifacts[name]["filename"])
        shutil.copy2(project.dist_dir / filename, project.docs_path / filename)

    staged_manifest = project.dist_dir / INSTALLER_MANIFEST_NAME
    if staged_manifest.is_file():
        shutil.copy2(staged_manifest, project.manifest_path)
    else:
        project.manifest_path.write_text(
            json.dumps(installer_manifest(project, project.published_app_name), indent=2)
            + "\n",
            encoding="utf-8",
        )

    retain_docs_app_binaries(project)
    print(f"{project.id}: published {project.version} to {project.docs['path']}")
    return 0


def publish_project(project: Project) -> int:
    result = stage_project(project)
    if result:
        return result
    return publish_staged_project(project)


def path_is_scoped_release(project: Project, path: Path) -> bool:
    allowed = {item.resolve() for item in scoped_release_paths(project)}
    return path.resolve() in allowed
