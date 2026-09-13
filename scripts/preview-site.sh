#!/usr/bin/env bash
# Preview the Astro installer site.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SITE="$ROOT/site"
PORT="${1:-4321}"

if ! command -v npm >/dev/null 2>&1; then
  echo "npm is required to preview the site." >&2
  exit 1
fi

if [[ ! -d "$SITE/node_modules" ]]; then
  (cd "$SITE" && npm ci)
fi

echo "Site preview: http://localhost:${PORT}/esp32-tinker/"
echo "Press Ctrl+C to stop."
cd "$SITE"
exec npm run dev -- --host --port "$PORT"
