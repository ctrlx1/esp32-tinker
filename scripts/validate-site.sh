#!/usr/bin/env bash
# Validate published firmware and a built Astro catalog against the registry.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
exec python3 "$ROOT/scripts/validate_site.py" --require-dist "$@"
