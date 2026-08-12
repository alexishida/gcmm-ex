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
test_user_dir=${DOLPHIN_USER_DIR:-"$project_root/tests/dolphin/user"}
memorycards_dir=${GCMM_TEST_MEMORYCARDS_DIR:-"$project_root/memorycards"}

set_ini_value() {
  local ini=$1
  local section=$2
  local key=$3
  local value=$4
  local temporary

  temporary=$(mktemp "$ini.XXXXXX")
  GCMM_DOLPHIN_INI_VALUE=$value awk -v section="$section" -v key="$key" '
    BEGIN {
      target = "[" section "]"
      value = ENVIRON["GCMM_DOLPHIN_INI_VALUE"]
      in_target = 0
      section_found = 0
      record_count = 0
      target_line_count = 0
    }
    {
      sub(/\r$/, "")
    }
    $0 == target {
      in_target = 1
      if (!section_found) {
        record_type[++record_count] = "target"
        section_found = 1
      }
      next
    }
    $0 ~ /^\[/ {
      in_target = 0
    }
    in_target {
      if ($0 ~ ("^" key "[[:space:]]*="))
        next
      target_line[++target_line_count] = $0
      next
    }
    {
      record_type[++record_count] = "line"
      record_line[record_count] = $0
    }
    END {
      if (!section_found) {
        record_type[++record_count] = "line"
        record_line[record_count] = ""
        record_type[++record_count] = "target"
      }
      for (i = 1; i <= record_count; i++) {
        if (record_type[i] == "target") {
          print target
          print key " = " value
          for (j = 1; j <= target_line_count; j++)
            print target_line[j]
        } else {
          print record_line[i]
        }
      }
    }
  ' "$ini" > "$temporary"
  mv "$temporary" "$ini"
}

prepare_test_cards() {
  local config="$test_user_dir/Config/Dolphin.ini"
  # Dolphin resolves custom card paths to region-specific names. Keep the
  # expected USA suffix so it opens the RAW card instead of creating an empty
  # regional card at launch.
  local raw_source="$memorycards_dir/backup.USA.raw"
  local card_b_raw="$test_user_dir/GC/GCMM-EX/Test Card B.USA.raw"
  local config_card_b_raw=$card_b_raw

  mkdir -p "$(dirname -- "$config")" "$(dirname -- "$card_b_raw")"

  if [ ! -e "$card_b_raw" ]; then
    if [ ! -f "$raw_source" ]; then
      echo "Missing Dolphin test card: $raw_source" >&2
      exit 1
    fi
    cp -- "$raw_source" "$card_b_raw"
  fi

  if [[ "$dolphin" == *.exe ]]; then
    if ! command -v wslpath >/dev/null 2>&1; then
      echo "Windows Dolphin requires WSL path conversion. Set DOLPHIN to a native build." >&2
      exit 1
    fi
    config_card_b_raw=$(wslpath -w "$card_b_raw")
  fi

  touch "$config"
  set_ini_value "$config" Core SlotA 255
  set_ini_value "$config" Core SlotB 1
  set_ini_value "$config" Core MemcardBPath "$config_card_b_raw"
}

case "$platform" in
  --gc)
    dol="$project_root/releases/gcmm_ex_GC.dol"
    ;;
  --wii)
    dol="$project_root/releases/gcmm_ex_WII.dol"
    ;;
  --setup)
    platform=
    ;;
  *)
    echo "Usage: $0 [--gc|--wii|--setup]" >&2
    exit 64
    ;;
esac

if [ -n "$platform" ] && [ ! -f "$dol" ]; then
  echo "Missing $dol. Build it first with: retro-gcwii make ${platform#--}" >&2
  exit 1
fi

if [ -n "$platform" ] && [ -z "${DOLPHIN:-}" ]; then
  echo "DOLPHIN is not set. Configure it in $project_root/.env." >&2
  exit 1
fi
dolphin=${DOLPHIN:-}

if [ -n "$platform" ] && [ ! -f "$dolphin" ]; then
  echo "Configured Dolphin executable was not found: $dolphin" >&2
  exit 1
fi

mkdir -p "$test_user_dir/GC" "$test_user_dir/Wii"
prepare_test_cards

if [ -z "$platform" ]; then
  echo "Configured Dolphin test cards in $test_user_dir/GC/GCMM-EX."
  exit 0
fi

if [[ "$dolphin" == *.exe ]]; then
  exec "$dolphin" -u "$(wslpath -w "$test_user_dir")" -e "$(wslpath -w "$dol")"
fi

exec "$dolphin" -u "$test_user_dir" -e "$dol"
