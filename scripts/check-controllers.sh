#!/usr/bin/env bash
set -euo pipefail
for unit in awccd.service fancontrol.service i8kmon.service tuxedo-control-center.service; do
  state="$(systemctl show --property=ActiveState --value "$unit")"
  if [[ "$state" != inactive && "$state" != failed ]]; then
    printf '%s is %s. Stop it explicitly before starting alienwarectl.\n' "$unit" "$state" >&2
    exit 1
  fi
done
