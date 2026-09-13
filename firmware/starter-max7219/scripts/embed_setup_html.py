Import("env")

import os
import re

project_dir = env.subst("$PROJECT_DIR")
repo_dir = os.path.abspath(os.path.join(project_dir, "..", ".."))
version_path = os.path.join(project_dir, "VERSION")
shell_path = os.path.join(
    repo_dir, "packages", "tinker-core", "assets", "portal", "shell.html"
)
content_path = os.path.join(project_dir, "portal", "portal_content.html")
out_path = os.path.join(project_dir, "include", "setup_html.generated.h")

SEMVER_RE = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$")

with open(version_path, "r", encoding="utf-8") as version_file:
    app_version = version_file.read().strip()
if not SEMVER_RE.match(app_version):
    raise RuntimeError(f"VERSION must contain strict semver, got: {app_version!r}")

with open(shell_path, "r", encoding="utf-8") as shell_file:
    shell = shell_file.read()
with open(content_path, "r", encoding="utf-8") as content_file:
    content = content_file.read().rstrip("\n")

html = shell.replace("{{PROJECT_PORTAL_CONTENT}}", content)
html = html.replace("APP_VERSION_PLACEHOLDER", app_version)
html = html.replace(
    "APP_FIRMWARE_FILENAME_PLACEHOLDER",
    f"starter-max7219_{app_version}.bin",
)

delimiter = "StarterSetup"
while f"){delimiter}" in html:
    delimiter += "X"
    if len(delimiter) > 16:
        raise RuntimeError("Could not find a raw-string delimiter for setup portal")

with open(out_path, "w", encoding="utf-8") as out_file:
    out_file.write("// Auto-generated from portal shell and project content\n")
    out_file.write("#pragma once\n\n")
    out_file.write("#include <pgmspace.h>\n\n")
    out_file.write(f'const char SETUP_PORTAL_HTML[] PROGMEM = R"{delimiter}(\n')
    out_file.write(html)
    if not html.endswith("\n"):
        out_file.write("\n")
    out_file.write(f'){delimiter}";\n')
