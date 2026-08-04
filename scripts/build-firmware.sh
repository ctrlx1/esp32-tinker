#!/usr/bin/env bash
# Build the ESP32 firmware in Firmware-PIO with PlatformIO.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FIRMWARE="$ROOT/Firmware-PIO"

if command -v pio >/dev/null 2>&1; then
  PIO=pio
elif [[ -x "$HOME/.platformio/penv/bin/pio" ]]; then
  PIO="$HOME/.platformio/penv/bin/pio"
else
  echo "PlatformIO not found. Install the PlatformIO extension or add pio to PATH." >&2
  exit 1
fi

python3 "$ROOT/scripts/import-pixel-art.py"

cd "$FIRMWARE"
exec "$PIO" run "$@"
