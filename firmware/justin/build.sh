#!/usr/bin/env bash
# Build this firmware through the repository project registry.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
exec "$ROOT/scripts/build.sh" justin "$@"
