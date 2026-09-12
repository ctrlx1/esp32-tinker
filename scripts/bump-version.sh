#!/usr/bin/env bash
# Increment one registered project version.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

if [[ "$#" -eq 0 || "${1:-}" == -* ]]; then
  echo "Compatibility mode: defaulting to project 'justin'." >&2
  set -- justin "$@"
fi

exec python3 "$ROOT/scripts/project_tool.py" bump "$@"
