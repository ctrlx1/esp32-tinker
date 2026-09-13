#!/usr/bin/env bash
# Stage verified production artifacts for one project or all projects.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
exec python3 "$ROOT/scripts/project_tool.py" stage "$@"
