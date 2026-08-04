Import("env")

import os
import re

project_dir = env.subst("$PROJECT_DIR")
root_dir = os.path.abspath(os.path.join(project_dir, ".."))
version_path = os.path.join(root_dir, "VERSION")
html_path = os.path.join(project_dir, "include", "setup_portal.html")
out_path = os.path.join(project_dir, "include", "setup_html.generated.h")

SEMVER_RE = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$")

with open(version_path, "r", encoding="utf-8") as version_file:
    app_version = version_file.read().strip()

if not SEMVER_RE.match(app_version):
    raise RuntimeError(f"VERSION must contain strict semver, got: {app_version!r}")

with open(html_path, "r", encoding="utf-8") as html_file:
    html = html_file.read()

html = html.replace("APP_VERSION_PLACEHOLDER", app_version)
html = html.replace("APP_FIRMWARE_FILENAME_PLACEHOLDER", f"firmware_{app_version}.bin")

delimiter = "TinkerSetup"
while f"){delimiter}" in html:
    delimiter += "X"
    if len(delimiter) > 16:
        raise RuntimeError("Could not find a raw-string delimiter for setup_portal.html")

with open(out_path, "w", encoding="utf-8") as out_file:
    out_file.write("// Auto-generated from include/setup_portal.html — do not edit\n")
    out_file.write("#pragma once\n\n")
    out_file.write("#include <pgmspace.h>\n\n")
    out_file.write(f'const char SETUP_PORTAL_HTML[] PROGMEM = R"{delimiter}(\n')
    out_file.write(html)
    if not html.endswith("\n"):
        out_file.write("\n")
    out_file.write(f'){delimiter}";\n')
