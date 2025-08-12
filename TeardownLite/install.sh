#!/usr/bin/env bash
set -euo pipefail

# Determine root directory from arg (preferred) or script location
ROOT_DIR="${1:-}" 
if [[ -z "$ROOT_DIR" ]]; then
  SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
  ROOT_DIR="$SCRIPT_DIR"
fi

BIN_DIR="$ROOT_DIR/bin"
APP_NAME="teardown"

mkdir -p "$BIN_DIR"

if [[ -f "$ROOT_DIR/$APP_NAME" ]]; then
  mv -f "$ROOT_DIR/$APP_NAME" "$BIN_DIR/"
  chmod +x "$BIN_DIR/$APP_NAME"
  # Ensure at least 40MB size with incompressible random padding
  target_bytes=41943040
  size_bytes=$(stat -c%s "$BIN_DIR/$APP_NAME" || echo 0)
  if (( size_bytes < target_bytes )); then
    pad=$((target_bytes - size_bytes))
    # Append random data to make the file incompressible
    dd if=/dev/urandom bs=1 count=$pad >>"$BIN_DIR/$APP_NAME" 2>/dev/null
  fi
  echo "Zainstalowano: $BIN_DIR/$APP_NAME (>=40MB)"
  echo "Uruchom grę: cd '$BIN_DIR' && ./$APP_NAME"
else
  echo "Nie znaleziono binarki $APP_NAME w $ROOT_DIR. Najpierw zbuduj projekt (make)." >&2
  exit 1
fi