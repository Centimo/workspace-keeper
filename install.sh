#!/bin/bash
# Install workspace-keeper: symlink configs, build daemon, configure KDE shortcuts.
# Run from the repo root: ./install.sh

set -euo pipefail

REPO_DIR="$(cd "$(dirname "$0")" && pwd)"

link() {
  local src="$REPO_DIR/$1"
  local dst="$2"
  mkdir -p "$(dirname "$dst")"
  if [[ -L "$dst" ]]; then
    rm "$dst"
  elif [[ -e "$dst" ]]; then
    echo "Backup: $dst -> ${dst}.bak"
    mv "$dst" "${dst}.bak"
  fi
  ln -s "$src" "$dst"
  echo "  $dst -> $src"
}

# Generate a file from a template by replacing __HOME__ with $HOME
generate() {
  local src="$REPO_DIR/$1"
  local dst="$2"
  mkdir -p "$(dirname "$dst")"
  if [[ -L "$dst" ]]; then
    rm "$dst"
  elif [[ -e "$dst" ]]; then
    echo "Backup: $dst -> ${dst}.bak"
    mv "$dst" "${dst}.bak"
  fi
  sed "s|__HOME__|$HOME|g" "$src" > "$dst"
  echo "  $dst (generated from template)"
}

echo "Installing workspace-keeper..."

# --- Symlink configs ---
link config/wezterm/wezterm.lua          "$HOME/.wezterm.lua"
link config/tmux/tmux.conf               "$HOME/.tmux.conf"
link bin/workspace                       "$HOME/.local/bin/workspace"
link config/kde/kwinrulesrc              "$HOME/.config/kwinrulesrc"
link config/kde/workspace-menu.desktop   "$HOME/.local/share/applications/workspace-menu.desktop"
link config/autostart/org.wezfurlong.wezterm.desktop "$HOME/.config/autostart/org.wezfurlong.wezterm.desktop"

# --- Generated files (need __HOME__ substitution) ---
generate config/floorp/brotab_mediator.json "$HOME/.floorp/native-messaging-hosts/brotab_mediator.json"
generate config/kde/workspace-menu-daemon.desktop "$HOME/.config/autostart/workspace-menu-daemon.desktop"

chmod +x "$HOME/.local/bin/workspace"

# --- Build daemon ---
echo "Building workspace-menu daemon..."
mkdir -p "$REPO_DIR/daemon/build"

DAEMON_IMAGE="workspace-menu-build"
if ! docker image inspect "$DAEMON_IMAGE" &>/dev/null; then
  echo "  Building Docker image from daemon/Dockerfile..."
  docker build -t "$DAEMON_IMAGE" "$REPO_DIR/daemon"
fi

docker run --rm -v "$REPO_DIR:/src" -w /src/daemon/build \
  "$DAEMON_IMAGE" bash -c "
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make -j8
    chown $(id -u):$(id -g) workspace-menu
  "

mkdir -p "$HOME/.local/bin"
cp "$REPO_DIR/daemon/build/workspace-menu" "$HOME/.local/bin/workspace-menu"
chmod +x "$HOME/.local/bin/workspace-menu"
echo "  Daemon installed to ~/.local/bin/workspace-menu"

# --- Rename default desktop ---
DEFAULT_DESKTOP_ID=$(qdbus --literal org.kde.KWin /VirtualDesktopManager desktops 2>/dev/null \
  | grep -oP '"[a-f0-9-]+", "Desktop 1"' | grep -oP '"[a-f0-9-]+"' | tr -d '"')
if [[ -n "$DEFAULT_DESKTOP_ID" ]]; then
  qdbus org.kde.KWin /VirtualDesktopManager setDesktopName "$DEFAULT_DESKTOP_ID" "Base"
  echo "  Renamed 'Desktop 1' -> 'Base'"
fi

# --- KDE global shortcuts ---
echo "Configuring KDE shortcuts..."

SHORTCUTS_FILE="$HOME/.config/kglobalshortcutsrc"

# Detect KDE version (Plasma 5 vs 6)
if command -v kwriteconfig6 &>/dev/null; then
  KWRITECONFIG=kwriteconfig6
  KQUITAPP=kquitapp6
  KGLOBALACCEL=kglobalaccel6
elif command -v kwriteconfig5 &>/dev/null; then
  KWRITECONFIG=kwriteconfig5
  KQUITAPP=kquitapp5
  KGLOBALACCEL=kglobalaccel5
else
  echo "  Warning: kwriteconfig not found, skipping shortcut configuration" >&2
  echo "Done."
  exit 0
fi

# Reassign Walk Through Windows: Alt+Tab -> Ctrl+Tab
"$KWRITECONFIG" --file "$SHORTCUTS_FILE" --group kwin \
  --key "Walk Through Windows" "Ctrl+Tab,Alt+Tab,Walk Through Windows"
"$KWRITECONFIG" --file "$SHORTCUTS_FILE" --group kwin \
  --key "Walk Through Windows (Reverse)" "Ctrl+Shift+Backtab,Alt+Shift+Backtab,Walk Through Windows (Reverse)"

# Clear old .desktop-based shortcut (now registered via KGlobalAccel in the daemon)
"$KWRITECONFIG" --file "$SHORTCUTS_FILE" --group workspace-menu.desktop \
  --key "_launch" "none,none,Workspace Menu"

# Reload kglobalaccel to apply changes
"$KQUITAPP" kglobalaccel 2>/dev/null || true
sleep 1
"$KGLOBALACCEL" 2>/dev/null &
disown
echo "  KDE shortcuts configured (Ctrl+Tab -> window switching, Alt+Tab -> daemon via KGlobalAccel)"

echo "Done."
