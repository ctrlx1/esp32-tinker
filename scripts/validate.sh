#!/usr/bin/env bash
# Validate the project registry and run script unit tests.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
python3 "$ROOT/scripts/project_tool.py" validate
python3 -m unittest discover -s scripts/tests -p 'test_*.py'
