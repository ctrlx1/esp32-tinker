#!/usr/bin/env bash
# Compatibility wrapper: preview the Astro installer site.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
echo "Deprecated: use ./scripts/preview-site.sh" >&2
exec "$ROOT/scripts/preview-site.sh" "$@"
