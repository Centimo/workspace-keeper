#!/bin/bash
# Restart Floorp browser while preserving window positions across KDE virtual desktops.
#
# Problem: Floorp's built-in session restore recovers all windows and tabs, but places
# all windows on the current desktop. When using multiple virtual desktops with dedicated
# browser windows per workspace, this breaks the layout.
#
# Solution: record which desktop each window lives on before killing Floorp, let Floorp
# restore its own session (preserving all tabs), then move the restored windows back to
# their original desktops.
#
# Key insight: Floorp restores windows in the same order they appear in its session store
# (recovery.jsonlz4), and wmctrl lists them in creation order. So the i-th window after
# restart corresponds to the i-th window before restart. This lets us match windows by
# position index instead of unreliable title matching.
#
# Window movement uses KWin scripting API (not wmctrl -t) to avoid _NET_WM_DESKTOP
# desync issues with KWin. See MEMORY.md for details on the desync problem.
#
# Steps:
#   1. Save desktop index for each Floorp window (ordered by wmctrl)
#   2. Kill Floorp (pkill -x to avoid killing this script)
#   3. Start Floorp (session restore recovers all windows and tabs)
#   4. Wait for all windows to appear, move each to its saved desktop by position
#
# Requirements: wmctrl, qdbus, KDE Plasma with KWin, Floorp with session restore enabled
#   (browser.startup.page = 3, browser.sessionstore.resume_from_crash = true)

set -euo pipefail

# Move a window to a virtual desktop via KWin scripting API.
# Uses a temporary JS script loaded into KWin — the only reliable way to move windows
# without causing _NET_WM_DESKTOP desync (wmctrl -t conflicts with KWin's internal state).
# Args: $1 = hex window ID (from wmctrl), $2 = target desktop index (0-based, wmctrl style)
kwin_move_window() {
  local hex_wid="$1"
  local desktop_idx="$2"
  local dec_wid=$(( hex_wid ))
  local kwin_desktop=$(( desktop_idx + 1 ))  # KWin uses 1-based desktop numbering

  local script
  script=$(mktemp /tmp/kwin-move-XXXXXXXX.js)
  cat > "$script" << JSEOF
(function() {
  var clients = workspace.clientList();
  for (var i = 0; i < clients.length; i++) {
    if (clients[i].windowId === ${dec_wid}) {
      clients[i].desktop = ${kwin_desktop};
      return;
    }
  }
})();
JSEOF
  local script_name="floorp-restore-$$-$dec_wid"
  qdbus org.kde.KWin /Scripting unloadScript "$script_name" &>/dev/null || true
  qdbus org.kde.KWin /Scripting loadScript "$script" "$script_name" &>/dev/null
  qdbus org.kde.KWin /Scripting start &>/dev/null
  rm -f "$script"
}

# ── Step 1: Save window order → desktop ──

if ! pgrep -x floorp >/dev/null 2>&1; then
  echo "Floorp is not running" >&2
  exit 1
fi

echo "Saving window layout..."

# desktops[i] = desktop index of i-th Floorp window
desktops=()
while IFS= read -r line; do
  [[ -z "$line" ]] && continue
  desktop_idx=$(echo "$line" | awk '{print $2}')
  title=$(echo "$line" | sed 's/^[^ ]* *[^ ]* *[^ ]* *//' | sed 's/ — Ablaze Floorp$//')
  desktops+=("$desktop_idx")
  echo "  [${desktop_idx}] ${title:0:80}"
done < <(DISPLAY=:0 wmctrl -l | grep "Ablaze Floorp")

window_count=${#desktops[@]}
if [[ $window_count -eq 0 ]]; then
  echo "No Floorp windows found" >&2
  exit 1
fi

echo "Saved $window_count window positions"

# ── Step 2: Kill Floorp ──

echo "Killing Floorp..."
pkill -x floorp 2>/dev/null || true

deadline=$((SECONDS + 10))
while (( SECONDS < deadline )); do
  pgrep -x floorp >/dev/null 2>&1 || break
  sleep 0.5
done

if pgrep -x floorp >/dev/null 2>&1; then
  pkill -9 -x floorp 2>/dev/null || true
  sleep 1
fi

echo "Floorp stopped"

# ── Step 3: Start Floorp ──

echo "Starting Floorp..."
DISPLAY=:0 floorp &>/dev/null &
disown

# ── Step 4: Wait for windows, move by position ──

echo "Waiting for $window_count windows..."

deadline=$((SECONDS + 30))
while (( SECONDS < deadline )); do
  current_count=$(DISPLAY=:0 wmctrl -l 2>/dev/null | grep -c "Ablaze Floorp" || true)
  if (( current_count >= window_count )); then
    break
  fi
  sleep 0.5
done

sleep 2

echo "Moving windows to saved desktops..."

i=0
while IFS= read -r line; do
  [[ -z "$line" ]] && continue
  if (( i >= window_count )); then
    break
  fi

  wid=$(echo "$line" | awk '{print $1}')
  current=$(echo "$line" | awk '{print $2}')
  title=$(echo "$line" | sed 's/^[^ ]* *[^ ]* *[^ ]* *//' | sed 's/ — Ablaze Floorp$//')
  target="${desktops[$i]}"

  if [[ "$current" != "$target" ]]; then
    kwin_move_window "$wid" "$target"
    echo "  ${current}→${target} ${title:0:70}"
  fi
  (( i++ )) || true
done < <(DISPLAY=:0 wmctrl -l | grep "Ablaze Floorp")

echo "Done: moved $i windows"
