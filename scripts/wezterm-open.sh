#!/bin/bash
# Open WezTerm in the current KDE desktop's workspace.
# This ensures the mux workspace name matches the KDE desktop name,
# which is used by the daemon to track tabs per workspace.

workspace=$(wmctrl -d 2>/dev/null | awk '/\*/ {print $NF}')
if [[ -z "$workspace" ]]; then
  workspace="default"
fi

exec wezterm start --always-new-process --domain unix --workspace "$workspace" --cwd .
