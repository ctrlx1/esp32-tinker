#!/usr/bin/env bash
# Copy PlatformIO build artifacts into docs/ for the esp-web-tools browser installer.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/Firmware-PIO/.pio/build/esp32dev"
DOCS="$ROOT/docs"
VERSION_FILE="$ROOT/VERSION"

VERSION="$(<"$VERSION_FILE")"
if [[ ! "$VERSION" =~ ^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$ ]]; then
  echo "VERSION must contain strict semver, got: $VERSION" >&2
  exit 1
fi

FIRMWARE_NAME="firmware_${VERSION}.bin"

for bin in bootloader.bin partitions.bin firmware.bin; do
  if [[ ! -f "$BUILD/$bin" ]]; then
    echo "Missing $BUILD/$bin — run './scripts/build-firmware.sh' first." >&2
    exit 1
  fi
done

cp "$BUILD/bootloader.bin" "$BUILD/partitions.bin" "$DOCS/"
cp "$BUILD/firmware.bin" "$DOCS/$FIRMWARE_NAME"
rm -f "$DOCS/firmware.bin"

python3 - "$DOCS/manifest.json" "$VERSION" "$FIRMWARE_NAME" <<'PY'
import json
import sys
from pathlib import Path

manifest_path = Path(sys.argv[1])
version = sys.argv[2]
firmware_name = sys.argv[3]

manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
manifest["version"] = version

updated = False
for build in manifest.get("builds", []):
    for part in build.get("parts", []):
        if part.get("offset") == 65536:
            part["path"] = firmware_name
            updated = True

if not updated:
    raise SystemExit("Could not find firmware app partition at offset 65536")

manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
PY

echo "Updated docs/bootloader.bin, docs/partitions.bin, docs/$FIRMWARE_NAME"
echo "Updated docs/manifest.json to version $VERSION"
