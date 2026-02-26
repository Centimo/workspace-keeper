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

# --- Install host runtime dependencies ---
mapfile -t RUNTIME_DEPS < "$REPO_DIR/daemon/runtime-deps.txt"

missing=()
for pkg in "${RUNTIME_DEPS[@]}"; do
  [[ -z "$pkg" ]] && continue
  if ! dpkg -s "$pkg" &>/dev/null; then
    missing+=("$pkg")
  fi
done

if [[ ${#missing[@]} -gt 0 ]]; then
  echo "Installing missing runtime dependencies: ${missing[*]}"
  sudo apt-get install -y "${missing[@]}"
fi

# --- Symlink configs ---
link config/wezterm/wezterm.lua          "$HOME/.wezterm.lua"
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

DAEMON_IMAGE="workspace-menu-build"
if ! docker image inspect "$DAEMON_IMAGE" &>/dev/null; then
  echo "  Building Docker image from daemon/Dockerfile..."
  docker build -t "$DAEMON_IMAGE" "$REPO_DIR/daemon"
fi

# Use a separate build dir to avoid conflicts with dev builds in daemon/build
BUILD_DIR="$REPO_DIR/daemon/build-install"
mkdir -p "$BUILD_DIR"

docker run --rm \
  --user "$(id -u):$(id -g)" \
  -v /etc/passwd:/etc/passwd:ro \
  -v /etc/group:/etc/group:ro \
  -v "$REPO_DIR:$REPO_DIR" \
  -w "$BUILD_DIR" \
  "$DAEMON_IMAGE" bash -c "
    cmake '$REPO_DIR/daemon' -DCMAKE_BUILD_TYPE=Release
    make -j8
  "

mkdir -p "$HOME/.local/bin"

# Stop daemon before overwriting binary (otherwise cp fails with "Text file busy")
if pkill -x workspace-menu 2>/dev/null; then
  echo "  Stopped old daemon"
  sleep 0.5
fi

cp "$BUILD_DIR/workspace-menu" "$HOME/.local/bin/workspace-menu"
chmod +x "$HOME/.local/bin/workspace-menu"
echo "  Daemon installed to ~/.local/bin/workspace-menu"
DISPLAY="${DISPLAY:-:0}" nohup "$HOME/.local/bin/workspace-menu" > /dev/null 2>&1 &
disown
echo "  Daemon started (PID $!)"

# --- Install plasmoid ---
echo "Installing Claude Monitor plasmoid..."
PLASMOID_DIR="$REPO_DIR/plasmoid/org.workspace.claude-monitor"
PLASMOID_ID="org.workspace.claude-monitor"
if kpackagetool5 --type Plasma/Applet --show "$PLASMOID_ID" &>/dev/null; then
  kpackagetool5 --type Plasma/Applet --upgrade "$PLASMOID_DIR"
  echo "  Plasmoid upgraded"
else
  kpackagetool5 --type Plasma/Applet --install "$PLASMOID_DIR"
  echo "  Plasmoid installed"
fi

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
