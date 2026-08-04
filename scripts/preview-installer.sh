#!/usr/bin/env bash
# Preview the browser installer locally (same files GitHub Pages serves from docs/).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PORT="${1:-8765}"

echo "Installer preview: http://localhost:$PORT/"
echo "Press Ctrl+C to stop."
cd "$ROOT/docs"
exec python3 -m http.server "$PORT"
