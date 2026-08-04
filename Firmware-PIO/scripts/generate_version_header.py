Import("env")

import os
import re

project_dir = env.subst("$PROJECT_DIR")
root_dir = os.path.abspath(os.path.join(project_dir, ".."))
version_path = os.path.join(root_dir, "VERSION")
out_path = os.path.join(project_dir, "include", "app_version.generated.h")

SEMVER_RE = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$")

with open(version_path, "r", encoding="utf-8") as version_file:
    app_version = version_file.read().strip()

if not SEMVER_RE.match(app_version):
    raise RuntimeError(f"VERSION must contain strict semver, got: {app_version!r}")

firmware_filename = f"firmware_{app_version}.bin"

with open(out_path, "w", encoding="utf-8") as out_file:
    out_file.write("// Auto-generated from VERSION - do not edit\n")
    out_file.write("#pragma once\n\n")
    out_file.write(f'#define APP_VERSION "{app_version}"\n')
    out_file.write(f'#define APP_FIRMWARE_FILENAME "{firmware_filename}"\n')
