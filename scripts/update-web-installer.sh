#!/usr/bin/env bash
# Compatibility wrapper for the former single-project publish command.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
echo "Deprecated: use ./scripts/publish.sh justin" >&2
exec "$ROOT/scripts/publish.sh" justin
