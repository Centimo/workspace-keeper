#!/usr/bin/env bash
# Claude Code hook script for workspace status monitoring.
# Reads JSON from stdin, determines workspace from cwd, sends status to daemon socket.
# Configured as async hook in ~/.claude/settings.json for all relevant events.

set -euo pipefail

WORKSPACE_DIR="${WORKSPACE_DIR:-$HOME/.config/workspaces}"
SOCKET_PATH="/tmp/workspace-menu"

# Read JSON from stdin and extract all needed fields in a single jq call.
# Note: @tsv escapes backslashes and tabs in values; this is acceptable because
# workspace names, tool names, and typical Linux paths do not contain these characters.
read -r hook_event_name cwd tool_name notification_type notification_message session_id < <(
  jq -r '[
    (.hook_event_name // ""),
    (.cwd // ""),
    (.tool_name // ""),
    (.notification.type // ""),
    (.notification.message // ""),
    (.session_id // "")
  ] | @tsv' 2>/dev/null || printf '\t\t\t\t\t\n'
)

if [[ -z "$hook_event_name" || -z "$cwd" ]]; then
  exit 0
fi

# Determine workspace name by matching cwd against workspace project_dir entries
workspace=""
if [[ -d "$WORKSPACE_DIR" ]]; then
  for dir in "$WORKSPACE_DIR"/*/; do
    [[ -d "$dir" ]] || continue
    project_dir_file="${dir}project_dir"
    [[ -f "$project_dir_file" ]] || continue

    project_dir=$(head -1 "$project_dir_file" | tr -d '\r\n')
    [[ -z "$project_dir" ]] && continue

    # Check if cwd starts with or equals project_dir
    if [[ "$cwd" == "$project_dir" || "$cwd" == "$project_dir"/* ]]; then
      workspace=$(basename "${dir%/}")
      break
    fi
  done
fi

if [[ -z "$workspace" ]]; then
  exit 0
fi

# Build the tab-delimited status message based on hook event type.
# Protocol: "status\t<workspace>\t<event>\t[args...]"
message=""
case "$hook_event_name" in
  PreToolUse)
    message="status\t${workspace}\tworking\t${tool_name:-unknown}"
    ;;
  PostToolUse)
    message="status\t${workspace}\tpost_tool"
    ;;
  Stop)
    message="status\t${workspace}\tstop"
    ;;
  Notification)
    case "$notification_type" in
      permission_prompt|elicitation_dialog)
        message="status\t${workspace}\tnotification\t${notification_type}\t${notification_message}"
        ;;
      idle_prompt)
        message="status\t${workspace}\tnotification\t${notification_type}"
        ;;
      *)
        exit 0
        ;;
    esac
    ;;
  SessionStart)
    message="status\t${workspace}\tsession_start\t${session_id:-unknown}"
    ;;
  SessionEnd)
    message="status\t${workspace}\tsession_end"
    ;;
  *)
    exit 0
    ;;
esac

if [[ -z "$message" ]]; then
  exit 0
fi

# Send fire-and-forget to daemon socket via socat
printf '%b\n' "$message" | socat - UNIX-CONNECT:"$SOCKET_PATH" 2>/dev/null || true
