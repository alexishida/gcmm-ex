#!/usr/bin/env bash
# Build GCMM-EX with the configured host toolchain or Retro environment.
set -euo pipefail

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

usage() {
  cat >&2 <<'EOF'
Usage: build.sh [all|gc|wii|clean]

  all    Build GameCube and Wii targets (default).
  gc     Build the GameCube target.
  wii    Build the Wii target.
  clean  Remove generated build outputs.
EOF
}

target=${1:-all}
case "$target" in
  all|gc|wii|clean)
    ;;
  -h|--help)
    usage
    exit 0
    ;;
  *)
    usage
    exit 64
    ;;
esac

if [ -f "$project_root/.env" ]; then
  set -a
  . "$project_root/.env"
  set +a
fi

cd "$project_root"

if [ -n "${RETRO_BIN:-}" ] || [ -n "${RETRO_PLATFORM:-}" ]; then
  if [ -z "${RETRO_BIN:-}" ] || [ -z "${RETRO_PLATFORM:-}" ]; then
    echo "RETRO_BIN and RETRO_PLATFORM must both be set." >&2
    exit 1
  fi
  exec "$RETRO_BIN" "$RETRO_PLATFORM" make "$target"
fi

if [ -z "${DEVKITPPC:-}" ]; then
  echo "DEVKITPPC is not set. Configure .env or RETRO_BIN/RETRO_PLATFORM." >&2
  exit 1
fi

exec make "$target"
