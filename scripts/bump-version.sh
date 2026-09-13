#!/usr/bin/env bash
# Increment one registered project version.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

if [[ "$#" -eq 0 || "${1:-}" == -* ]]; then
  echo "Usage: $0 <project> [--major|--minor|--patch]" >&2
  exit 2
fi

exec python3 "$ROOT/scripts/project_tool.py" bump "$@"
