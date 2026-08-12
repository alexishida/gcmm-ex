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
test_user_dir=${DOLPHIN_USER_DIR:-"$project_root/tests/dolphin/user"}
memorycards_dir=${GCMM_TEST_MEMORYCARDS_DIR:-"$project_root/memorycards"}

usage() {
  cat >&2 <<'EOF'
Usage: run-dolphin.sh [--gc|--wii|--setup] [--reset-card|--import FILE]

  --gc, --wii     Launch the built DOL for that platform. Defaults to --gc.
  --setup         Configure the test cards and exit without launching.
  --reset-card    Overwrite Dolphin's test card with memorycards/backup.USA.raw.
  --import FILE   Overwrite Dolphin's test card with FILE.

Without a platform, --reset-card and --import configure and exit. Resetting
discards whatever the emulated card currently holds.
EOF
}

platform=
platform_given=0
reset_card=0
import_source=

# No arguments keeps the historical behaviour: build a GC session and launch.
if [ $# -eq 0 ]; then
  platform_given=1
fi

while [ $# -gt 0 ]; do
  case "$1" in
    --gc|--wii)
      platform=$1
      platform_given=1
      ;;
    --setup)
      platform_given=0
      ;;
    --reset-card)
      reset_card=1
      ;;
    --import)
      shift
      if [ $# -eq 0 ]; then
        echo "--import needs a file path." >&2
        exit 64
      fi
      import_source=$1
      reset_card=1
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
  shift
done

if [ "$platform_given" -eq 1 ]; then
  platform=${platform:---gc}
else
  platform=
fi

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
  local raw_source=${import_source:-"$memorycards_dir/backup.USA.raw"}
  local card_b_raw="$test_user_dir/GC/GCMM-EX/Test Card B.USA.raw"
  local config_card_b_raw=$card_b_raw

  mkdir -p "$(dirname -- "$config")" "$(dirname -- "$card_b_raw")"

  if [ "$reset_card" -eq 1 ] || [ ! -e "$card_b_raw" ]; then
    if [ ! -f "$raw_source" ]; then
      echo "Missing Dolphin test card: $raw_source" >&2
      exit 1
    fi
    # A card image is a whole number of 8 KiB blocks. A file that is not one
    # would leave Dolphin with a card it cannot open.
    local source_bytes
    source_bytes=$(wc -c < "$raw_source")
    if [ "$source_bytes" -eq 0 ] || [ $((source_bytes % 8192)) -ne 0 ]; then
      echo "Not a RAW card image ($source_bytes bytes, expected a multiple of 8192): $raw_source" >&2
      exit 1
    fi
    if [ "$reset_card" -eq 1 ] && [ -e "$card_b_raw" ]; then
      echo "Discarding current emulated card contents."
    fi
    cp -- "$raw_source" "$card_b_raw"
    echo "Imported $raw_source into Slot B ($((source_bytes / 1024 / 1024)) MB, $((source_bytes / 8192)) blocks)."
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

dol=
case "$platform" in
  --gc)
    dol="$project_root/releases/gcmm_ex_GC.dol"
    ;;
  --wii)
    dol="$project_root/releases/gcmm_ex_WII.dol"
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
