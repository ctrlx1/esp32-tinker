#!/usr/bin/env bash
# Print the firmware CI matrix as JSON.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
exec python3 "$ROOT/scripts/project_tool.py" ci-matrix
