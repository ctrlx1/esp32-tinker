#!/usr/bin/env bash
# Migration wrapper for the former command's documented -e, -t, and -v options.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MODE="production"
TARGET=()
VERBOSE=()

while [[ "$#" -gt 0 ]]; do
  case "$1" in
    -e|--environment|--env)
      [[ "$#" -ge 2 ]] || { echo "Missing environment value" >&2; exit 2; }
      case "$2" in
        esp32dev|production) MODE="production" ;;
        wokwi) MODE="wokwi" ;;
        *) echo "Unknown Justin environment: $2" >&2; exit 2 ;;
      esac
      shift 2
      ;;
    -t|--target)
      [[ "$#" -ge 2 ]] || { echo "Missing target value" >&2; exit 2; }
      TARGET+=(--target "$2")
      shift 2
      ;;
    -v|--verbose)
      VERBOSE=(--verbose)
      shift
      ;;
    -h|--help)
      echo "Deprecated: use ./scripts/build.sh justin [--env production|wokwi] [--target TARGET] [--verbose]"
      echo "This migration wrapper supports only -e/--environment, -t/--target, and -v/--verbose."
      exit 0
      ;;
    *)
      echo "Unsupported legacy argument: $1" >&2
      echo "The migration wrapper supports only -e/--environment, -t/--target, and -v/--verbose." >&2
      echo "Use ./scripts/build.sh justin --help" >&2
      exit 2
      ;;
  esac
done

echo "Deprecated: use ./scripts/build.sh justin --env $MODE ${TARGET[*]:-} ${VERBOSE[*]:-}" >&2
exec "$ROOT/scripts/build.sh" justin --env "$MODE" "${TARGET[@]}" "${VERBOSE[@]}"
