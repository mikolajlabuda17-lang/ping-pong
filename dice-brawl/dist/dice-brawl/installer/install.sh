#!/usr/bin/env bash
set -euo pipefail

APP_NAME="Dice Brawl"
APP_ID="dice-brawl"
INSTALL_BIN_DIR="$HOME/.local/games"
INSTALL_APP_DIR="$HOME/.local/share/applications"
INSTALL_ICON_DIR="$HOME/.local/share/icons/hicolor/256x256/apps"
DATA_DIR="$HOME/.local/share/dice-brawl"
WEB_DIR="$DATA_DIR/web"

mkdir -p "$INSTALL_BIN_DIR" "$INSTALL_APP_DIR" "$INSTALL_ICON_DIR" "$WEB_DIR"

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
PKG_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Copy binary
install -m 0755 "$PKG_ROOT/bin/dice-brawl" "$INSTALL_BIN_DIR/dice-brawl"

# Icon (optional placeholder)
if [[ -f "$PKG_ROOT/icon.png" ]]; then
  install -m 0644 "$PKG_ROOT/icon.png" "$INSTALL_ICON_DIR/${APP_ID}.png"
fi

# Web content
if [[ -d "$PKG_ROOT/web" ]]; then
  rsync -a --delete "$PKG_ROOT/web/" "$WEB_DIR/" 2>/dev/null || cp -a "$PKG_ROOT/web/." "$WEB_DIR/"
fi

# Desktop entry
cat > "$INSTALL_APP_DIR/${APP_ID}.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=${APP_NAME}
Comment=Lekka gra: walka kostek i coiny
Exec=${INSTALL_BIN_DIR}/dice-brawl
Icon=${APP_ID}
Terminal=false
Categories=Game;
EOF

update-desktop-database ~/.local/share/applications 2>/dev/null || true

echo "Zainstalowano: ${APP_NAME}"
echo "Uruchom z menu aplikacji lub poleceniem: ${INSTALL_BIN_DIR}/dice-brawl"