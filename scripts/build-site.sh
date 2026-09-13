#!/usr/bin/env bash
# Build the Astro installer site.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SITE="$ROOT/site"

if ! command -v npm >/dev/null 2>&1; then
  echo "npm is required to build the site." >&2
  exit 1
fi

if [[ ! -d "$SITE/node_modules" ]]; then
  (cd "$SITE" && npm ci)
fi

(cd "$SITE" && ASTRO_TELEMETRY_DISABLED=1 npm run build)
echo "Site built at site/dist. Preview with ./scripts/preview-site.sh"
