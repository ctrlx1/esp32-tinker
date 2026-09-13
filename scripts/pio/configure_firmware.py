"""Generate version and embedded portal headers for one PlatformIO project."""

Import("env")

import os
import re


SEMVER_RE = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$")
PROJECT_ID_RE = re.compile(r"^[a-z0-9][a-z0-9_-]*$")

project_dir = env.subst("$PROJECT_DIR")
repo_dir = os.path.abspath(os.path.join(project_dir, "..", ".."))
project_id = env.GetProjectOption("custom_tinker_project_id")

if not PROJECT_ID_RE.fullmatch(project_id):
    raise RuntimeError(f"Invalid custom_tinker_project_id: {project_id!r}")

version_path = os.path.join(project_dir, "VERSION")
shell_path = os.path.join(
    repo_dir, "packages", "tinker-core", "assets", "portal", "shell.html"
)
content_path = os.path.join(project_dir, "portal", "portal_content.html")
version_header_path = os.path.join(
    project_dir, "include", "app_version.generated.h"
)
portal_header_path = os.path.join(project_dir, "include", "setup_html.generated.h")

with open(version_path, "r", encoding="utf-8") as version_file:
    app_version = version_file.read().strip()
if not SEMVER_RE.fullmatch(app_version):
    raise RuntimeError(f"VERSION must contain strict semver, got: {app_version!r}")

firmware_filename = "firmware.bin"

with open(version_header_path, "w", encoding="utf-8") as out_file:
    out_file.write("// Auto-generated from VERSION - do not edit\n")
    out_file.write("#pragma once\n\n")
    out_file.write(f'#define APP_PROJECT_ID "{project_id}"\n')
    out_file.write(f'#define APP_VERSION "{app_version}"\n')
    out_file.write(f'#define APP_FIRMWARE_FILENAME "{firmware_filename}"\n')

with open(shell_path, "r", encoding="utf-8") as shell_file:
    shell = shell_file.read()
with open(content_path, "r", encoding="utf-8") as content_file:
    content = content_file.read().rstrip("\n")

if shell.count("{{PROJECT_PORTAL_CONTENT}}") != 1:
    raise RuntimeError("Portal shell must contain exactly one project content marker")
if "{{PROJECT_PORTAL_CONTENT}}" in content:
    raise RuntimeError("Project portal content must not contain the shell marker")
for placeholder in (
    "APP_VERSION_PLACEHOLDER",
    "APP_FIRMWARE_FILENAME_PLACEHOLDER",
):
    if content.count(placeholder) != 1:
        raise RuntimeError(
            f"Project portal content must contain exactly one {placeholder}"
        )

html = shell.replace("{{PROJECT_PORTAL_CONTENT}}", content)
html = html.replace("APP_VERSION_PLACEHOLDER", app_version)
html = html.replace("APP_FIRMWARE_FILENAME_PLACEHOLDER", firmware_filename)
if (
    "{{PROJECT_PORTAL_CONTENT}}" in html
    or "APP_VERSION_PLACEHOLDER" in html
    or "APP_FIRMWARE_FILENAME_PLACEHOLDER" in html
):
    raise RuntimeError("Portal composition left an unresolved build placeholder")

delimiter = "TinkerSetup"
while f"){delimiter}" in html:
    delimiter += "X"
    if len(delimiter) > 16:
        raise RuntimeError("Could not find a raw-string delimiter for setup portal")

with open(portal_header_path, "w", encoding="utf-8") as out_file:
    out_file.write("// Auto-generated from portal shell and project content\n")
    out_file.write("#pragma once\n\n")
    out_file.write("#include <pgmspace.h>\n\n")
    out_file.write(f'const char SETUP_PORTAL_HTML[] PROGMEM = R"{delimiter}(\n')
    out_file.write(html)
    if not html.endswith("\n"):
        out_file.write("\n")
    out_file.write(f'){delimiter}";\n')
