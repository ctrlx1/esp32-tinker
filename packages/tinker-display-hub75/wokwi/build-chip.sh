#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
SRC="$ROOT/hub75-matrix.chip.c"
OUT="$ROOT/hub75-matrix.chip.wasm"

CLANG=""
if command -v clang >/dev/null 2>&1; then
  CLANG="clang"
elif [ -x /tmp/wasi-sdk-25/wasi-sdk-25.0-x86_64-linux/bin/clang ]; then
  CLANG="/tmp/wasi-sdk-25/wasi-sdk-25.0-x86_64-linux/bin/clang"
fi

if [ -z "$CLANG" ]; then
  echo "clang is required to build the Wokwi HUB75 chip" >&2
  exit 1
fi

SYSROOT=""
if [ -d /tmp/wasi-sdk-25/wasi-sdk-25.0-x86_64-linux/share/wasi-sysroot ]; then
  SYSROOT="/tmp/wasi-sdk-25/wasi-sdk-25.0-x86_64-linux/share/wasi-sysroot"
fi

if [ -n "$SYSROOT" ]; then
  "$CLANG" --target=wasm32-unknown-wasi --sysroot "$SYSROOT" \
    -nostartfiles -O2 -Wall -Werror \
    -Wl,--no-entry -Wl,--import-memory -Wl,--export-table \
    -Wl,--export=chipInit \
    -Wl,--export=__wokwi_api_version_1 \
    -o "$OUT" "$SRC"
else
  "$CLANG" --target=wasm32-unknown-unknown -nostdlib -ffreestanding \
    -O2 -Wall -Werror \
    -Wl,--no-entry -Wl,--export=chipInit \
    -Wl,--export=__wokwi_api_version_1 -Wl,--allow-undefined \
    -o "$OUT" "$SRC"
fi

echo "Wrote $OUT"
