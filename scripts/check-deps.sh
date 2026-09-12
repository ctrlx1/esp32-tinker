#!/usr/bin/env bash
# Check that local tools needed to build, flash, and simulate are present.
set -euo pipefail

FAILED=0

ok()   { printf '  ok    %s\n' "$1"; }
miss() { printf '  miss  %s\n' "$1"; FAILED=1; }
warn() { printf '  warn  %s\n' "$1"; }

find_extension() {
  local pattern="$1"
  local dir match
  local saved
  saved="$(shopt -p nullglob)"
  shopt -s nullglob
  for dir in "$HOME/.cursor/extensions" "$HOME/.vscode/extensions" "$HOME/.vscode-oss/extensions"; do
    [[ -d "$dir" ]] || continue
    for match in "$dir"/$pattern; do
      [[ -d "$match" ]] || continue
      eval "$saved"
      printf '%s\n' "$match"
      return 0
    done
  done
  eval "$saved"
  return 1
}

echo "Checking development dependencies..."
echo

if command -v python3 >/dev/null 2>&1; then
  ok "python3          $(python3 --version 2>&1)"
else
  miss "python3          not found (needed by firmware pre-build scripts)"
fi

PIO=""
if command -v pio >/dev/null 2>&1; then
  PIO="$(command -v pio)"
elif [[ -x "$HOME/.platformio/penv/bin/pio" ]]; then
  PIO="$HOME/.platformio/penv/bin/pio"
elif [[ -x "$HOME/.local/bin/pio" ]]; then
  PIO="$HOME/.local/bin/pio"
fi

if [[ -n "$PIO" ]]; then
  ok "PlatformIO CLI   $("$PIO" --version 2>&1)  ($PIO)"
else
  miss "PlatformIO CLI   not found"
  printf '         Install: pipx install platformio\n'
  printf '         Or install the PlatformIO IDE extension (it provides pio).\n'
fi

if WOKWI="$(find_extension 'wokwi.wokwi-vscode*')"; then
  ok "Wokwi extension  ${WOKWI/#$HOME/~}"
else
  miss "Wokwi extension  not found (needed to run the simulator)"
  printf '         Install in Cursor/VS Code: wokwi.wokwi-vscode\n'
fi

if PIO_IDE="$(find_extension '*platformio-ide*')"; then
  ok "PlatformIO IDE   ${PIO_IDE/#$HOME/~}"
else
  warn "PlatformIO IDE   not found (optional; the CLI is enough to build)"
fi

echo
if [[ "$FAILED" -ne 0 ]]; then
  echo "Missing required tools. See README Development."
  exit 1
fi

echo "Ready to build and simulate."
echo "  ./scripts/build-firmware.sh -e wokwi"
echo "  Then: Cmd+Shift+P → Wokwi: Start Simulator"
