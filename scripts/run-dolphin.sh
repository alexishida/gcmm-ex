#!/usr/bin/env bash
# Launch a built GCMM-EX DOL with a test-only Dolphin user directory.
set -euo pipefail

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ -f "$project_root/.env" ]; then
  # Local emulator settings are intentionally not tracked.
  set -a
  . "$project_root/.env"
  set +a
fi
platform=${1:---gc}
test_user_dir=${DOLPHIN_USER_DIR:-"$project_root/test/dolphin/user"}

case "$platform" in
  --gc)
    dol="$project_root/releases/gcmm_GC.dol"
    ;;
  --wii)
    dol="$project_root/releases/gcmm_WII.dol"
    ;;
  *)
    echo "Usage: $0 [--gc|--wii]" >&2
    exit 64
    ;;
esac

if [ ! -f "$dol" ]; then
  echo "Missing $dol. Build it first with: retro-gcwii make ${platform#--}" >&2
  exit 1
fi

if [ -z "${DOLPHIN:-}" ]; then
  echo "DOLPHIN is not set. Configure it in $project_root/.env." >&2
  exit 1
fi
dolphin="$DOLPHIN"

if [ ! -f "$dolphin" ]; then
  echo "Configured Dolphin executable was not found: $dolphin" >&2
  exit 1
fi

mkdir -p "$test_user_dir/GC" "$test_user_dir/Wii"

if [[ "$dolphin" == *.exe ]]; then
  if ! command -v wslpath >/dev/null 2>&1; then
    echo "Windows Dolphin requires WSL path conversion. Set DOLPHIN to a native build." >&2
    exit 1
  fi
  exec "$dolphin" -u "$(wslpath -w "$test_user_dir")" -e "$(wslpath -w "$dol")"
fi

exec "$dolphin" -u "$test_user_dir" -e "$dol"
