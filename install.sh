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

# --- Install BroTab (forked, from submodule) ---
BROTAB_DIR="$REPO_DIR/third_party/brotab"
BROTAB_MARKER="$HOME/.local/share/workspace-menu/brotab-commit"
BROTAB_COMMIT="$(git -C "$BROTAB_DIR" rev-parse HEAD 2>/dev/null || echo "")"

if [[ -n "$BROTAB_COMMIT" ]] && [[ "$(cat "$BROTAB_MARKER" 2>/dev/null)" != "$BROTAB_COMMIT" ]]; then
  echo "Installing BroTab from fork (commit ${BROTAB_COMMIT:0:7})..."

  # Install/reinstall Python package via pipx
  if command -v pipx &>/dev/null; then
    pipx install --force "$BROTAB_DIR"
    echo "  BroTab Python package installed via pipx"
  else
    echo "  Warning: pipx not found, skipping BroTab Python package install" >&2
  fi

  # Pack and install browser extension into Floorp profile
  FLOORP_PROFILE_DIR=$(find "$HOME/.floorp" -maxdepth 1 -name "*.default*" -type d | head -1)
  if [[ -n "$FLOORP_PROFILE_DIR" ]]; then
    XPI_SRC="$BROTAB_DIR/brotab/extension/firefox"
    XPI_DST="$FLOORP_PROFILE_DIR/extensions/brotab_mediator@example.org.xpi"

    # Read manifest from installed xpi to get icon and manifest, overlay with fork's background.js
    TMPDIR_XPI=$(mktemp -d)
    if [[ -f "$XPI_DST" ]]; then
      unzip -o "$XPI_DST" -d "$TMPDIR_XPI" >/dev/null 2>&1 || true
    fi
    # Remove old signatures (we build unsigned)
    rm -r "$TMPDIR_XPI/META-INF" 2>/dev/null || true
    # Copy our modified background.js and ensure manifest + icon exist
    cp "$XPI_SRC/background.js" "$TMPDIR_XPI/background.js"
    [[ ! -f "$TMPDIR_XPI/manifest.json" ]] && cp "$XPI_SRC/manifest.json" "$TMPDIR_XPI/manifest.json"
    [[ ! -f "$TMPDIR_XPI/brotab-icon-128x128.png" ]] && \
      cp "$BROTAB_DIR/brotab/extension/chrome/brotab-icon-128x128.png" "$TMPDIR_XPI/" 2>/dev/null || true
    (cd "$TMPDIR_XPI" && zip -r "$XPI_DST" . >/dev/null)
    rm -r "$TMPDIR_XPI"
    echo "  BroTab extension installed to $XPI_DST"
  else
    echo "  Warning: no Floorp profile found, skipping extension install" >&2
  fi

  mkdir -p "$(dirname "$BROTAB_MARKER")"
  echo "$BROTAB_COMMIT" > "$BROTAB_MARKER"
else
  echo "BroTab is up to date"
fi

# --- Generate bash constants from C++ enums ---
echo "Generating bash constants..."
"$REPO_DIR/scripts/generate_constants.sh"

# --- Build Docker image ---
BUILD_IMAGE="workspace-build"
if ! docker image inspect "$BUILD_IMAGE" &>/dev/null; then
  echo "  Building Docker image from Dockerfile..."
  docker build -t "$BUILD_IMAGE" "$REPO_DIR"
fi

# --- Build daemon ---
echo "Building workspace-menu daemon..."

# Use a separate build dir to avoid conflicts with dev builds in daemon/build
BUILD_DIR="$REPO_DIR/daemon/build-install"
mkdir -p "$BUILD_DIR"

docker run --rm \
  --user "$(id -u):$(id -g)" \
  -v /etc/passwd:/etc/passwd:ro \
  -v /etc/group:/etc/group:ro \
  -v "$REPO_DIR:$REPO_DIR" \
  -w "$BUILD_DIR" \
  "$BUILD_IMAGE" bash -c "
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

# --- Build and install QML monitor plugin ---
echo "Building workspace monitor QML plugin..."

PLUGIN_BUILD_DIR="$REPO_DIR/plasmoid/plugin/build-install"
mkdir -p "$PLUGIN_BUILD_DIR"

docker run --rm \
  --user "$(id -u):$(id -g)" \
  -v /etc/passwd:/etc/passwd:ro \
  -v /etc/group:/etc/group:ro \
  -v "$REPO_DIR:$REPO_DIR" \
  -w "$PLUGIN_BUILD_DIR" \
  "$BUILD_IMAGE" bash -c "
    cmake '$REPO_DIR/plasmoid/plugin' -DCMAKE_BUILD_TYPE=Release
    make -j8
  "

QT5_QML_DIR=$(qmake -query QT_INSTALL_QML 2>/dev/null || echo "/usr/lib/x86_64-linux-gnu/qt5/qml")
PLUGIN_INSTALL_DIR="$QT5_QML_DIR/org/workspace/monitor"
sudo mkdir -p "$PLUGIN_INSTALL_DIR"
sudo cp "$PLUGIN_BUILD_DIR/libworkspacemonitorplugin.so" "$PLUGIN_INSTALL_DIR/"
sudo cp "$REPO_DIR/plasmoid/plugin/qmldir" "$PLUGIN_INSTALL_DIR/"
echo "  Plugin installed to $PLUGIN_INSTALL_DIR"

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
