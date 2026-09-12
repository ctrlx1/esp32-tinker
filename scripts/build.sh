#!/usr/bin/env bash
# Build one registered firmware project or all projects.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
exec python3 "$ROOT/scripts/project_tool.py" build "$@"
