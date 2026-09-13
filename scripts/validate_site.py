#!/usr/bin/env python3
"""Validate published firmware and a built Astro catalog against the registry."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import List, Optional, Set
from urllib.parse import urlparse, unquote

from project_registry import Registry, RegistryError, load_registry


REPO_ROOT = Path(__file__).resolve().parents[1]
SITE_DIST = REPO_ROOT / "site" / "dist"
SITE_BASE = "/esp32-tinker/"
ATTR_RE = re.compile(
    r"""(?:href|src|manifest)\s*=\s*["']([^"']+)["']""",
    re.IGNORECASE,
)
PUBLISHED_FILES = ("bootloader.bin", "partitions.bin", "firmware.bin", "manifest.json")


def _error(errors: List[str], message: str) -> None:
    errors.append(message)


def validate_published_packages(registry: Registry, errors: List[str]) -> None:
    catalog_ids = {project.id for project in registry.projects}
    for project in registry.projects:
        if not project.buildable:
            if project.docs.get("manifest"):
                _error(errors, f"{project.id}: template must not publish a manifest")
            continue
        for name in PUBLISHED_FILES:
            path = project.docs_path / name
            if not path.is_file():
                _error(errors, f"{project.id}: missing published {name}")
        if not project.manifest_path.is_file():
            continue
        try:
            manifest = json.loads(project.manifest_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as exc:
            _error(errors, f"{project.id}: invalid manifest JSON: {exc.msg}")
            continue
        if manifest.get("name") != project.name:
            _error(
                errors,
                f"{project.id}: manifest name {manifest.get('name')!r} != {project.name!r}",
            )
        if manifest.get("version") != project.version:
            _error(
                errors,
                f"{project.id}: manifest version {manifest.get('version')!r} != {project.version!r}",
            )
        parts = []
        for build in manifest.get("builds") or []:
            parts.extend(part.get("path") for part in build.get("parts") or [])
        for name in ("bootloader.bin", "partitions.bin", "firmware.bin"):
            if name not in parts:
                _error(errors, f"{project.id}: manifest is missing {name}")
        extra = {part for part in parts if part and part not in PUBLISHED_FILES}
        if extra:
            _error(errors, f"{project.id}: unexpected manifest parts {sorted(extra)}")
    firmware_root = next(
        (project.docs_path.parent for project in registry.projects if project.buildable),
        None,
    )
    if firmware_root and firmware_root.is_dir():
        published_ids = {
            path.name for path in firmware_root.iterdir() if path.is_dir()
        }
        unexpected = published_ids - catalog_ids
        if unexpected:
            _error(
                errors,
                f"published firmware folders not in registry: {sorted(unexpected)}",
            )


def _route_page(dist: Path, route: str) -> Path:
    slug = route.strip("/")
    if not slug:
        return dist / "index.html"
    return dist / slug / "index.html"


def validate_built_routes(registry: Registry, dist: Path, errors: List[str]) -> None:
    if not (dist / "index.html").is_file():
        _error(errors, "site dist is missing the catalog index.html")
    catalog = (dist / "index.html").read_text(encoding="utf-8") if (dist / "index.html").is_file() else ""
    for project in registry.projects:
        page = _route_page(dist, project.docs["route"])
        if not page.is_file():
            _error(errors, f"{project.id}: missing built page {page.relative_to(dist)}")
            continue
        html = page.read_text(encoding="utf-8")
        if project.name not in html:
            _error(errors, f"{project.id}: built page does not include {project.name!r}")
        if project.name not in catalog:
            _error(errors, f"{project.id}: catalog does not include {project.name!r}")
        if project.buildable:
            firmware_dir = dist / "firmware" / project.id
            for name in PUBLISHED_FILES:
                if not (firmware_dir / name).is_file():
                    _error(errors, f"{project.id}: built site missing firmware/{project.id}/{name}")


def _local_target(dist: Path, source: Path, raw_url: str) -> Optional[Path]:
    url = raw_url.strip()
    if not url or url.startswith(("#", "mailto:", "data:", "javascript:")):
        return None
    parsed = urlparse(url)
    if parsed.scheme in {"http", "https"}:
        return None
    path = unquote(parsed.path)
    if path.startswith(SITE_BASE):
        relative = path[len(SITE_BASE) :]
        target = dist / relative
    elif path.startswith("/"):
        return None
    else:
        target = (source.parent / path).resolve()
        try:
            target.relative_to(dist.resolve())
        except ValueError:
            return None
    if target.is_dir() or path.endswith("/"):
        return target / "index.html"
    return target


def validate_built_links(dist: Path, errors: List[str]) -> None:
    seen: Set[str] = set()
    for html_path in sorted(dist.rglob("*.html")):
        text = html_path.read_text(encoding="utf-8")
        for match in ATTR_RE.finditer(text):
            raw = match.group(1)
            if raw.startswith("/") and not raw.startswith(SITE_BASE):
                key = f"{html_path}:{raw}"
                if key not in seen:
                    seen.add(key)
                    _error(
                        errors,
                        f"{html_path.relative_to(dist)}: absolute path is not base-safe: {raw}",
                    )
                continue
            target = _local_target(dist, html_path, raw)
            if target is None:
                continue
            key = str(target)
            if key in seen:
                continue
            seen.add(key)
            if not target.is_file():
                _error(
                    errors,
                    f"{html_path.relative_to(dist)}: broken link {raw} -> {target.relative_to(dist) if dist in target.parents or target.parent == dist else target}",
                )


def validate_site(
    registry: Optional[Registry] = None,
    dist: Optional[Path] = None,
    require_dist: bool = False,
) -> List[str]:
    loaded = registry or load_registry()
    errors: List[str] = []
    validate_published_packages(loaded, errors)
    if dist is None:
        dist = SITE_DIST
    if dist.is_dir():
        validate_built_routes(loaded, dist, errors)
        validate_built_links(dist, errors)
    elif require_dist:
        _error(errors, f"site dist not found: {dist}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--dist",
        type=Path,
        default=SITE_DIST,
        help="built Astro output directory",
    )
    parser.add_argument(
        "--require-dist",
        action="store_true",
        help="fail when the built site directory is missing",
    )
    args = parser.parse_args()
    try:
        errors = validate_site(dist=args.dist, require_dist=args.require_dist)
    except RegistryError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    if errors:
        for error in errors:
            print(f"error: {error}", file=sys.stderr)
        return 1
    print("site: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
