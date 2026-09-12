#!/usr/bin/env bash
# Check dependencies for one registered project or all projects.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
exec python3 "$ROOT/scripts/project_tool.py" check-deps "$@"
