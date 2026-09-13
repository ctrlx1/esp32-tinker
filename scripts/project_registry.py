#!/usr/bin/env python3
"""Load and validate the firmware project registry."""

from __future__ import annotations

import json
import re
from dataclasses import dataclass
from pathlib import Path
from string import Formatter
from typing import Any, Dict, Iterable, List, Mapping, Optional


REPO_ROOT = Path(__file__).resolve().parents[1]
PROJECT_ID_RE = re.compile(r"^[a-z0-9]+(?:[-_][a-z0-9]+)*$")
SEMVER_RE = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$")


class RegistryError(ValueError):
    """Raised when projects.json is invalid."""


def _relative_path(value: Any, field: str) -> Path:
    if not isinstance(value, str) or not value:
        raise RegistryError(f"{field} must be a non-empty relative path")
    path = Path(value)
    if path.is_absolute() or ".." in path.parts:
        raise RegistryError(f"{field} must stay inside the repository: {value!r}")
    return path


def _string(value: Any, field: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise RegistryError(f"{field} must be a non-empty string")
    return value


@dataclass(frozen=True)
class Project:
    """A validated firmware project entry."""

    root: Path
    data: Mapping[str, Any]

    @property
    def id(self) -> str:
        return str(self.data["id"])

    @property
    def name(self) -> str:
        return str(self.data["name"])

    @property
    def path(self) -> Path:
        return self.root / str(self.data["path"])

    @property
    def relative_path(self) -> Path:
        return Path(str(self.data["path"]))

    @property
    def version_file(self) -> Path:
        return self.root / str(self.data["versionFile"])

    @property
    def relative_version_file(self) -> Path:
        return Path(str(self.data["versionFile"]))

    @property
    def version(self) -> str:
        return self.version_file.read_text(encoding="utf-8").strip()

    @property
    def buildable(self) -> bool:
        return bool(self.data.get("buildable", True))

    @property
    def environments(self) -> Mapping[str, str]:
        return self.data["environments"]  # type: ignore[return-value]

    @property
    def hardware(self) -> Mapping[str, Any]:
        return self.data["hardware"]  # type: ignore[return-value]

    @property
    def pre_build(self) -> List[List[str]]:
        return [list(command) for command in self.data.get("preBuild", [])]

    @property
    def dependencies(self) -> Mapping[str, object]:
        return self.data.get("dependencies", {})  # type: ignore[return-value]

    @property
    def docs(self) -> Mapping[str, str]:
        return self.data["docs"]  # type: ignore[return-value]

    @property
    def docs_path(self) -> Path:
        return self.root / self.docs["path"]

    @property
    def manifest_path(self) -> Path:
        manifest = self.docs.get("manifest")
        if not manifest:
            raise RegistryError(f"{self.id}: project has no publish manifest")
        return self.root / manifest

    @property
    def artifacts(self) -> Mapping[str, Mapping[str, Any]]:
        return self.data.get("artifacts", {})  # type: ignore[return-value]

    def environment(self, mode: str) -> Optional[str]:
        return self.environments.get(mode)


@dataclass(frozen=True)
class Registry:
    """Validated project registry."""

    root: Path
    projects: List[Project]

    def project(self, project_id: str) -> Project:
        for project in self.projects:
            if project.id == project_id:
                return project
        known = ", ".join(project.id for project in self.projects)
        raise RegistryError(f"unknown project {project_id!r}; choose one of: {known}")

    def select(self, target: str) -> List[Project]:
        if target == "all":
            return list(self.projects)
        return [self.project(target)]


def _validate_project(root: Path, raw: Any, index: int) -> Project:
    label = f"projects[{index}]"
    if not isinstance(raw, dict):
        raise RegistryError(f"{label} must be an object")

    project_id = _string(raw.get("id"), f"{label}.id")
    if not PROJECT_ID_RE.fullmatch(project_id):
        raise RegistryError(
            f"{label}.id must use lowercase letters, digits, hyphens, or underscores"
        )
    _string(raw.get("name"), f"{label}.name")

    buildable = raw.get("buildable", True)
    if not isinstance(buildable, bool):
        raise RegistryError(f"{label}.buildable must be a boolean")

    project_path = _relative_path(raw.get("path"), f"{label}.path")
    version_path = _relative_path(raw.get("versionFile"), f"{label}.versionFile")
    try:
        version_path.relative_to(project_path)
    except ValueError as exc:
        raise RegistryError(
            f"{label}.versionFile must be inside the project path"
        ) from exc
    if not (root / project_path).is_dir():
        raise RegistryError(f"{label}.path does not exist: {project_path}")
    if buildable and not (root / project_path / "platformio.ini").is_file():
        raise RegistryError(f"{label}.path has no platformio.ini: {project_path}")
    if not (root / version_path).is_file():
        raise RegistryError(f"{label}.versionFile does not exist: {version_path}")
    version = (root / version_path).read_text(encoding="utf-8").strip()
    if not SEMVER_RE.fullmatch(version):
        raise RegistryError(f"{label}.versionFile must contain strict semver: {version!r}")

    environments = raw.get("environments")
    if not isinstance(environments, dict):
        raise RegistryError(f"{label}.environments must be an object")
    for mode, environment in environments.items():
        _string(mode, f"{label}.environments key")
        _string(environment, f"{label}.environments.{mode}")
    if buildable and "production" not in environments:
        raise RegistryError(f"{label}.environments must define production")

    hardware = raw.get("hardware")
    if not isinstance(hardware, dict):
        raise RegistryError(f"{label}.hardware must be an object")
    status = _string(hardware.get("status"), f"{label}.hardware.status")
    if status not in {"configured", "unassigned"}:
        raise RegistryError(
            f"{label}.hardware.status must be 'configured' or 'unassigned'"
        )
    if buildable and status != "configured":
        raise RegistryError(f"{label} cannot be buildable with unassigned hardware")
    if not buildable and status != "unassigned":
        raise RegistryError(
            f"{label} must use unassigned hardware when buildable is false"
        )
    if not buildable and environments:
        raise RegistryError(
            f"{label}.environments must be empty when buildable is false"
        )
    if status == "configured":
        _string(hardware.get("profile"), f"{label}.hardware.profile")
        _string(hardware.get("chipFamily"), f"{label}.hardware.chipFamily")

    pre_build = raw.get("preBuild", [])
    if not isinstance(pre_build, list):
        raise RegistryError(f"{label}.preBuild must be an array")
    for command_index, command in enumerate(pre_build):
        if (
            not isinstance(command, list)
            or not command
            or any(not isinstance(part, str) or not part for part in command)
        ):
            raise RegistryError(
                f"{label}.preBuild[{command_index}] must be a non-empty string array"
            )

    dependencies = raw.get("dependencies", {})
    if not isinstance(dependencies, dict):
        raise RegistryError(f"{label}.dependencies must be an object")
    for dependency_type in ("commands", "pythonModules"):
        values = dependencies.get(dependency_type, [])
        if not isinstance(values, list) or any(
            not isinstance(value, str) or not value for value in values
        ):
            raise RegistryError(
                f"{label}.dependencies.{dependency_type} must be a string array"
            )
    conditional_modules = dependencies.get("conditionalPythonModules", [])
    if not isinstance(conditional_modules, list):
        raise RegistryError(
            f"{label}.dependencies.conditionalPythonModules must be an array"
        )
    for module_index, dependency in enumerate(conditional_modules):
        dependency_label = (
            f"{label}.dependencies.conditionalPythonModules[{module_index}]"
        )
        if not isinstance(dependency, dict):
            raise RegistryError(f"{dependency_label} must be an object")
        _string(dependency.get("module"), f"{dependency_label}.module")
        _relative_path(dependency.get("whenPath"), f"{dependency_label}.whenPath")

    docs = raw.get("docs")
    if not isinstance(docs, dict):
        raise RegistryError(f"{label}.docs must be an object")
    route = _string(docs.get("route"), f"{label}.docs.route")
    if not route.startswith("/"):
        raise RegistryError(f"{label}.docs.route must start with '/'")
    docs_path = _relative_path(docs.get("path"), f"{label}.docs.path")
    manifest_path: Optional[Path] = None
    if buildable or docs.get("manifest") is not None:
        manifest_path = _relative_path(
            docs.get("manifest"), f"{label}.docs.manifest"
        )
        try:
            manifest_relative = manifest_path.relative_to(docs_path)
        except ValueError as exc:
            raise RegistryError(
                f"{label}.docs.manifest must be inside docs.path"
            ) from exc
        if not manifest_relative.parts:
            raise RegistryError(
                f"{label}.docs.manifest must name a file inside docs.path"
            )

    artifacts = raw.get("artifacts")
    if not buildable and artifacts is None:
        return Project(root=root, data=raw)
    if not isinstance(artifacts, dict):
        raise RegistryError(f"{label}.artifacts must be an object")
    for artifact_name in ("bootloader", "partitions", "app"):
        artifact = artifacts.get(artifact_name)
        if not isinstance(artifact, dict):
            raise RegistryError(f"{label}.artifacts.{artifact_name} must be an object")
        source = _relative_path(
            artifact.get("source"), f"{label}.artifacts.{artifact_name}.source"
        )
        if len(source.parts) != 1:
            raise RegistryError(
                f"{label}.artifacts.{artifact_name}.source must be a filename"
            )
        offset = artifact.get("offset")
        if not isinstance(offset, int) or isinstance(offset, bool) or offset < 0:
            raise RegistryError(
                f"{label}.artifacts.{artifact_name}.offset must be a non-negative integer"
            )
    published_name = _string(
        artifacts["app"].get("publishedName"),
        f"{label}.artifacts.app.publishedName",
    )
    if "{version}" not in published_name:
        raise RegistryError(
            f"{label}.artifacts.app.publishedName must contain '{{version}}'"
        )
    try:
        fields = list(Formatter().parse(published_name))
    except ValueError as exc:
        raise RegistryError(
            f"{label}.artifacts.app.publishedName has an invalid placeholder"
        ) from exc
    for _, field_name, format_spec, conversion in fields:
        if field_name is not None and (
            field_name not in {"version", "project"} or format_spec or conversion
        ):
            raise RegistryError(
                f"{label}.artifacts.app.publishedName supports only "
                "'{project}' and '{version}' placeholders"
            )
    try:
        rendered_name = published_name.format(version=version, project=project_id)
    except (IndexError, KeyError, ValueError) as exc:
        raise RegistryError(
            f"{label}.artifacts.app.publishedName has an invalid placeholder"
        ) from exc
    rendered_path = _relative_path(
        rendered_name, f"{label}.artifacts.app.publishedName"
    )
    if len(rendered_path.parts) != 1:
        raise RegistryError(
            f"{label}.artifacts.app.publishedName must render to a filename"
        )
    artifact_destinations = {
        docs_path / str(artifacts[name]["source"])
        for name in ("bootloader", "partitions")
    }
    artifact_destinations.add(docs_path / rendered_name)
    if manifest_path is not None and manifest_path in artifact_destinations:
        raise RegistryError(
            f"{label}.docs.manifest must not collide with a published artifact"
        )

    return Project(root=root, data=raw)


def load_registry(
    path: Optional[Path] = None, root: Optional[Path] = None
) -> Registry:
    """Load projects.json and validate all project entries."""

    repository_root = (root or REPO_ROOT).resolve()
    registry_path = path or repository_root / "projects.json"
    try:
        data: Dict[str, Any] = json.loads(
            registry_path.read_text(encoding="utf-8")
        )
    except FileNotFoundError as exc:
        raise RegistryError(f"project registry not found: {registry_path}") from exc
    except json.JSONDecodeError as exc:
        raise RegistryError(
            f"invalid JSON in {registry_path}: line {exc.lineno}: {exc.msg}"
        ) from exc

    if not isinstance(data, dict):
        raise RegistryError("project registry must be an object")
    if data.get("schemaVersion") != 1:
        raise RegistryError("projects.json schemaVersion must be 1")
    raw_projects = data.get("projects")
    if not isinstance(raw_projects, list) or not raw_projects:
        raise RegistryError("projects.json projects must be a non-empty array")

    projects = [
        _validate_project(repository_root, raw, index)
        for index, raw in enumerate(raw_projects)
    ]
    _require_unique((project.id for project in projects), "project id")
    _require_unique(
        (str(project.relative_path) for project in projects), "project path"
    )
    _require_unique(
        (str(project.relative_version_file) for project in projects), "version file"
    )
    _require_unique((project.docs["route"] for project in projects), "docs route")
    _require_unique((project.docs["path"] for project in projects), "docs path")
    return Registry(root=repository_root, projects=projects)


def _require_unique(values: Iterable[str], label: str) -> None:
    seen = set()
    for value in values:
        if value in seen:
            raise RegistryError(f"duplicate {label}: {value}")
        seen.add(value)
