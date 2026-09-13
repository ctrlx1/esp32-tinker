#!/usr/bin/env python3
"""Manifest-driven firmware project commands."""

from __future__ import annotations

import argparse
import importlib.util
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import List, Optional, Sequence

from package_release import (
    next_semver,
    path_is_scoped_release,
    publish_project,
    restore_version,
    scoped_release_paths,
    stage_project,
)
from project_registry import Project, RegistryError, load_registry


ROOT = Path(__file__).resolve().parents[1]


def find_platformio() -> Optional[Path]:
    executable = shutil.which("pio")
    if executable:
        return Path(executable)
    for candidate in (
        Path.home() / ".platformio" / "penv" / "bin" / "pio",
        Path.home() / ".local" / "bin" / "pio",
    ):
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate
    return None


def find_extension(pattern: str) -> Optional[Path]:
    for directory in (
        Path.home() / ".cursor" / "extensions",
        Path.home() / ".vscode" / "extensions",
        Path.home() / ".vscode-oss" / "extensions",
    ):
        if not directory.is_dir():
            continue
        matches = sorted(path for path in directory.glob(pattern) if path.is_dir())
        if matches:
            return matches[-1]
    return None


def run(command: Sequence[str], cwd: Path = ROOT) -> int:
    print("+ " + " ".join(command), flush=True)
    return subprocess.run(list(command), cwd=str(cwd), check=False).returncode


def selected_projects(target: str, root: Optional[Path] = None) -> List[Project]:
    return load_registry(root=root or ROOT).select(target)


def environment_for(project: Project, mode: str, target: str) -> Optional[str]:
    environment = project.environment(mode)
    if environment:
        return environment
    message = f"{project.id}: environment {mode!r} is not supported"
    if target == "all":
        print(f"skip  {message}")
        return None
    raise RegistryError(message)


def run_pre_build(project: Project) -> int:
    for command in project.pre_build:
        result = run(command)
        if result:
            print(f"{project.id}: pre-build command failed", file=sys.stderr)
            return result
    return 0


def build_projects(
    target: str,
    mode: str,
    build_targets: Sequence[str],
    verbose: bool = False,
    root: Optional[Path] = None,
) -> int:
    unsafe_aggregate_targets = {"erase", "upload", "uploadfs", "uploadfsota"}
    requested_unsafe_targets = unsafe_aggregate_targets.intersection(build_targets)
    if target == "all" and requested_unsafe_targets:
        names = ", ".join(sorted(requested_unsafe_targets))
        print(
            f"Cannot run device target(s) {names} for all projects; "
            "select one project.",
            file=sys.stderr,
        )
        return 2

    pio = find_platformio()
    if not pio:
        print(
            "PlatformIO not found. Run ./scripts/check-deps.sh first.",
            file=sys.stderr,
        )
        return 1

    for project in selected_projects(target, root=root):
        if not project.buildable:
            message = f"{project.id}: hardware is unassigned; project is not buildable"
            if target == "all":
                print(f"skip  {message}")
                continue
            print(message, file=sys.stderr)
            return 1

        environment = environment_for(project, mode, target)
        if not environment:
            continue
        print(f"\nBuilding {project.id} ({mode}: {environment})")
        result = run_pre_build(project)
        if result:
            return result
        command = [str(pio), "run", "-d", str(project.path), "-e", environment]
        if verbose:
            command.append("-v")
        for build_target in build_targets:
            command.extend(["-t", build_target])
        result = run(command)
        if result:
            return result
        if mode == "production":
            result = stage_project(project)
            if result:
                return result
    return 0


def _configured_environments(project: Project) -> set:
    text = (project.path / "platformio.ini").read_text(encoding="utf-8")
    return set(re.findall(r"^\[env:([^\]]+)\]\s*$", text, flags=re.MULTILINE))


def check_dependencies(target: str, mode: Optional[str]) -> int:
    failed = False
    print("Checking shared dependencies...\n")

    print(f"  ok    python3          {sys.version.split()[0]} ({sys.executable})")
    pio = find_platformio()
    if pio:
        version = subprocess.run(
            [str(pio), "--version"], capture_output=True, text=True, check=False
        )
        print(f"  ok    PlatformIO CLI   {version.stdout.strip()} ({pio})")
    else:
        failed = True
        print("  miss  PlatformIO CLI   not found")
        print("         Install with: pipx install platformio")
        print("         Or install the PlatformIO IDE extension.")

    optional_ide = find_extension("*platformio-ide*")
    if optional_ide:
        print(f"  ok    PlatformIO IDE   {optional_ide}")
    else:
        print("  warn  PlatformIO IDE   not found (CLI is enough to build)")

    projects = selected_projects(target)
    needs_wokwi = False
    for project in projects:
        print(f"\nChecking project {project.id} ({project.version})...")
        project_failed = False
        print(f"  ok    project path     {project.relative_path}")
        print(f"  ok    version          {project.relative_version_file}")

        if project.hardware["status"] == "configured":
            print(f"  ok    hardware         {project.hardware['profile']}")
        elif project.buildable:
            project_failed = True
            print("  miss  hardware         unassigned for buildable project")
        else:
            print("  skip  hardware         unassigned template")

        configured_environments = (
            _configured_environments(project) if project.buildable else set()
        )
        modes = [mode] if mode else list(project.environments)
        for requested_mode in modes:
            environment = project.environment(requested_mode)
            if not environment:
                if not project.buildable:
                    print(f"  skip  {requested_mode:<16} hardware unassigned")
                elif target != "all":
                    project_failed = True
                    print(f"  miss  {requested_mode:<16} unsupported")
                else:
                    print(f"  skip  {requested_mode:<16} unsupported")
                continue
            if environment not in configured_environments:
                project_failed = True
                print(
                    f"  miss  {requested_mode:<16} "
                    f"env:{environment} absent from platformio.ini"
                )
            else:
                print(f"  ok    {requested_mode:<16} env:{environment}")
            if requested_mode == "wokwi":
                needs_wokwi = True

        for command in project.dependencies.get("commands", []):
            executable = shutil.which(command)
            if executable:
                print(f"  ok    command          {command} ({executable})")
            else:
                project_failed = True
                print(f"  miss  command          {command}")

        for module in project.dependencies.get("pythonModules", []):
            if importlib.util.find_spec(module):
                print(f"  ok    Python module    {module}")
            else:
                project_failed = True
                print(f"  miss  Python module    {module}")

        for dependency in project.dependencies.get(
            "conditionalPythonModules", []
        ):
            module = dependency["module"]
            when_path = project.root / dependency["whenPath"]
            has_assets = when_path.exists() and any(
                path.is_file() for path in when_path.rglob("*")
            )
            if not has_assets:
                print(
                    f"  skip  Python module    {module} "
                    f"(no assets at {dependency['whenPath']})"
                )
            elif importlib.util.find_spec(module):
                print(f"  ok    Python module    {module} (asset regeneration)")
            else:
                project_failed = True
                print(
                    f"  miss  Python module    {module} "
                    f"(required by assets at {dependency['whenPath']})"
                )

        if project_failed:
            failed = True

    if needs_wokwi:
        wokwi = find_extension("wokwi.wokwi-vscode*")
        if wokwi:
            print(f"\n  ok    Wokwi extension  {wokwi}")
        else:
            failed = True
            print("\n  miss  Wokwi extension  install wokwi.wokwi-vscode")

    print()
    if failed:
        print("Missing required dependencies or invalid project configuration.")
        return 1
    print("Ready for the requested project builds.")
    return 0


def bump_project(project: Project, increment: str) -> str:
    version = project.version
    new_version = next_semver(version, increment)
    project.version_file.write_text(new_version + "\n", encoding="utf-8")
    print(f"{project.id}: {version} -> {new_version}")
    return new_version


def _git(project: Project, *arguments: str, capture: bool = False) -> subprocess.CompletedProcess:
    return subprocess.run(
        ["git", *arguments],
        cwd=str(project.root),
        capture_output=capture,
        text=True,
        check=False,
    )


def _git_tag_exists(project: Project, tag: str) -> bool:
    result = _git(project, "rev-parse", "-q", "--verify", f"refs/tags/{tag}", capture=True)
    return result.returncode == 0


def deploy_project(
    project: Project,
    increment: str,
    push: bool = False,
    build=None,
) -> int:
    if not project.buildable:
        print(f"{project.id}: project is not deployable", file=sys.stderr)
        return 1
    if not project.environment("production"):
        print(f"{project.id}: no production environment", file=sys.stderr)
        return 1

    status = _git(project, "status", "--porcelain", capture=True)
    if status.returncode or status.stdout.strip():
        print("Cannot deploy: working tree is not clean.", file=sys.stderr)
        return 1

    previous_version = project.version
    try:
        next_version = next_semver(previous_version, increment)
    except RegistryError as exc:
        print(exc, file=sys.stderr)
        return 1
    tag = project.release_tag(next_version)
    if _git_tag_exists(project, tag):
        print(f"Cannot deploy: tag {tag} already exists.", file=sys.stderr)
        return 1

    version = bump_project(project, increment)
    committed = False
    try:
        result = (
            build()
            if build is not None
            else build_projects(project.id, "production", [], root=project.root)
        )
        if result:
            return result
        result = publish_project(project)
        if result:
            return result

        release_paths = scoped_release_paths(project)
        add = _git(project, "add", "--", *[str(path) for path in release_paths])
        if add.returncode:
            return add.returncode

        staged = _git(project, "diff", "--cached", "--name-only", "-z", capture=True)
        if staged.returncode:
            return staged.returncode
        staged_files = [
            project.root / name
            for name in staged.stdout.split("\0")
            if name
        ]
        unexpected = [
            path for path in staged_files if not path_is_scoped_release(project, path)
        ]
        if unexpected:
            print(
                f"Cannot deploy: commit would include files outside {project.id}: "
                + ", ".join(str(path.relative_to(project.root)) for path in unexpected),
                file=sys.stderr,
            )
            _git(project, "reset", "--mixed", "HEAD")
            return 1

        commit = _git(
            project,
            "commit",
            "-m",
            f"Release {project.id} {version}",
        )
        if commit.returncode:
            return commit.returncode
        committed = True

        tagged = _git(project, "tag", tag)
        if tagged.returncode:
            print(
                f"{project.id}: commit succeeded but tagging {tag} failed",
                file=sys.stderr,
            )
            return tagged.returncode

        if not push:
            print(f"{project.id}: created local tag {tag}; pass --push to publish it")
            return 0

        pushed = _git(project, "push")
        if pushed.returncode:
            return pushed.returncode
        return _git(project, "push", "origin", tag).returncode
    finally:
        if not committed and project.version != previous_version:
            restore_version(project, previous_version)
            print(f"{project.id}: restored VERSION {previous_version}")


def increment_from_args(args: argparse.Namespace) -> str:
    if args.major:
        return "major"
    if args.minor:
        return "minor"
    return "patch"


def add_increment_flags(
    parser: argparse.ArgumentParser, required: bool = False
) -> None:
    group = parser.add_mutually_exclusive_group(required=required)
    group.add_argument("--major", action="store_true")
    group.add_argument("--minor", action="store_true")
    group.add_argument("--patch", action="store_true")


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    subparsers.add_parser("validate", help="validate projects.json")
    subparsers.add_parser("list", help="list registered projects")

    build = subparsers.add_parser("build", help="build one project or all projects")
    build.add_argument("target")
    build.add_argument(
        "--env", choices=("production", "wokwi"), default="production"
    )
    build.add_argument("--target", dest="build_targets", action="append", default=[])
    build.add_argument("-v", "--verbose", action="store_true")

    check = subparsers.add_parser("check-deps", help="check project dependencies")
    check.add_argument("target", nargs="?", default="all")
    check.add_argument("--env", choices=("production", "wokwi"))

    bump = subparsers.add_parser("bump", help="increment one project version")
    bump.add_argument("target")
    add_increment_flags(bump)

    stage = subparsers.add_parser("stage", help="stage one project or all")
    stage.add_argument("target")

    publish = subparsers.add_parser("publish", help="publish one project or all")
    publish.add_argument("target")

    deploy = subparsers.add_parser("deploy", help="release one project")
    deploy.add_argument("target")
    add_increment_flags(deploy, required=True)
    deploy.add_argument(
        "--push",
        action="store_true",
        help="push the release commit and project tag",
    )
    return parser


def main() -> int:
    parser = create_parser()
    args = parser.parse_args()
    try:
        registry = load_registry()
        if args.command == "validate":
            print(f"projects.json: ok ({len(registry.projects)} project(s))")
            return 0
        if args.command == "list":
            for project in registry.projects:
                modes = ",".join(project.environments)
                status = "buildable" if project.buildable else "template"
                print(f"{project.id}\t{project.version}\t{status}\t{modes}")
            return 0
        if args.command == "build":
            return build_projects(
                args.target, args.env, args.build_targets, args.verbose
            )
        if args.command == "check-deps":
            return check_dependencies(args.target, args.env)
        if args.command == "bump":
            projects = registry.select(args.target)
            if len(projects) != 1:
                raise RegistryError("bump requires one project, not 'all'")
            bump_project(projects[0], increment_from_args(args))
            return 0
        if args.command == "stage":
            for project in registry.select(args.target):
                if args.target == "all" and not project.buildable:
                    print(f"skip  {project.id}: project is not stageable")
                    continue
                result = stage_project(project)
                if result:
                    return result
            return 0
        if args.command == "publish":
            for project in registry.select(args.target):
                if args.target == "all" and not project.buildable:
                    print(f"skip  {project.id}: project is not publishable")
                    continue
                result = publish_project(project)
                if result:
                    return result
            return 0
        if args.command == "deploy":
            projects = registry.select(args.target)
            if len(projects) != 1:
                raise RegistryError("deploy requires one project, not 'all'")
            return deploy_project(
                projects[0], increment_from_args(args), push=args.push
            )
    except RegistryError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    return 1


if __name__ == "__main__":
    sys.exit(main())
