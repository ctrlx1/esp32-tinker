#!/usr/bin/env bash
# Increment the project VERSION file using semver.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION_FILE="$ROOT/VERSION"
INCREMENT="patch"
INCREMENT_FLAGS=0

usage() {
  echo "Usage: $0 [--major|--minor|--patch]" >&2
}

for arg in "$@"; do
  case "$arg" in
    --major|--minor|--patch)
      INCREMENT_FLAGS=$((INCREMENT_FLAGS + 1))
      INCREMENT="${arg#--}"
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      usage
      exit 1
      ;;
  esac
done

if (( INCREMENT_FLAGS > 1 )); then
  usage
  exit 1
fi

VERSION="$(<"$VERSION_FILE")"
if [[ ! "$VERSION" =~ ^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$ ]]; then
  echo "VERSION must contain strict semver, got: $VERSION" >&2
  exit 1
fi

IFS=. read -r major minor patch <<<"$VERSION"

case "$INCREMENT" in
  major)
    major=$((major + 1))
    minor=0
    patch=0
    ;;
  minor)
    minor=$((minor + 1))
    patch=0
    ;;
  patch)
    patch=$((patch + 1))
    ;;
esac

NEW_VERSION="$major.$minor.$patch"
printf '%s\n' "$NEW_VERSION" >"$VERSION_FILE"
echo "$VERSION -> $NEW_VERSION"
